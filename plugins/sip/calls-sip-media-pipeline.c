/*
 * Copyright (C) 2021-2022 Purism SPC
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
#include "util.h"

#include <gst/gst.h>
#include <gio/gio.h>

#define MAKE_ELEMENT(var, element, name)                        \
  self->var = gst_element_factory_make (element, name);         \
  if (!self->var) {                                             \
    if (error)                                                  \
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,        \
                   "Could not create '%s' element of type %s",  \
                   name ? : "unnamed", element);                \
    return FALSE;                                               \
  }


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

  g_debug ("pad added: %s", GST_PAD_NAME (srcpad));

  sinkpad = gst_element_get_static_pad (depayloader, "sink");
  g_debug ("linking to %s", GST_PAD_NAME (sinkpad));

  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK)
    g_warning ("Failed to link rtpbin to depayloader");

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
      g_warning ("Error on the message bus: %s (%s)", error->message, msg);
      break;
    }

  case GST_MESSAGE_WARNING:
    {
      g_autoptr (GError) error = NULL;
      g_autofree char *msg = NULL;

      gst_message_parse_warning (message, &error, &msg);
      g_warning ("Warning on the message bus: %s (%s)", error->message, msg);
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


/* Setting up pipelines */

static gboolean
send_pipeline_link_elements (CallsSipMediaPipeline *self,
                             GError               **error)
{
  g_autoptr (GstPad) srcpad = NULL;
  g_autoptr (GstPad) sinkpad = NULL;

  g_assert (CALLS_IS_SIP_MEDIA_PIPELINE (self));

#if GST_CHECK_VERSION (1, 20, 0)
  sinkpad = gst_element_request_pad_simple (self->send_rtpbin, "send_rtp_sink_0");
#else
  sinkpad = gst_element_get_request_pad (self->send_rtpbin, "send_rtp_sink_0");
#endif
  srcpad = gst_element_get_static_pad (self->payloader, "src");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    if (error)
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
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link rtpbin to rtpsink");
    return FALSE;
  }

  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  /* RTCP srcpad to udpsink */
#if GST_CHECK_VERSION (1, 20, 0)
  srcpad = gst_element_request_pad_simple (self->send_rtpbin, "send_rtcp_src_0");
#else
  srcpad = gst_element_get_request_pad (self->send_rtpbin, "send_rtcp_src_0");
#endif
  sinkpad = gst_element_get_static_pad (self->rtcp_send_sink, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link rtpbin to rtcpsink");
    return FALSE;
  }

  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  /* receive RTCP */
  srcpad = gst_element_get_static_pad (self->rtcp_send_src, "src");
#if GST_CHECK_VERSION (1, 20, 0)
  sinkpad = gst_element_request_pad_simple (self->send_rtpbin, "recv_rtcp_sink_0");
#else
  sinkpad = gst_element_get_request_pad (self->send_rtpbin, "recv_rtcp_sink_0");
#endif
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    if (error)
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to link rtcpsrc to rtpbin");
    return FALSE;
  }

  return TRUE;
}


static gboolean
send_pipeline_setup_codecs (CallsSipMediaPipeline *self,
                            MediaCodecInfo        *codec,
                            GError               **error)
{
  g_assert (CALLS_IS_SIP_MEDIA_PIPELINE (self));
  g_assert (codec);

  MAKE_ELEMENT (encoder, codec->gst_encoder_name, "encoder");
  MAKE_ELEMENT (payloader, codec->gst_payloader_name, "payloader");

  gst_bin_add_many (GST_BIN (self->send_pipeline), self->payloader, self->encoder,
                    self->audiosrc, NULL);

  if (!gst_element_link_many (self->audiosrc, self->encoder, self->payloader, NULL)) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link audiosrc encoder and payloader");
    return FALSE;
  }

  return send_pipeline_link_elements (self, error);
}

/** TODO: we're describing the desired state (not the current state)
 * Prepare a skeleton send pipeline where we can later
 * plug the codec specific elements into.
 *
 * In contrast to the receiver pipeline there is no need to start the
 * pipeline until we actually want to establish a media session.
 *
 * The receiver pipeline should have been initialized at this point
 * allowing us to reuse GSockets.
 */
static gboolean
send_pipeline_init (CallsSipMediaPipeline *self,
                    GCancellable          *cancellable,
                    GError               **error)
{
  const char *env_var;

  g_assert (CALLS_SIP_MEDIA_PIPELINE (self));

  self->send_pipeline = gst_pipeline_new ("rtp-send-pipeline");

  if (!self->send_pipeline) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not create send pipeline");
    return FALSE;
  }

  gst_object_ref_sink (self->send_pipeline);

  env_var = g_getenv ("CALLS_AUDIOSRC");
  if (!STR_IS_NULL_OR_EMPTY (env_var)) {
    MAKE_ELEMENT (audiosrc, env_var, "audiosource");
  } else {
    g_autoptr (GstStructure) gst_props = NULL;

    MAKE_ELEMENT (audiosrc, "pulsesrc", "audiosource");

    /* enable echo cancellation and set buffer size to 40ms */
    gst_props = gst_structure_new ("props",
                                   "media.role", G_TYPE_STRING, "phone",
                                   "filter.want", G_TYPE_STRING, "echo-cancel",
                                   NULL);

    g_object_set (self->audiosrc,
                  "buffer-time", (gint64) 40000,
                  "stream-properties", gst_props,
                  NULL);
  }

  MAKE_ELEMENT (send_rtpbin, "rtpbin", "send-rtpbin");
  MAKE_ELEMENT (rtp_sink, "udpsink", "rtp-udp-sink");
  MAKE_ELEMENT (rtcp_send_src, "udpsrc", "rtcp-udp-send-src");
  MAKE_ELEMENT (rtcp_send_sink, "udpsink", "rtcp-udp-send-sink");

  g_object_set (self->rtcp_send_sink,
                "async", FALSE,
                "sync", FALSE,
                NULL);

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

  gst_bin_add (GST_BIN (self->send_pipeline), self->send_rtpbin);
  gst_bin_add_many (GST_BIN (self->send_pipeline), self->rtp_sink,
                    self->rtcp_send_src, self->rtcp_send_sink, NULL);

  self->bus_send = gst_pipeline_get_bus (GST_PIPELINE (self->send_pipeline));
  self->bus_watch_send = gst_bus_add_watch (self->bus_send, on_bus_message, self);

  return TRUE;
}


static gboolean
recv_pipeline_link_elements (CallsSipMediaPipeline *self,
                             GError               **error)
{
  g_autoptr (GstPad) srcpad = NULL;
  g_autoptr (GstPad) sinkpad = NULL;

  g_assert (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  srcpad = gst_element_get_static_pad (self->rtp_src, "src");
#if GST_CHECK_VERSION (1, 20, 0)
  sinkpad = gst_element_request_pad_simple (self->recv_rtpbin, "recv_rtp_sink_0");
#else
  sinkpad = gst_element_get_request_pad (self->recv_rtpbin, "recv_rtp_sink_0");
#endif
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link rtpsrc to rtpbin");
    return FALSE;
  }

  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  srcpad = gst_element_get_static_pad (self->rtcp_recv_src, "src");
#if GST_CHECK_VERSION (1, 20 , 0)
  sinkpad = gst_element_request_pad_simple (self->recv_rtpbin, "recv_rtcp_sink_0");
#else
  sinkpad = gst_element_get_request_pad (self->recv_rtpbin, "recv_rtcp_sink_0");
#endif
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link rtcpsrc to rtpbin");
    return FALSE;
  }

  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

#if GST_CHECK_VERSION (1, 20, 0)
  srcpad = gst_element_request_pad_simple (self->recv_rtpbin, "send_rtcp_src_0");
#else
  srcpad = gst_element_get_request_pad (self->recv_rtpbin, "send_rtcp_src_0");
#endif
  sinkpad = gst_element_get_static_pad (self->rtcp_recv_sink, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link rtpbin to rtcpsink");
    return FALSE;
  }

  g_signal_connect (self->recv_rtpbin, "pad-added", G_CALLBACK (on_pad_added), self->depayloader);

  return TRUE;
}


static gboolean
recv_pipeline_setup_codecs (CallsSipMediaPipeline *self,
                            MediaCodecInfo        *codec,
                            GError               **error)
{
  g_autoptr (GstCaps) caps = NULL;
  g_autofree char *caps_string = NULL;

  g_assert (CALLS_IS_SIP_MEDIA_PIPELINE (self));
  g_assert (codec);

  MAKE_ELEMENT (decoder, codec->gst_decoder_name, "decoder");
  MAKE_ELEMENT (depayloader, codec->gst_depayloader_name, "depayloader");

  gst_bin_add_many (GST_BIN (self->recv_pipeline), self->depayloader, self->decoder,
                    self->audiosink, NULL);

  if (!gst_element_link_many (self->depayloader, self->decoder, self->audiosink, NULL)) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link depayloader decoder and audiosink");
    return FALSE;
  }

  /* UDP src capabilities */
  caps_string = media_codec_get_gst_capabilities (codec);
  g_debug ("Capabilities:\n%s", caps_string);

  caps = gst_caps_from_string (caps_string);

  /* set udp sinks and sources for RTP and RTCP */
  g_object_set (self->rtp_src,
                "caps", caps,
                NULL);

  return recv_pipeline_link_elements (self, error);
}


/** TODO: we're describing the desired state (not the current state)
 * Prepares a skeleton receiver pipeline which can later be
 * used to plug codec specific element in.
 * This pipeline just consists of (minimally linked) rtpbin
 * audio sink and two udpsrc elements, one for RTP and one for RTCP.
 *
 * The pipeline will be started and stopped to let the OS allocate
 * sockets for us instead of building and providing GSockets ourselves
 * by hand. These GSockets will later be reused for any outgoing
 * traffic for of our hole punching scheme as a simple NAT traversal
 * technique.
 */
static gboolean
recv_pipeline_init (CallsSipMediaPipeline *self,
                    GCancellable          *cancellable,
                    GError               **error)
{
  const char *env_var;

  g_assert (CALLS_SIP_MEDIA_PIPELINE (self));

  self->recv_pipeline = gst_pipeline_new ("rtp-recv-pipeline");

  if (!self->recv_pipeline) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not create receiver pipeline");
    return FALSE;
  }

  gst_object_ref_sink (self->recv_pipeline);

  env_var = g_getenv ("CALLS_AUDIOSINK");
  if (!STR_IS_NULL_OR_EMPTY (env_var)) {
    MAKE_ELEMENT (audiosink, env_var, "audiosink");
  } else {
      g_autoptr (GstStructure) gst_props = NULL;

      MAKE_ELEMENT (audiosink, "pulsesink", "audiosink");

      /* enable echo cancellation and set buffer size to 40ms */
      gst_props = gst_structure_new ("props",
                                     "media.role", G_TYPE_STRING, "phone",
                                     "filter.want", G_TYPE_STRING, "echo-cancel",
                                     NULL);

      g_object_set (self->audiosink,
                    "buffer-time", (gint64) 40000,
                    "stream-properties", gst_props,
                    NULL);

  }

  MAKE_ELEMENT (recv_rtpbin, "rtpbin", "recv-rtpbin")
  MAKE_ELEMENT (rtp_src, "udpsrc", "rtp-udp-src");
  MAKE_ELEMENT (rtcp_recv_src, "udpsrc", "rtcp-udp-recv-src");
  MAKE_ELEMENT (rtcp_recv_sink, "udpsink", "rtcp-udp-recv-sink");


  g_object_set (self->rtcp_recv_sink,
                "async", FALSE,
                "sync", FALSE,
                NULL);


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

  gst_bin_add (GST_BIN (self->recv_pipeline), self->recv_rtpbin);
  gst_bin_add_many (GST_BIN (self->recv_pipeline), self->rtp_src,
                    self->rtcp_recv_src, self->rtcp_recv_sink, NULL);

  /* TODO use temporary bus watch for the initial pipeline start/stop */
  self->bus_recv = gst_pipeline_get_bus (GST_PIPELINE (self->recv_pipeline));
  self->bus_watch_recv = gst_bus_add_watch (self->bus_recv, on_bus_message, self);


  return TRUE;
}


static void
calls_sip_media_pipeline_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
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
calls_sip_media_pipeline_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (object);

  switch (property_id) {
  case PROP_CODEC:
    calls_sip_media_pipeline_set_codec (self, g_value_get_pointer (value));
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
calls_sip_media_pipeline_finalize (GObject *object)
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

  object_class->set_property = calls_sip_media_pipeline_set_property;
  object_class->get_property = calls_sip_media_pipeline_get_property;
  object_class->finalize = calls_sip_media_pipeline_finalize;

  /* Maybe we want to turn Codec into a GObject later */
  props[PROP_CODEC] = g_param_spec_pointer ("codec",
                                            "Codec",
                                            "Media codec",
                                            G_PARAM_READWRITE);

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
  if (!gst_is_initialized ())
    gst_init (NULL, NULL);
}


static gboolean
pipelines_initable_init (GInitable    *initable,
                         GCancellable *cancellable,
                         GError      **error)
{
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (initable);

  if (!recv_pipeline_init (self, cancellable, error))
    return FALSE;

  if (!send_pipeline_init (self, cancellable, error))
    return FALSE;

  return TRUE;
}


static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = pipelines_initable_init;
}


CallsSipMediaPipeline*
calls_sip_media_pipeline_new (MediaCodecInfo *codec)
{
  CallsSipMediaPipeline *pipeline;
  g_autoptr (GError) error = NULL;

  pipeline = g_initable_new (CALLS_TYPE_SIP_MEDIA_PIPELINE, NULL, &error,
                             NULL);

  if (pipeline) {
    if (codec)
      g_object_set (pipeline, "codec", codec, NULL);
  } else {
    g_warning ("Media pipeline could not be initialized: %s", error->message);
  }

  return pipeline;
}


void
calls_sip_media_pipeline_set_codec (CallsSipMediaPipeline *self,
                                    MediaCodecInfo        *codec)
{
  g_autoptr (GError) error = NULL;

  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self));
  g_return_if_fail (codec);

  if (self->codec == codec)
    return;

  if (self->codec) {
    g_warning ("Cannot change codec of a pipeline. Use a new pipeline instead.");
    return;
  }

  if (!media_codec_available_in_gst (codec)) {
    g_warning ("Cannot setup pipeline with codec '%s' because it's not available in GStreamer",
               codec->name);
    return;
  }

  if (!recv_pipeline_setup_codecs (self, codec, &error)) {
    g_warning ("Error trying to setup codec for receive pipeline: %s",
               error->message);
    return;
  }

  if (!send_pipeline_setup_codecs (self, codec, &error)) {
    g_warning ("Error trying to setup codec for send pipeline: %s",
               error->message);
    return;
  }

  self->codec = codec;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CODEC]);
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

  if (!self->codec) {
    g_warning ("Codec not set for this pipeline. Cannot start");
    return;
  }

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
calls_sip_media_pipeline_pause (CallsSipMediaPipeline *self,
                                gboolean               pause)
{
  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  if (self->is_running != pause)
    return;

  g_debug ("%s media pipeline", self->is_running ?
           "Pausing" :
           "Unpausing");
  gst_element_set_state (self->recv_pipeline, self->is_running ?
                         GST_STATE_PAUSED :
                         GST_STATE_PLAYING);
  gst_element_set_state (self->send_pipeline, self->is_running ?
                         GST_STATE_PAUSED :
                         GST_STATE_PLAYING);

  self->is_running = !self->is_running;
}


#undef MAKE_ELEMENT
