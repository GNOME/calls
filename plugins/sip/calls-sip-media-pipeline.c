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

#include "calls-media-pipeline-enums.h"
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


/* The following defines are used to set/reset bitmaps of playing/paused/stop state */
#define EL_SEND_PIPELINE (1<<0)
#define EL_SEND_AUDIO_SRC (1<<1)
#define EL_SEND_RTPBIN (1<<2)
#define EL_SEND_RTP_SINK (1<<3)
#define EL_SEND_RTCP_SINK (1<<4)
#define EL_SEND_RTCP_SRC (1<<5)
#define EL_SEND_PAYLOADER (1<<6)
#define EL_SEND_ENCODER (1<<7)

#define EL_SEND_ALL_RTP EL_SEND_PIPELINE | EL_SEND_AUDIO_SRC |                  \
  EL_SEND_RTPBIN | EL_SEND_RTP_SINK | EL_SEND_RTCP_SRC | EL_SEND_RTCP_SINK |    \
  EL_SEND_PAYLOADER | EL_SEND_ENCODER
#define EL_SEND_SENDING EL_SEND_AUDIO_SRC | EL_SEND_RTPBIN | EL_SEND_RTP_SINK | \
  EL_SEND_PAYLOADER | EL_SEND_ENCODER

/* leave some room for more elements to be added later */

#define EL_RECV_PIPELINE (1<<16)
#define EL_RECV_AUDIO_SINK (1<<17)
#define EL_RECV_RTPBIN (1<<18)
#define EL_RECV_RTP_SRC (1<<19)
#define EL_RECV_RTCP_SINK (1<<20)
#define EL_RECV_RTCP_SRC (1<<21)
#define EL_RECV_DEPAYLOADER (1<<22)
#define EL_RECV_DECODER (1<<23)

#define EL_RECV_ALL_RTP EL_RECV_PIPELINE | EL_RECV_AUDIO_SINK |                 \
  EL_RECV_RTPBIN | EL_RECV_RTP_SRC | EL_RECV_RTCP_SRC | EL_RECV_RTCP_SINK |     \
  EL_RECV_DEPAYLOADER | EL_RECV_DECODER


enum {
  PROP_0,
  PROP_CODEC,
  PROP_REMOTE,
  PROP_RPORT_RTP,
  PROP_RPORT_RTCP,
  PROP_DEBUG,
  PROP_STATE,
  PROP_LAST_PROP,
};

enum {
  SENDING_STARTED,
  N_SIGNALS
};

static GParamSpec *props[PROP_LAST_PROP];
static uint signals[N_SIGNALS];


struct _CallsSipMediaPipeline {
  GObject parent;

  MediaCodecInfo *codec;
  gboolean debug;

  CallsMediaPipelineState state;
  uint element_map_playing;
  uint element_map_paused;
  uint element_map_stopped;
  gboolean emitted_sending_signal;
  /* Connection details */
  char *remote;

  gint rport_rtp;

  gint rport_rtcp;

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

#if GLIB_CHECK_VERSION(2, 70, 0)
G_DEFINE_FINAL_TYPE (CallsSipMediaPipeline, calls_sip_media_pipeline, G_TYPE_OBJECT)
#else
G_DEFINE_TYPE (CallsSipMediaPipeline, calls_sip_media_pipeline, G_TYPE_OBJECT)
#endif



static void
set_state (CallsSipMediaPipeline  *self,
           CallsMediaPipelineState state)
{
  g_assert (CALLS_SIP_MEDIA_PIPELINE (self));

  if (self->state == state)
    return;

  self->state = state;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);

  self->emitted_sending_signal = FALSE;
}


static void
check_element_maps (CallsSipMediaPipeline *self)
{
  g_assert (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  if (self->element_map_playing == (EL_SEND_ALL_RTP | EL_RECV_ALL_RTP)) {
    g_debug ("All pipeline elements are playing");
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_PLAYING);
    return;
  }

  if (self->element_map_paused == (EL_SEND_ALL_RTP | EL_RECV_ALL_RTP)) {
    g_debug ("All pipeline elements are paused");
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_PAUSED);
    return;
  }

  if (self->element_map_stopped == (EL_SEND_ALL_RTP | EL_RECV_ALL_RTP)) {
    g_debug ("All pipeline elements are stopped");
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_STOPPED);
    return;
  }

  if ((self->element_map_playing & (EL_SEND_SENDING)) == (EL_SEND_SENDING) &&
      !self->emitted_sending_signal) {
    g_debug ("Sender pipeline is sending data to %s RTP/RTCP %d/%d",
             self->remote, self->rport_rtp, self->rport_rtcp);
    g_signal_emit (self, signals[SENDING_STARTED], 0);
    self->emitted_sending_signal = TRUE;
  }
}


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
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (data);

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
    calls_sip_media_pipeline_stop (self);
    break;

  case GST_MESSAGE_STATE_CHANGED:
    {
      GstState oldstate;
      GstState newstate;
      uint element_id = 0;
      uint unset_element_id;

      gst_message_parse_state_changed (message, &oldstate, &newstate, NULL);

      g_debug ("Element %s has changed state from %s to %s",
               GST_OBJECT_NAME (message->src),
               gst_element_state_get_name (oldstate),
               gst_element_state_get_name (newstate));

      /* Sender pipeline elements */
      if (message->src == GST_OBJECT (self->send_pipeline))
        element_id = EL_SEND_PIPELINE;
      else if (message->src == GST_OBJECT (self->audiosrc))
        element_id = EL_SEND_AUDIO_SRC;
      else if (message->src == GST_OBJECT (self->send_rtpbin))
        element_id = EL_SEND_RTPBIN;
      else if (message->src == GST_OBJECT (self->rtp_sink))
        element_id = EL_SEND_RTP_SINK;
      else if (message->src == GST_OBJECT (self->rtcp_send_sink))
        element_id = EL_SEND_RTCP_SINK;
      else if (message->src == GST_OBJECT (self->rtcp_send_src))
        element_id = EL_SEND_RTCP_SRC;
      else if (message->src == GST_OBJECT (self->payloader))
        element_id = EL_SEND_PAYLOADER;
      else if (message->src == GST_OBJECT (self->encoder))
        element_id = EL_SEND_ENCODER;
      /* Receiver pipeline elements */
      else if (message->src == GST_OBJECT (self->recv_pipeline))
        element_id = EL_RECV_PIPELINE;
      else if (message->src == GST_OBJECT (self->audiosink))
        element_id = EL_RECV_AUDIO_SINK;
      else if (message->src == GST_OBJECT (self->recv_rtpbin))
        element_id = EL_RECV_RTPBIN;
      else if (message->src == GST_OBJECT (self->rtp_src))
        element_id = EL_RECV_RTP_SRC;
      else if (message->src == GST_OBJECT (self->rtcp_recv_sink))
        element_id = EL_RECV_RTCP_SINK;
      else if (message->src == GST_OBJECT (self->rtcp_recv_src))
        element_id = EL_RECV_RTCP_SRC;
      else if (message->src == GST_OBJECT (self->depayloader))
        element_id = EL_RECV_DEPAYLOADER;
      else if (message->src == GST_OBJECT (self->decoder))
        element_id = EL_RECV_DECODER;

      unset_element_id = G_MAXUINT ^ element_id;
      if (newstate == GST_STATE_PLAYING) {
        self->element_map_playing |= element_id;
        self->element_map_paused &= unset_element_id;
        self->element_map_stopped &= unset_element_id;
      } else if (newstate == GST_STATE_PAUSED) {
        self->element_map_paused |= element_id;
        self->element_map_playing &= unset_element_id;
        self->element_map_stopped &= unset_element_id;
      } else if (newstate == GST_STATE_NULL) {
        self->element_map_stopped |= element_id;
        self->element_map_playing &= unset_element_id;
        self->element_map_paused &= unset_element_id;
      }

      check_element_maps (self);
      break;
    }

  default:
    if (self->debug)
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

/**
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
                    GError               **error)
{
  g_autoptr (GSocket) rtp_sock = NULL;
  g_autoptr (GSocket) rtcp_sock = NULL;
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

  g_object_bind_property (self, "rport-rtcp",
                          self->rtcp_send_sink, "port",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "remote",
                          self->rtcp_send_sink, "host",
                          G_BINDING_BIDIRECTIONAL);

  /* bind sockets for hole punching scheme */
  g_object_get (self->rtp_src, "used-socket", &rtp_sock, NULL);
  if (!rtp_sock) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not retrieve used socket from RTP udpsrc element");
    return FALSE;
  }

  g_object_set (self->rtp_sink,
                "socket", rtp_sock,
                "close-socket", FALSE,
                NULL);


  g_object_get (self->rtcp_recv_src, "used-socket", &rtcp_sock, NULL);
  if (!rtcp_sock) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not retrieve used socket from RTCP udpsrc element");
    return FALSE;
  }
  g_object_set (self->rtcp_send_sink,
                "socket", rtcp_sock,
                "close-socket", FALSE,
                NULL);
  g_object_set (self->rtcp_send_src,
                "socket", rtcp_sock,
                "close-socket", FALSE,
                NULL);

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


/**
 * Prepares a skeleton receiver pipeline which can later be
 * used to plug codec specific element in.
 * This pipeline just consists of (minimally linked) rtpbin
 * audio sink and two udpsrc elements, one for RTP and one for RTCP.
 *
 * The pipeline will be set ready to let the OS allocate sockets
 * for us instead of building and providing GSockets ourselves
 * by hand. These GSockets are reused for any outgoing traffic in our
 * hole punching scheme as a simple NAT traversal technique.
 */
static gboolean
recv_pipeline_init (CallsSipMediaPipeline *self,
                    GError               **error)
{
  g_autoptr (GSocket) rtcp_sock = NULL;
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


  /* port 0 means allocate */
  g_object_set (self->rtp_src, "port", 0, NULL);
  g_object_set (self->rtcp_recv_src, "port", 0, NULL);

  g_object_bind_property (self, "rport-rtcp",
                          self->rtcp_recv_sink, "port",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "remote",
                          self->rtcp_recv_sink, "host",
                          G_BINDING_BIDIRECTIONAL);

  gst_bin_add (GST_BIN (self->recv_pipeline), self->recv_rtpbin);
  gst_bin_add_many (GST_BIN (self->recv_pipeline), self->rtp_src,
                    self->rtcp_recv_src, self->rtcp_recv_sink, NULL);

  self->bus_recv = gst_pipeline_get_bus (GST_PIPELINE (self->recv_pipeline));
  self->bus_watch_recv = gst_bus_add_watch (self->bus_recv, on_bus_message, self);

  g_object_set (self->rtp_src,
                "close-socket", FALSE,
                "reuse", TRUE,
                NULL);
  g_object_set (self->rtcp_recv_src,
                "close-socket", FALSE,
                "reuse", TRUE,
                NULL);

  /* Set pipeline to ready to get ports allocated */
  gst_element_set_state (self->recv_pipeline, GST_STATE_READY);

  g_object_get (self->rtcp_recv_src, "used-socket", &rtcp_sock, NULL);
  if (!rtcp_sock) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not retrieve used socket from RTCP udpsrc element");
    return FALSE;
  }

  /* Ports are allocated. Let's reuse the socket for rtcp source in the sink for NAT traversal*/
  g_object_set (self->rtcp_recv_sink,
                "socket", rtcp_sock,
                "close-socket", FALSE,
                NULL);

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

  case PROP_RPORT_RTP:
    g_value_set_uint (value, self->rport_rtp);
    break;

  case PROP_RPORT_RTCP:
    g_value_set_uint (value, self->rport_rtcp);
    break;

  case PROP_DEBUG:
    g_value_set_boolean (value, self->debug);
    break;

  case PROP_STATE:
    g_value_set_enum (value, calls_sip_media_pipeline_get_state (self));
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
calls_sip_media_pipeline_constructed (GObject *object)
{
  g_autoptr (GError) error = NULL;
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (object);

  G_OBJECT_CLASS (calls_sip_media_pipeline_parent_class)->constructed (object);

  set_state (self, CALLS_MEDIA_PIPELINE_STATE_INITIALIZING);

  if (!recv_pipeline_init (self, &error)) {
    g_warning ("Could not create receive pipeline: %s", error->message);
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_ERROR);
    return;
  }

  if (!send_pipeline_init (self, &error)) {
    g_warning ("Could not create send pipeline: %s", error->message);
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_ERROR);
    return;
  }

  set_state (self, CALLS_MEDIA_PIPELINE_STATE_NEED_CODEC);
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
  object_class->constructed = calls_sip_media_pipeline_constructed;
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

  props[PROP_STATE] = g_param_spec_enum ("state",
                                         "State",
                                         "The state of the media pipeline",
                                         CALLS_TYPE_MEDIA_PIPELINE_STATE,
                                         CALLS_MEDIA_PIPELINE_STATE_UNKNOWN,
                                         G_PARAM_READABLE);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  signals[SENDING_STARTED] =
    g_signal_new ("sending-started",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}


static void
calls_sip_media_pipeline_init (CallsSipMediaPipeline *self)
{
  if (!gst_is_initialized ())
    gst_init (NULL, NULL);
}


CallsSipMediaPipeline*
calls_sip_media_pipeline_new (MediaCodecInfo *codec)
{
  CallsSipMediaPipeline *pipeline;

  pipeline = g_object_new (CALLS_TYPE_SIP_MEDIA_PIPELINE, NULL);

  if (codec)
    g_object_set (pipeline, "codec", codec, NULL);

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
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_ERROR);
    return;
  }

  if (!send_pipeline_setup_codecs (self, codec, &error)) {
    g_warning ("Error trying to setup codec for send pipeline: %s",
               error->message);
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_ERROR);
    return;
  }

  self->codec = codec;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CODEC]);

  set_state (self, CALLS_MEDIA_PIPELINE_STATE_READY);
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

  if (self->state != CALLS_MEDIA_PIPELINE_STATE_PLAYING &&
      self->state != CALLS_MEDIA_PIPELINE_STATE_PAUSED) {
    g_warning ("Cannot diagnose ports when pipeline is not active");
    return;
  }

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
  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  if (self->state != CALLS_MEDIA_PIPELINE_STATE_READY) {
    g_warning ("Cannot start pipeline because it's not ready");
    return;
  }

  g_debug ("Starting media pipeline");

  gst_element_set_state (self->recv_pipeline, GST_STATE_PLAYING);
  gst_element_set_state (self->send_pipeline, GST_STATE_PLAYING);

  g_debug ("RTP/RTCP port after starting pipeline: %d/%d",
           calls_sip_media_pipeline_get_rtp_port (self),
           calls_sip_media_pipeline_get_rtcp_port (self));

  set_state (self, CALLS_MEDIA_PIPELINE_STATE_PLAY_PENDING);

  if (self->debug)
    diagnose_ports_in_use (self);
}


void
calls_sip_media_pipeline_stop (CallsSipMediaPipeline *self)
{
  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  g_debug ("Stopping media pipeline");

  /* Stop the pipelines in reverse order (compared to the starting) */
  gst_element_set_state (self->send_pipeline, GST_STATE_NULL);
  gst_element_set_state (self->recv_pipeline, GST_STATE_NULL);

  set_state (self, CALLS_MEDIA_PIPELINE_STATE_STOP_PENDING);
}


void
calls_sip_media_pipeline_pause (CallsSipMediaPipeline *self,
                                gboolean               pause)
{
  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  if (pause &&
      (self->state == CALLS_MEDIA_PIPELINE_STATE_PAUSED ||
       self->state == CALLS_MEDIA_PIPELINE_STATE_PAUSE_PENDING))
    return;

  if (!pause &&
      (self->state == CALLS_MEDIA_PIPELINE_STATE_PLAYING ||
       self->state == CALLS_MEDIA_PIPELINE_STATE_PLAY_PENDING))
    return;

  if (self->state != CALLS_MEDIA_PIPELINE_STATE_PLAYING &&
      self->state != CALLS_MEDIA_PIPELINE_STATE_PLAY_PENDING &&
      self->state != CALLS_MEDIA_PIPELINE_STATE_PAUSED &&
      self->state != CALLS_MEDIA_PIPELINE_STATE_PAUSE_PENDING) {
    g_warning ("Cannot pause or unpause pipeline because it's not currently active");
    return;
  }

  g_debug ("%s media pipeline", pause ?
           "Pausing" :
           "Unpausing");

  gst_element_set_state (self->recv_pipeline, pause ?
                         GST_STATE_PAUSED :
                         GST_STATE_PLAYING);
  gst_element_set_state (self->send_pipeline, pause ?
                         GST_STATE_PAUSED :
                         GST_STATE_PLAYING);

  set_state (self, pause ?
             CALLS_MEDIA_PIPELINE_STATE_PAUSE_PENDING :
             CALLS_MEDIA_PIPELINE_STATE_PLAY_PENDING);
}


int
calls_sip_media_pipeline_get_rtp_port (CallsSipMediaPipeline *self)
{
  int port;

  g_return_val_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self), 0);

  g_object_get (self->rtp_src, "port", &port, NULL);

  return port;
}


int
calls_sip_media_pipeline_get_rtcp_port (CallsSipMediaPipeline *self)
{
  int port;

  g_return_val_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self), 0);

  g_object_get (self->rtcp_recv_src, "port", &port, NULL);

  return port;
}


CallsMediaPipelineState
calls_sip_media_pipeline_get_state (CallsSipMediaPipeline *self)
{
  g_return_val_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self),
                        CALLS_MEDIA_PIPELINE_STATE_UNKNOWN);

  return self->state;
}

#undef MAKE_ELEMENT

#undef EL_SEND_PIPELINE
#undef EL_SEND_AUDIO_SRC
#undef EL_SEND_RTPBIN
#undef EL_SEND_RTP_SINK
#undef EL_SEND_RTCP_SINK
#undef EL_SEND_RTCP_SRC
#undef EL_SEND_PAYLOADER
#undef EL_SEND_ENCODER

#undef EL_SEND_ALL_RTP
#undef EL_SEND_SENDING

#undef EL_RECV_PIPELINE
#undef EL_RECV_AUDIO_SINK
#undef EL_RECV_RTPBIN
#undef EL_RECV_RTP_SRC
#undef EL_RECV_RTCP_SINK
#undef EL_RECV_RTCP_SRC
#undef EL_RECV_DEPAYLOADER
#undef EL_RECV_DECODER

#undef EL_RECV_ALL_RTP

