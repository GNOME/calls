/*
 * Copyright (C) 2021 Purism SPC
 *
 * This file is part of Calls.
 *
 * Calls is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Calls is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Calls.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define G_LOG_DOMAIN "CallsSipMediaPipeline"

#include "calls-sip-media-pipeline.h"

#include <gst/gst.h>
#include <gio/gio.h>

/**
 * SECTION:sip-media-pipeline
 * @short_description:
 * @Title:
 *
 * #CallsSipMediaPipeline is responsible for building Gstreamer pipelines.
 * Usually a sender and receiver pipeline is employed.
 *
 * The sender pipeline records audio and uses RTP to send it out over the network
 * to the specified host.
 * The receiver pipeline receives RTP from the network and plays the audio
 * on the system.
 *
 * Both pipelines are using RTCP.
 */

enum {
  PROP_0,
  PROP_CODEC,
  PROP_REMOTE,
  PROP_LPORT_RTP,
  PROP_RPORT_RTP,
  PROP_LPORT_RTCP,
  PROP_RPORT_RTCP,
  PROP_DEBUG,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

struct _CallsSipMediaPipeline {
  GObject parent;

  MediaCodecInfo *codec;
  gboolean debug;
  /* Connection details */
  char *remote;

  gint rport_rtp;
  gint lport_rtp;

  gint rport_rtcp;
  gint lport_rtcp;

  gboolean is_running;

  /* Gstreamer Elements (sending) */
  GstElement *send_pipeline;
  GstElement *audiosrc;
  GstElement *send_rtpbin;
  GstElement *rtp_sink; /* UDP out */
  GstElement *payloader;
  GstElement *encoder;
  GstElement *rtcp_send_sink;
  GstElement *rtcp_send_src;
  /* Gstreamer elements (receiving) */
  GstElement *recv_pipeline;
  GstElement *audiosink;
  GstElement *recv_rtpbin;
  GstElement *rtp_src; /* UDP in */
  GstElement *depayloader;
  GstElement *decoder;
  GstElement *rtcp_recv_sink;
  GstElement *rtcp_recv_src;

  /* Gstreamer busses */
  GstBus *bus_send;
  GstBus *bus_recv;
  guint bus_watch_send;
  guint bus_watch_recv;
};


static void initable_iface_init (GInitableIface *iface);


G_DEFINE_TYPE_WITH_CODE (CallsSipMediaPipeline, calls_sip_media_pipeline, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init));

/* rtpbin adds a pad once the payload is verified */
static void
on_pad_added (GstElement *rtpbin,
              GstPad     *srcpad,
              GstElement *depayloader)
{
  GstPad *sinkpad;
  /* there might still be another rtp src bin linked to the depayloader */
  //GstPad *other_srcpad;

  g_debug ("pad added: %s", GST_PAD_NAME (srcpad));

  sinkpad = gst_element_get_static_pad (depayloader, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_error ("Failed to link rtpbin to depayloader");

  gst_object_unref (sinkpad);
}


static gboolean
on_bus_message (GstBus     *bus,
                GstMessage *message,
                gpointer    data)
{
  CallsSipMediaPipeline *pipeline = CALLS_SIP_MEDIA_PIPELINE (data);

  switch (GST_MESSAGE_TYPE (message)) {
  case GST_MESSAGE_ERROR:
    {
      g_autoptr (GError) error = NULL;
      g_autofree char *msg = NULL;

      gst_message_parse_error (message, &error, &msg);
      g_error ("Error: %s", msg);
      break;
    }

  case GST_MESSAGE_WARNING:
    {
      g_autoptr (GError) error = NULL;
      g_autofree char *msg = NULL;

      gst_message_parse_warning (message, &error, &msg);
      g_warning ("Warning: %s", msg);
      break;
    }

  case GST_MESSAGE_EOS:
    g_debug ("Received end of stream");
    calls_sip_media_pipeline_stop (pipeline);
    break;

  case GST_MESSAGE_STATE_CHANGED:
    {
      GstState oldstate;
      GstState newstate;

      gst_message_parse_state_changed (message, &oldstate, &newstate, NULL);
      g_debug ("Element %s has changed state from %s to %s",
               GST_OBJECT_NAME (message->src),
               gst_element_state_get_name (oldstate),
               gst_element_state_get_name (newstate));
      break;
    }

  default:
    if (pipeline->debug)
      g_debug ("Got unhandled %s message", GST_MESSAGE_TYPE_NAME (message));
    break;
  }

  /* keep watching for messages on the bus */
  return TRUE;
}


static void
get_property (GObject      *object,
              guint         property_id,
              GValue       *value,
              GParamSpec   *pspec)
{
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (object);

  switch (property_id) {
  case PROP_CODEC:
    g_value_set_pointer (value, self->codec);
    break;

  case PROP_REMOTE:
    g_value_set_string (value, self->remote);
    break;

  case PROP_LPORT_RTP:
    g_value_set_uint (value, self->lport_rtp);
    break;

  case PROP_LPORT_RTCP:
    g_value_set_uint (value, self->lport_rtcp);
    break;

  case PROP_RPORT_RTP:
    g_value_set_uint (value, self->rport_rtp);
    break;

  case PROP_RPORT_RTCP:
    g_value_set_uint (value, self->rport_rtcp);
    break;

  case PROP_DEBUG:
    g_value_set_boolean (value, self->debug);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (object);

  switch (property_id) {
  case PROP_CODEC:
    self->codec = g_value_get_pointer (value);
    break;

  case PROP_REMOTE:
    g_free (self->remote);
    self->remote = g_value_dup_string (value);
    break;

  case PROP_LPORT_RTP:
    self->lport_rtp = g_value_get_uint (value);
    break;

  case PROP_LPORT_RTCP:
    self->lport_rtcp = g_value_get_uint (value);
    break;

  case PROP_RPORT_RTP:
    self->rport_rtp = g_value_get_uint (value);
    break;

  case PROP_RPORT_RTCP:
    self->rport_rtcp = g_value_get_uint (value);
    break;

  case PROP_DEBUG:
    self->debug = g_value_get_boolean (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
finalize (GObject *object)
{
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (object);

  calls_sip_media_pipeline_stop (self);

  gst_object_unref (self->send_pipeline);
  gst_object_unref (self->recv_pipeline);
  gst_bus_remove_watch (self->bus_send);
  gst_object_unref (self->bus_send);
  gst_bus_remove_watch (self->bus_recv);
  gst_object_unref (self->bus_recv);

  g_free (self->remote);

  G_OBJECT_CLASS (calls_sip_media_pipeline_parent_class)->finalize (object);
}


static void
calls_sip_media_pipeline_class_init (CallsSipMediaPipelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->get_property = get_property;
  object_class->finalize = finalize;

  /* Maybe we want to turn Codec into a GObject later */
  props[PROP_CODEC] = g_param_spec_pointer ("codec",
                                            "Codec",
                                            "Media codec",
                                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  props[PROP_REMOTE] = g_param_spec_string ("remote",
                                            "Remote",
                                            "Remote host",
                                            NULL,
                                            G_PARAM_READWRITE);

  props[PROP_LPORT_RTP] = g_param_spec_uint ("lport-rtp",
                                             "lport-rtp",
                                             "local rtp port",
                                             1025, 65535, 5002,
                                             G_PARAM_READWRITE);

  props[PROP_LPORT_RTCP] = g_param_spec_uint ("lport-rtcp",
                                              "lport-rtcp",
                                              "local rtcp port",
                                              1025, 65535, 5003,
                                              G_PARAM_READWRITE);

  props[PROP_RPORT_RTP] = g_param_spec_uint ("rport-rtp",
                                             "rport-rtp",
                                             "remote rtp port",
                                             1025, 65535, 5002,
                                             G_PARAM_READWRITE);

  props[PROP_RPORT_RTCP] = g_param_spec_uint ("rport-rtcp",
                                              "rport-rtcp",
                                              "remote rtcp port",
                                              1025, 65535, 5003,
                                              G_PARAM_READWRITE);

  props[PROP_DEBUG] = g_param_spec_boolean ("debug",
                                            "Debug",
                                            "Enable debugging information",
                                            FALSE,
                                            G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_sip_media_pipeline_init (CallsSipMediaPipeline *self)
{
}


static gboolean
initable_init (GInitable    *initable,
               GCancellable *cancelable,
               GError      **error)
{
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (initable);
  GstCaps *caps;
  g_autofree char *caps_string = NULL;
  GstPad *srcpad, *sinkpad;
  GstStructure *props = NULL;
  const char *env_var;

  env_var = g_getenv ("CALLS_AUDIOSINK");
  if (env_var) {
    self->audiosink = gst_element_factory_make (env_var, "sink");
  } else {
    /* could also use autoaudiosink instead of pulsesink */
    self->audiosink = gst_element_factory_make ("pulsesink", "sink");

    /* enable echo cancellation and set buffer size to 40ms */
    props = gst_structure_new ("props",
                               "media.role", G_TYPE_STRING, "phone",
                               "filter.want", G_TYPE_STRING, "echo-cancel",
                               NULL);

    g_object_set (self->audiosink,
                  "buffer-time", (gint64) 40000,
                  "stream-properties", props,
                  NULL);

    g_object_set (self->audiosrc,
                  "buffer-time", (gint64) 40000,
                  "stream-properties", props,
                  NULL);

    gst_structure_free (props);
  }

  env_var = g_getenv ("CALLS_AUDIOSRC");
  if (env_var)
    self->audiosrc = gst_element_factory_make (env_var, "source");
  else
    self->audiosrc = gst_element_factory_make ("pulsesrc", "source");

  if (!self->audiosrc || !self->audiosink) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not create audiosink or audiosrc");
    return FALSE;
  }

  /* maybe we need to also explicitly add audioconvert and audioresample elements */
  self->send_rtpbin = gst_element_factory_make ("rtpbin", "send-rtpbin");
  self->recv_rtpbin = gst_element_factory_make ("rtpbin", "recv-rtpbin");
  if (!self->send_rtpbin || !self->recv_rtpbin) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not create send/receive rtpbin");
    return FALSE;
  }

  self->decoder = gst_element_factory_make (self->codec->gst_decoder_name, "decoder");
  if (!self->decoder) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not create decoder %s", self->codec->gst_decoder_name);
    return FALSE;
  }

  self->depayloader = gst_element_factory_make (self->codec->gst_depayloader_name, "depayloader");
  if (!self->depayloader) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not create depayloader %s", self->codec->gst_depayloader_name);
    return FALSE;
  }

  self->encoder = gst_element_factory_make (self->codec->gst_encoder_name, "encoder");
  if (!self->encoder) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not create encoder %s", self->codec->gst_encoder_name);
    return FALSE;
  }

  self->payloader = gst_element_factory_make (self->codec->gst_payloader_name, "payloader");
  if (!self->encoder) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not create payloader %s", self->codec->gst_payloader_name);
    return FALSE;
  }

  self->rtp_src = gst_element_factory_make ("udpsrc", "rtp-udp-src");
  self->rtp_sink = gst_element_factory_make ("udpsink", "rtp-udp-sink");
  self->rtcp_recv_sink = gst_element_factory_make ("udpsink", "rtcp-udp-recv-sink");
  self->rtcp_recv_src = gst_element_factory_make ("udpsrc", "rtcp-udp-recv-src");
  self->rtcp_send_sink = gst_element_factory_make ("udpsink", "rtcp-udp-send-sink");
  self->rtcp_send_src = gst_element_factory_make ("udpsrc", "rtcp-udp-send-src");

  if (!self->rtp_src || !self->rtp_sink ||
      !self->rtcp_recv_sink || !self->rtcp_recv_src ||
      !self->rtcp_send_sink || !self->rtcp_send_src) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not create udp sinks or sources");
    return FALSE;
  }

  self->send_pipeline = gst_pipeline_new ("rtp-send-pipeline");
  self->recv_pipeline = gst_pipeline_new ("rtp-recv-pipeline");

  if (!self->send_pipeline || !self->recv_pipeline) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not create send or receiver pipeline");
    return FALSE;
  }

  gst_object_ref_sink (self->send_pipeline);
  gst_object_ref_sink (self->recv_pipeline);

/* get the busses and establish watches */
  self->bus_send = gst_pipeline_get_bus (GST_PIPELINE (self->send_pipeline));
  self->bus_recv = gst_pipeline_get_bus (GST_PIPELINE (self->recv_pipeline));
  self->bus_watch_send = gst_bus_add_watch (self->bus_send, on_bus_message, self);
  self->bus_watch_recv = gst_bus_add_watch (self->bus_recv, on_bus_message, self);

  gst_bin_add_many (GST_BIN (self->recv_pipeline), self->depayloader, self->decoder,
                    self->audiosink, NULL);
  gst_bin_add_many (GST_BIN (self->send_pipeline), self->payloader, self->encoder,
                    self->audiosrc, NULL);

  if (!gst_element_link_many (self->depayloader, self->decoder, self->audiosink, NULL)) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to link depayloader decoder and audiosink");
    return FALSE;
  }

  if (!gst_element_link_many (self->audiosrc, self->encoder, self->payloader, NULL)) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to link audiosrc encoder and payloader");
    return FALSE;
  }

  gst_bin_add (GST_BIN (self->send_pipeline), self->send_rtpbin);
  gst_bin_add (GST_BIN (self->recv_pipeline), self->recv_rtpbin);

  gst_bin_add_many (GST_BIN (self->send_pipeline), self->rtp_sink,
                    self->rtcp_send_src, self->rtcp_send_sink, NULL);
  gst_bin_add_many (GST_BIN (self->recv_pipeline), self->rtp_src,
                    self->rtcp_recv_src, self->rtcp_recv_sink, NULL);

  caps_string = media_codec_get_gst_capabilities (self->codec);
  g_debug ("Capabilities:\n%s", caps_string);

  caps = gst_caps_from_string (caps_string);

  /* set udp sinks and sources for RTP and RTCP */
  g_object_set (self->rtp_src,
                "caps", caps,
                NULL);

  g_object_set (self->rtcp_recv_sink,
                "async", FALSE,
                "sync", FALSE,
                NULL);

  g_object_set (self->rtcp_send_sink,
                "async", FALSE,
                "sync", FALSE,
                NULL);

  /* bind to properties of udp sinks and sources */
  /* Receiver side */
  if (self->remote == NULL)
    self->remote = g_strdup ("localhost");

  g_object_bind_property (self, "lport-rtp",
                          self->rtp_src, "port",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "lport-rtcp",
                          self->rtcp_recv_src, "port",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "rport-rtcp",
                          self->rtcp_recv_sink, "port",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "remote",
                          self->rtcp_recv_sink, "host",
                          G_BINDING_BIDIRECTIONAL);

  /* Sender side */
  g_object_bind_property (self, "rport-rtp",
                          self->rtp_sink, "port",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "remote",
                          self->rtp_sink, "host",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "lport-rtcp",
                          self->rtcp_send_src, "port",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "rport-rtcp",
                          self->rtcp_send_sink, "port",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "remote",
                          self->rtcp_send_sink, "host",
                          G_BINDING_BIDIRECTIONAL);

  /* Link pads */
  /* in/receive direction */
  /* request and link the pads */
  srcpad = gst_element_get_static_pad (self->rtp_src, "src");
  sinkpad = gst_element_get_request_pad (self->recv_rtpbin, "recv_rtp_sink_0");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to link rtpsrc to rtpbin");
    return FALSE;
  }
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  srcpad = gst_element_get_static_pad (self->rtcp_recv_src, "src");
  sinkpad = gst_element_get_request_pad (self->recv_rtpbin, "recv_rtcp_sink_0");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to link rtcpsrc to rtpbin");
    return FALSE;
  }
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  srcpad = gst_element_get_request_pad (self->recv_rtpbin, "send_rtcp_src_0");
  sinkpad = gst_element_get_static_pad (self->rtcp_recv_sink, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to link rtpbin to rtcpsink");
    return FALSE;
  }
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  /* need to link RTP pad to the depayloader */
  g_signal_connect (self->recv_rtpbin, "pad-added", G_CALLBACK (on_pad_added), self->depayloader);


  /* out/send direction */
  /* link payloader src to RTP sink pad */
  sinkpad = gst_element_get_request_pad (self->send_rtpbin, "send_rtp_sink_0");
  srcpad = gst_element_get_static_pad (self->payloader, "src");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to link payloader to rtpbin");
    return FALSE;
  }
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  /* link RTP srcpad to udpsink */
  srcpad = gst_element_get_static_pad (self->send_rtpbin, "send_rtp_src_0");
  sinkpad = gst_element_get_static_pad (self->rtp_sink, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to link rtpbin to rtpsink");
    return FALSE;
  }
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  /* RTCP srcpad to udpsink */
  srcpad = gst_element_get_request_pad (self->send_rtpbin, "send_rtcp_src_0");
  sinkpad = gst_element_get_static_pad (self->rtcp_send_sink, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to link rtpbin to rtcpsink");
    return FALSE;
  }
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  /* receive RTCP */
  srcpad = gst_element_get_static_pad (self->rtcp_send_src, "src");
  sinkpad = gst_element_get_request_pad (self->send_rtpbin, "recv_rtcp_sink_0");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to link rtcpsrc to rtpbin");
    return FALSE;
  }
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  return TRUE;
}


static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = initable_init;
}


CallsSipMediaPipeline*
calls_sip_media_pipeline_new (MediaCodecInfo *codec)
{
  CallsSipMediaPipeline *pipeline;
  g_autoptr (GError) error = NULL;

  pipeline = g_initable_new (CALLS_TYPE_SIP_MEDIA_PIPELINE, NULL, &error,
                             "codec", codec,
                             NULL);
  if (pipeline == NULL)
    g_error ("Media pipeline could not be initialized: %s", error->message);

  return pipeline;
}


static void
diagnose_used_ports_in_socket (GSocket *socket)
{
  g_autoptr (GSocketAddress) local_addr = NULL;
  g_autoptr (GSocketAddress) remote_addr = NULL;
  guint16 local_port;
  guint16 remote_port;

  local_addr = g_socket_get_local_address (socket, NULL);
  remote_addr = g_socket_get_remote_address (socket, NULL);
  if (!local_addr) {
    g_warning ("Could not get local address of socket");
    return;
  }
  g_assert (G_IS_INET_SOCKET_ADDRESS (local_addr));

  local_port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (local_addr));
  g_debug ("Using local port %d", local_port);

  if (!remote_addr) {
    g_warning ("Could not get remote address of socket");
    return;
  }
  g_assert (G_IS_INET_SOCKET_ADDRESS (remote_addr));

  remote_port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (remote_addr));
  g_debug ("Using remote port %d", remote_port);

}


static void
diagnose_ports_in_use (CallsSipMediaPipeline *self)
{
  GSocket *socket_in;
  GSocket *socket_out;
  gboolean same_socket = FALSE;

  g_assert (CALLS_IS_SIP_MEDIA_PIPELINE (self));
  g_assert (self->is_running);

  g_object_get (self->rtp_src, "used-socket", &socket_in, NULL);
  g_object_get (self->rtp_sink, "used-socket", &socket_out, NULL);

  if (socket_in == NULL || socket_out == NULL) {
    g_warning ("Could not get used socket");
    return;
  }
  same_socket = socket_in == socket_out;

  if (same_socket) {
    g_debug ("Diagnosing bidirectional socket...");
    diagnose_used_ports_in_socket (socket_in);
  }
  else {
    g_debug ("Diagnosing server socket...");
    diagnose_used_ports_in_socket (socket_in);
    g_debug ("Diagnosing client socket...");
    diagnose_used_ports_in_socket (socket_out);
  }
}


void
calls_sip_media_pipeline_start (CallsSipMediaPipeline *self)
{
  GSocket *socket;
  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  g_debug ("Starting media pipeline");
  self->is_running = TRUE;

  /* First start the receiver pipeline so that
     we may reuse the socket in the sender pipeline */
  /* TODO can we do something similar for RTCP? */
  gst_element_set_state (self->recv_pipeline, GST_STATE_PLAYING);

  g_object_get (self->rtp_src, "used-socket", &socket, NULL);

  if (socket) {
    g_object_set (self->rtp_sink,
                  "close-socket", FALSE,
                  "socket", socket,
                  NULL);
  }
  else
    g_warning ("Could not get used socket of udpsrc element");

  /* Now start the sender pipeline */
  gst_element_set_state (self->send_pipeline, GST_STATE_PLAYING);

  if (self->debug)
    diagnose_ports_in_use (self);

}


void
calls_sip_media_pipeline_stop (CallsSipMediaPipeline *self)
{
  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  g_debug ("Stopping media pipeline");
  self->is_running = FALSE;

  /* Stop the pipelines in reverse order (compared to the starting) */
  gst_element_set_state (self->send_pipeline, GST_STATE_NULL);
  gst_element_set_state (self->recv_pipeline, GST_STATE_NULL);
}


void
calls_sip_media_pipeline_pause (CallsSipMediaPipeline *self)
{
  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  g_debug ("Pause/unpause media pipeline");
  self->is_running = FALSE;
}

