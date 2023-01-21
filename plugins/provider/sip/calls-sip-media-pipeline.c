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

#include "calls-config.h"

#include "calls-media-pipeline-enums.h"
#include "calls-sip-media-pipeline.h"
#include "calls-srtp-utils.h"
#include "calls-util.h"

#include <glib-unix.h>
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
#define EL_PIPELINE (1<<0)
#define EL_RTPBIN (1<<1)
#define EL_RTP_SRC (1<<2)
#define EL_RTP_SINK (1<<3)
#define EL_RTCP_SRC (1<<4)
#define EL_RTCP_SINK (1<<5)

#define EL_SRTP_ENCODER (1<<6)
#define EL_SRTP_DECODER (1<<7)

#define EL_AUDIO_SRC (1<<8)
#define EL_AUDIO_SINK (1<<9)

#define EL_PAYLOADER (1<<10)
#define EL_DEPAYLOADER (1<<11)

#define EL_ENCODER (1<<12)
#define EL_DECODER (1<<13)

#define EL_SENDING                                                \
  (EL_AUDIO_SRC | EL_ENCODER | EL_PAYLOADER |                     \
   EL_RTPBIN | EL_RTP_SINK | EL_RTCP_SINK)

#define EL_ALL_RTP                                                \
  (EL_PIPELINE | EL_RTPBIN |                                      \
   EL_RTP_SRC | EL_RTP_SINK | EL_RTCP_SRC | EL_RTCP_SINK |        \
   EL_AUDIO_SRC | EL_AUDIO_SINK |                                 \
   EL_ENCODER | EL_DECODER | EL_PAYLOADER | EL_DEPAYLOADER)

#define EL_ALL_SRTP (EL_ALL_RTP | EL_SRTP_ENCODER | EL_SRTP_DECODER)


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
  GObject                      parent;

  MediaCodecInfo              *codec;
  gboolean                     debug;

  CallsMediaPipelineState      state;
  uint                         element_map_playing;
  uint                         element_map_paused;
  uint                         element_map_stopped;
  gboolean                     emitted_sending_signal;

  /* Connection details */
  char                        *remote;
  gint                         rport_rtp;
  gint                         rport_rtcp;

  GstElement                  *pipeline;
  GstElement                  *rtpbin;

  GstElement                  *rtp_src;
  GstElement                  *rtp_sink;
  GstElement                  *rtcp_sink;
  GstElement                  *rtcp_src;

  GstElement                  *audio_src;
  GstElement                  *payloader;
  GstElement                  *encoder;

  GstElement                  *audio_sink;
  GstElement                  *depayloader;
  GstElement                  *decoder;

  /* SRTP */
  gboolean                     use_srtp;
  calls_srtp_crypto_attribute *crypto_own;
  calls_srtp_crypto_attribute *crypto_theirs;

  GstElement                  *srtpenc;
  GstElement                  *srtpdec;

  gulong                       request_rtpbin_rtp_decoder_id;
  gulong                       request_rtpbin_rtp_encoder_id;
  gulong                       request_rtpbin_rtcp_encoder_id;
  gulong                       request_rtpbin_rtcp_decoder_id;

  /* Gstreamer busses */
  GstBus                      *bus;
  guint                        bus_watch_id;
};

#if GLIB_CHECK_VERSION (2, 70, 0)
G_DEFINE_FINAL_TYPE (CallsSipMediaPipeline, calls_sip_media_pipeline, G_TYPE_OBJECT)
#else
G_DEFINE_TYPE (CallsSipMediaPipeline, calls_sip_media_pipeline, G_TYPE_OBJECT)
#endif



static void
set_state (CallsSipMediaPipeline  *self,
           CallsMediaPipelineState state)
{
  g_autoptr (GEnumClass) enum_class = NULL;
  GEnumValue *enum_val;

  g_autofree char *fname = NULL;

  g_assert (CALLS_SIP_MEDIA_PIPELINE (self));

  if (self->state == state)
    return;

  self->state = state;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);

  self->emitted_sending_signal = FALSE;

  if (state == CALLS_MEDIA_PIPELINE_STATE_INITIALIZING)
    return;

  enum_class = g_type_class_ref (CALLS_TYPE_MEDIA_PIPELINE_STATE);
  enum_val = g_enum_get_value (enum_class, state);

  fname = g_strdup_printf ("calls-%s", enum_val->value_nick);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (self->pipeline),
                                     GST_DEBUG_GRAPH_SHOW_ALL,
                                     fname);
}


static void
check_element_maps (CallsSipMediaPipeline *self)
{
  uint all_rtp_elements;

  g_assert (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  all_rtp_elements = self->use_srtp ? EL_ALL_SRTP : EL_ALL_RTP;

  if (self->element_map_playing == all_rtp_elements) {
    g_debug ("All pipeline elements are playing");
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_PLAYING);
    return;
  }

  if (self->element_map_paused == all_rtp_elements) {
    g_debug ("All pipeline elements are paused");
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_PAUSED);
    return;
  }

  if (self->element_map_stopped == all_rtp_elements) {
    g_debug ("All pipeline elements are stopped");
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_STOPPED);
    return;
  }

  if ((self->element_map_playing & (EL_SENDING)) == (EL_SENDING) &&
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

    if (message->src == GST_OBJECT (self->pipeline))
      element_id = EL_PIPELINE;
    else if (message->src == GST_OBJECT (self->rtpbin))
      element_id = EL_RTPBIN;

    else if (message->src == GST_OBJECT (self->rtp_src))
      element_id = EL_RTP_SRC;
    else if (message->src == GST_OBJECT (self->rtp_sink))
      element_id = EL_RTP_SINK;

    else if (message->src == GST_OBJECT (self->rtcp_src))
      element_id = EL_RTCP_SRC;
    else if (message->src == GST_OBJECT (self->rtcp_sink))
      element_id = EL_RTCP_SINK;

    else if (message->src == GST_OBJECT (self->srtpenc))
      element_id = EL_SRTP_ENCODER;
    else if (message->src == GST_OBJECT (self->srtpdec))
      element_id = EL_SRTP_DECODER;

    else if (message->src == GST_OBJECT (self->audio_src))
      element_id = EL_AUDIO_SRC;
    else if (message->src == GST_OBJECT (self->audio_sink))
      element_id = EL_AUDIO_SINK;

    else if (message->src == GST_OBJECT (self->payloader))
      element_id = EL_PAYLOADER;
    else if (message->src == GST_OBJECT (self->depayloader))
      element_id = EL_DEPAYLOADER;

    else if (message->src == GST_OBJECT (self->encoder))
      element_id = EL_ENCODER;
    else if (message->src == GST_OBJECT (self->decoder))
      element_id = EL_DECODER;

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


/* SRTP setup */

static GstCaps *
on_srtpdec_request_key (GstElement *srtpdec,
                        guint       ssrc,
                        gpointer    user_data)
{
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (user_data);

  GstCaps *caps;

  const char *srtp_cipher = "null";
  const char *srtcp_cipher = "null";
  const char *srtp_auth = "null";
  const char *srtcp_auth = "null";

  gboolean need_mki;

  if (!calls_srtp_crypto_get_srtpdec_params (self->crypto_theirs,
                                             &srtp_cipher,
                                             &srtp_auth,
                                             &srtcp_cipher,
                                             &srtcp_auth))
    return NULL;

  if (self->crypto_theirs->n_key_params == 0 ||
      self->crypto_theirs->n_key_params > 16) {
    g_warning ("Got %u key parameters, but can only handle between 1 and 16",
               self->crypto_theirs->n_key_params);

    return NULL;
  }

  need_mki = self->crypto_theirs->n_key_params > 1;

  if (self->crypto_theirs->n_key_params == 1) {
    /* g_autofree guchar *key_salt = NULL; */
    guchar *key_salt = NULL;
    gsize key_salt_length;
    g_autoptr (GstBuffer) key_buffer = NULL;

    key_salt = g_base64_decode (self->crypto_theirs->key_params[0].b64_keysalt,
                                &key_salt_length);
    key_buffer = gst_buffer_new_wrapped (key_salt, key_salt_length);

    /* TODO Setting up MKI buffer not implemented yet */
    if (self->crypto_theirs->key_params[0].mki) {
      g_warning ("Using MKI is not implemented yet");
      return NULL;
    }

    return gst_caps_new_simple ("application/x-srtp",
                                "srtp-key", GST_TYPE_BUFFER, key_buffer,
                                "srtp-cipher", G_TYPE_STRING, srtp_cipher,
                                "srtcp-cipher", G_TYPE_STRING, srtcp_cipher,
                                "srtp-auth", G_TYPE_STRING, srtp_auth,
                                "srtcp-auth", G_TYPE_STRING, srtcp_auth,
                                NULL);

  }

  /* TODO Setting up MKI buffer not implemented yet */
  g_warning ("Using MKI is not implemented yet");
  return NULL;

  caps = gst_caps_new_simple ("application/x-srtp",
                              "srtp-cipher", G_TYPE_STRING, srtp_cipher,
                              "srtcp-cipher", G_TYPE_STRING, srtcp_cipher,
                              "srtp-auth", G_TYPE_STRING, srtp_auth,
                              "srtcp-auth", G_TYPE_STRING, srtcp_auth,
                              NULL);


  for (guint i = 0; i < self->crypto_theirs->n_key_params; i++) {
    GstStructure *structure;

    g_autofree char *structure_name = g_strdup_printf ("key-%u", i);

    guchar *key_salt = NULL;
    gsize key_salt_length;
    g_autoptr (GstBuffer) key_buffer = NULL;
    g_autoptr (GstBuffer) mki_buffer = NULL;

    key_salt = g_base64_decode (self->crypto_theirs->key_params[0].b64_keysalt,
                                &key_salt_length);
    key_buffer = gst_buffer_new_wrapped (key_salt, key_salt_length);


    if (i == 0 && need_mki) {
      structure = gst_structure_new (structure_name,
                                     "srtp-key", GST_TYPE_BUFFER, key_buffer,
                                     "mki", GST_TYPE_BUFFER, mki_buffer,
                                     NULL);
    } else if (i == 0 && !need_mki) {
      structure = gst_structure_new (structure_name,
                                     "srtp-key", GST_TYPE_BUFFER, key_buffer,
                                     NULL);
    } else {
      g_autofree char *key_field_name = g_strdup_printf ("srtp-key%u", i+1);
      g_autofree char *mki_field_name = g_strdup_printf ("mki%u", i+1);

      structure = gst_structure_new (structure_name,
                                     key_field_name, GST_TYPE_BUFFER, key_buffer,
                                     mki_field_name, GST_TYPE_BUFFER, mki_buffer,
                                     NULL);
    }
    gst_caps_append_structure (caps, structure);
  }

  return caps;
}


static GstElement *
on_rtpbin_request_decoder (GstElement *rtpbin,
                           guint       session_id,
                           gpointer    user_data)
{
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (user_data);

  if (!self->use_srtp)
    return NULL;

  return gst_object_ref (self->srtpdec);
}


static GstElement *
on_rtpbin_request_encoder (GstElement *rtpbin,
                           guint       session_id,
                           gpointer    user_data)
{
  CallsSipMediaPipeline *self = CALLS_SIP_MEDIA_PIPELINE (user_data);

  if (!self->use_srtp)
    return NULL;

  return gst_object_ref (self->srtpenc);
}


/* Pipeline setup */

static gboolean
setup_socket_reuse (CallsSipMediaPipeline *self,
                    GError               **error)
{
  g_autoptr (GSocket) rtp_sock = NULL;
  g_autoptr (GSocket) rtcp_sock = NULL;

  /* set rtp element ready and lock it's state so it doesn't get stopped */
  gst_element_set_locked_state (self->rtp_src, TRUE);
  gst_element_set_state (self->rtp_src, GST_STATE_READY);

  g_object_get (self->rtp_src, "used-socket", &rtp_sock, NULL);
  if (!rtp_sock) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not retrieve used socket from RTP udpsrc element");
    return FALSE;
  }

  /* configure socket and don't close it, since it belongs to rtp_src */
  g_object_set (self->rtp_sink,
                "socket", rtp_sock,
                "close-socket", FALSE,
                NULL);

  /* set rtcp element ready and lock it's state so it doesn't get stopped */
  gst_element_set_locked_state (self->rtcp_src, TRUE);
  gst_element_set_state (self->rtcp_src, GST_STATE_READY);

  g_object_get (self->rtcp_src, "used-socket", &rtcp_sock, NULL);
  if (!rtcp_sock) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not retrieve used socket from RTCP udpsrc element");
    return FALSE;
  }


  /* configure socket and don't close it, since it belongs to rtcp_src */
  g_object_set (self->rtcp_sink,
                "socket", rtcp_sock,
                "close-socket", FALSE,
                NULL);

  return TRUE;
}


static gboolean
pipeline_init (CallsSipMediaPipeline *self,
               GError               **error)
{
  GstPad *tmppad;
  const char *env_var;

  g_assert (CALLS_SIP_MEDIA_PIPELINE (self));

  self->pipeline = gst_pipeline_new ("media-pipeline");

  if (!self->pipeline) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not create media pipeline");
    return FALSE;
  }

  gst_object_ref_sink (self->pipeline);

  /* Audio source*/
  env_var = g_getenv ("CALLS_AUDIOSRC");
  if (!STR_IS_NULL_OR_EMPTY (env_var)) {
    MAKE_ELEMENT (audio_src, env_var, "audiosource");
  } else {
    g_autoptr (GstStructure) gst_props = NULL;

    MAKE_ELEMENT (audio_src, "pulsesrc", "audiosource");

    /* enable echo cancellation and set buffer size to 40ms */
    gst_props = gst_structure_new ("props",
                                   "media.role", G_TYPE_STRING, "phone",
                                   "filter.want", G_TYPE_STRING, "echo-cancel",
                                   NULL);

    g_object_set (self->audio_src,
                  "buffer-time", (gint64) 40000,
                  "stream-properties", gst_props,
                  NULL);
  }

  /* Audio sink */
  env_var = g_getenv ("CALLS_AUDIOSINK");
  if (!STR_IS_NULL_OR_EMPTY (env_var)) {
    MAKE_ELEMENT (audio_sink, env_var, "audiosink");
  } else {
    g_autoptr (GstStructure) gst_props = NULL;

    MAKE_ELEMENT (audio_sink, "pulsesink", "audiosink");

    /* enable echo cancellation and set buffer size to 40ms */
    gst_props = gst_structure_new ("props",
                                   "media.role", G_TYPE_STRING, "phone",
                                   "filter.want", G_TYPE_STRING, "echo-cancel",
                                   NULL);

    g_object_set (self->audio_sink,
                  "buffer-time", (gint64) 40000,
                  "stream-properties", gst_props,
                  NULL);

  }


  /* rtpbin */
  MAKE_ELEMENT (rtpbin, "rtpbin", "rtpbin");

  /* srtp elements */
  MAKE_ELEMENT (srtpdec, "srtpdec", "srtpdec");
  g_signal_connect (self->srtpdec,
                    "request-key",
                    G_CALLBACK (on_srtpdec_request_key),
                    self);

  MAKE_ELEMENT (srtpenc, "srtpenc", "srtpenc");

#if GST_CHECK_VERSION (1, 20, 0)
  tmppad = gst_element_request_pad_simple (self->srtpenc, "rtp_sink_0");
#else
  tmppad = gst_element_get_request_pad (self->srtpenc, "rtp_sink_0");
#endif
  gst_object_unref (tmppad);

#if GST_CHECK_VERSION (1, 20, 0)
  tmppad = gst_element_request_pad_simple (self->srtpenc, "rtcp_sink_0");
#else
  tmppad = gst_element_get_request_pad (self->srtpenc, "rtcp_sink_0");
#endif
  gst_object_unref (tmppad);


  self->request_rtpbin_rtp_encoder_id =
    g_signal_connect (self->rtpbin,
                      "request-rtp-encoder",
                      G_CALLBACK (on_rtpbin_request_encoder),
                      self);

  self->request_rtpbin_rtp_decoder_id =
    g_signal_connect (self->rtpbin,
                      "request-rtp-decoder",
                      G_CALLBACK (on_rtpbin_request_decoder),
                      self);

  self->request_rtpbin_rtcp_encoder_id =
    g_signal_connect (self->rtpbin,
                      "request-rtcp-encoder",
                      G_CALLBACK (on_rtpbin_request_encoder),
                      self);

  self->request_rtpbin_rtcp_decoder_id =
    g_signal_connect (self->rtpbin,
                      "request-rtcp-decoder",
                      G_CALLBACK (on_rtpbin_request_decoder),
                      self);

  /* UDP sources and sinks for RTP and RTCP */
  MAKE_ELEMENT (rtp_src, "udpsrc", "rtp-udp-src");
  MAKE_ELEMENT (rtp_sink, "udpsink", "rtp-udp-sink");

  MAKE_ELEMENT (rtcp_src, "udpsrc", "rtcp-udp-src");
  MAKE_ELEMENT (rtcp_sink, "udpsink", "rtcp-udp-sink");

  /* port 0 means letting the OS allocate */
  g_object_set (self->rtp_src, "port", 0, "address", "::", NULL);

  g_object_set (self->rtcp_src, "port", 0, "address", "::", NULL);

  g_object_set (self->rtp_sink, "async", FALSE, "sync", FALSE, NULL);
  g_object_set (self->rtcp_sink, "async", FALSE, "sync", FALSE, NULL);

  g_object_bind_property (self, "rport-rtp",
                          self->rtp_sink, "port",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "remote",
                          self->rtp_sink, "host",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "rport-rtcp",
                          self->rtcp_sink, "port",
                          G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (self, "remote",
                          self->rtcp_sink, "host",
                          G_BINDING_BIDIRECTIONAL);


  /* Add all elements to the pipeline */
  gst_bin_add_many (GST_BIN (self->pipeline),
                    self->audio_src, self->audio_sink,
                    self->rtpbin,
                    self->rtp_src, self->rtp_sink,
                    self->rtcp_src, self->rtcp_sink,
                    NULL);

  /* Setup bus watch */
  self->bus = gst_pipeline_get_bus (GST_PIPELINE (self->pipeline));
  self->bus_watch_id = gst_bus_add_watch (self->bus, on_bus_message, self);

  if (!setup_socket_reuse (self, error))
    return FALSE;

  return TRUE;
}


static gboolean
pipeline_link_elements (CallsSipMediaPipeline *self,
                        GError               **error)
{
  g_autoptr (GstPad) srcpad = NULL;
  g_autoptr (GstPad) sinkpad = NULL;
  GstPadLinkReturn ret;

  g_assert (CALLS_IS_SIP_MEDIA_PIPELINE (self));

  /* link to payloader */

#if GST_CHECK_VERSION (1, 20, 0)
  sinkpad = gst_element_request_pad_simple (self->rtpbin, "send_rtp_sink_0");
#else
  sinkpad = gst_element_get_request_pad (self->rtpbin, "send_rtp_sink_0");
#endif
  srcpad = gst_element_get_static_pad (self->payloader, "src");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link payloader to rtpbin");
    return FALSE;
  }


  /* Transmitter pads */

  srcpad = gst_element_get_static_pad (self->rtp_src, "src");
#if GST_CHECK_VERSION (1, 20, 0)
  sinkpad = gst_element_request_pad_simple (self->rtpbin, "recv_rtp_sink_0");
#else
  sinkpad = gst_element_get_request_pad (self->rtpbin, "recv_rtp_sink_0");
#endif
  ret = gst_pad_link (srcpad, sinkpad);
  if (ret != GST_PAD_LINK_OK) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link rtpsrc to rtpbin");
    return FALSE;
  }

  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  srcpad = gst_element_get_static_pad (self->rtpbin, "send_rtp_src_0");
  sinkpad = gst_element_get_static_pad (self->rtp_sink, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link rtpbin to rtpsink");
    return FALSE;
  }

  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);

  srcpad = gst_element_get_static_pad (self->rtcp_src, "src");
#if GST_CHECK_VERSION (1, 20, 0)
  sinkpad = gst_element_request_pad_simple (self->rtpbin, "recv_rtcp_sink_0");
#else
  sinkpad = gst_element_get_request_pad (self->rtpbin, "recv_rtcp_sink_0");
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
  srcpad = gst_element_request_pad_simple (self->rtpbin, "send_rtcp_src_0");
  #else
  srcpad = gst_element_get_request_pad (self->rtpbin, "send_rtcp_src_0");
  #endif
  sinkpad = gst_element_get_static_pad (self->rtcp_sink, "sink");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link rtpbin to rtcpsink");
    return FALSE;
  }

  /* can only link to depayloader after RTP payload has been verified */
  g_signal_connect (self->rtpbin, "pad-added", G_CALLBACK (on_pad_added), self->depayloader);

  /* request-encoder and request-decoder signals have been emitted after linking pads from rtpbin */
  if (self->request_rtpbin_rtp_decoder_id)
    g_signal_handler_disconnect (self->rtpbin, self->request_rtpbin_rtp_decoder_id);

  if (self->request_rtpbin_rtp_encoder_id)
    g_signal_handler_disconnect (self->rtpbin, self->request_rtpbin_rtp_encoder_id);

  if (self->request_rtpbin_rtcp_decoder_id)
    g_signal_handler_disconnect (self->rtpbin, self->request_rtpbin_rtcp_decoder_id);

  if (self->request_rtpbin_rtcp_encoder_id)
    g_signal_handler_disconnect (self->rtpbin, self->request_rtpbin_rtcp_encoder_id);

  return TRUE;
}


static gboolean
pipeline_setup_codecs (CallsSipMediaPipeline *self,
                       MediaCodecInfo        *codec,
                       GError               **error)
{
  g_autoptr (GstCaps) caps = NULL;
  g_autofree char *caps_string = NULL;

  g_assert (CALLS_IS_SIP_MEDIA_PIPELINE (self));
  g_assert (codec);

  MAKE_ELEMENT (decoder, codec->gst_decoder_name, "decoder");
  MAKE_ELEMENT (depayloader, codec->gst_depayloader_name, "depayloader");

  MAKE_ELEMENT (encoder, codec->gst_encoder_name, "encoder");
  MAKE_ELEMENT (payloader, codec->gst_payloader_name, "payloader");

  gst_bin_add_many (GST_BIN (self->pipeline),
                    self->depayloader, self->decoder,
                    self->payloader, self->encoder,
                    NULL);

  if (!gst_element_link_many (self->audio_src, self->encoder, self->payloader, NULL)) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link audiosrc encoder and payloader");
    return FALSE;
  }

  if (!gst_element_link_many (self->depayloader, self->decoder, self->audio_sink, NULL)) {
    if (error)
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to link depayloader decoder and audiosink");
    return FALSE;
  }

  /* UDP src capabilities */
  caps_string = media_codec_get_gst_capabilities (codec, self->use_srtp);
  g_debug ("Capabilities:\n%s", caps_string);

  caps = gst_caps_from_string (caps_string);

  /* set udp sinks and sources for RTP and RTCP */
  g_object_set (self->rtp_src,
                "caps", caps,
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

  if (!pipeline_init (self, &error)) {
    g_warning ("Could not create pipeline: %s", error->message);
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

  gst_object_unref (self->pipeline);
  gst_bus_remove_watch (self->bus);
  gst_object_unref (self->bus);
  gst_object_unref (self->srtpenc);
  gst_object_unref (self->srtpdec);

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
on_dump_data_written  (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      userdata)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *dot_data = userdata;
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);
  gsize n_expected = GPOINTER_TO_SIZE (dot_data);
  gsize n_written;

  n_written = g_output_stream_write_finish (stream, res, &error);
  if (n_written != n_expected) {
    /* this is not an error
     * TODO write the rest
     * note: maybe easier with GBytes because of keeping track of
     * dot_data reference ? */
    g_warning ("Expected to write %" G_GSIZE_FORMAT
               " but only wrote %" G_GSIZE_FORMAT " bytes",
               n_expected,
               n_written);
  }
  if (error) {
    g_warning ("Error while trying to dump dot graph to file: %s",
               error->message);
  }
}


static void
on_dump_file_created (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      userdata)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GFileOutputStream) stream = NULL;
  GFile *file = G_FILE (source_object);
  char *dot_data = userdata;


  stream = g_file_create_finish (G_FILE (source_object), res, &error);
  if (!stream) {
    g_autofree char *path = g_file_get_path (file);

    if (error->code == G_IO_ERROR_EXISTS) {
      /* we could potentially also g_file_replace() here,
       * but no immediate need to bother right now */
    }

    g_warning ("Cannot create file %s: %s", path, error->message);

    g_free (dot_data);
    return;
  }

  g_output_stream_write_async (G_OUTPUT_STREAM (stream),
                               dot_data,
                               strlen (dot_data),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               on_dump_data_written,
                               dot_data);
}



/* we need dump_pipeline_graph_to_path() because both
 * GST_DEBUG_BIN_TO_DOT_FILE*() and  gst_debug_bin_to_dot_file*()
 * require GStreamer being initialized with environment variable
 * GST_DEBUG_DOT_DIR set.
 */

static void
dump_pipeline_graph_to_path (GstBin     *bin,
                             const char *full_path)
{
  #ifdef CALLS_GST_DEBUG
  g_autoptr (GFile) file = NULL;
  char *dot_data;

  g_print ("Dumping pipeline graph to '%s'", full_path);

  dot_data = gst_debug_bin_to_dot_data (bin,
                                        GST_DEBUG_GRAPH_SHOW_VERBOSE);

  file = g_file_new_for_path (full_path);

  /* try creating, and if it fails with IO_ERROR_EXISTS try replacing the file */
  g_file_create_async (file,
                       G_PRIORITY_DEFAULT,
                       G_FILE_CREATE_PRIVATE,
                       NULL,
                       on_dump_file_created,
                       dot_data);
  #endif
}


static gboolean
usr2_handler (CallsSipMediaPipeline *self)
{
  g_autofree char *tmp_dir = NULL;
  g_autofree char *file_path = NULL;

  g_print ("playing: %d\n"
           "paused: %d\n"
           "stopped: %d\n"
           "target map: %d\n"
           "current state: %d\n",
           self->element_map_playing,
           self->element_map_paused,
           self->element_map_stopped,
           self->use_srtp ? EL_ALL_SRTP : EL_ALL_RTP,
           self->state);

  /* TODO once we require GLib >= 2.74
   * we can open a temp file more easily with g_file_new_tmp_async/finish()

     g_file_new_tmp_async ("calls-pipeline-XXXXXX",
                        G_PRIORITY_DEFAULT,
                        NULL,
                        on_dump_file_created,
                        dot_data);
   */
  tmp_dir = g_mkdtemp ("calls-pipeline-XXXXXX");
  file_path = g_strconcat (tmp_dir, G_DIR_SEPARATOR_S, "usr2-debug.dot", NULL);

  dump_pipeline_graph_to_path (GST_BIN (self->pipeline), file_path);

  return G_SOURCE_CONTINUE;
}


static void
calls_sip_media_pipeline_init (CallsSipMediaPipeline *self)
{
  if (!gst_is_initialized ())
    gst_init (NULL, NULL);

  /* Pipeline debugging */
  g_unix_signal_add (SIGUSR2,
                     (GSourceFunc) usr2_handler,
                     self);
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

  if (!pipeline_setup_codecs (self, codec, &error)) {
    g_warning ("Error trying to setup codecs for pipeline: %s",
               error->message);
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_ERROR);
    return;
  }

  if (!pipeline_link_elements (self, &error)) {
    g_warning ("Not all pads could be linked: %s",
               error->message);
    set_state (self, CALLS_MEDIA_PIPELINE_STATE_ERROR);
    return;
  }

  self->codec = codec;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CODEC]);

  set_state (self, CALLS_MEDIA_PIPELINE_STATE_READY);
}


void
calls_sip_media_pipeline_set_crypto (CallsSipMediaPipeline       *self,
                                     calls_srtp_crypto_attribute *crypto_own,
                                     calls_srtp_crypto_attribute *crypto_theirs)
{
  guchar *key_salt = NULL;
  gsize key_salt_length;
  GstSrtpCipherType srtp_cipher;
  GstSrtpAuthType srtp_auth;
  GstSrtpCipherType srtcp_cipher;
  GstSrtpAuthType srtcp_auth;

  g_autoptr (GstBuffer) key_buffer = NULL;

  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self));
  g_return_if_fail (crypto_own);
  g_return_if_fail (crypto_theirs);
  g_return_if_fail (crypto_own->crypto_suite == crypto_theirs->crypto_suite);
  g_return_if_fail (crypto_own->tag == crypto_theirs->tag);

  if (self->use_srtp)
    return;

  self->use_srtp = TRUE;
  self->crypto_own = crypto_own;
  self->crypto_theirs = crypto_theirs;

  if (!calls_srtp_crypto_get_srtpenc_params (crypto_own,
                                             &srtp_cipher,
                                             &srtp_auth,
                                             &srtcp_cipher,
                                             &srtcp_auth)) {
    g_autofree char *attr_str =
      calls_srtp_print_sdp_crypto_attribute (crypto_own, NULL);
    g_warning ("Could not get srtpenc parameters from attribute: %s", attr_str);
    return;
  }

  /* TODO MKI stuff */

  key_salt = g_base64_decode (crypto_own->key_params[0].b64_keysalt,
                              &key_salt_length);
  key_buffer = gst_buffer_new_wrapped (key_salt, key_salt_length);

  g_object_set (self->srtpenc,
                "key", key_buffer,
                "rtp-cipher", srtp_cipher,
                "rtp-auth", srtp_auth,
                "rtcp-cipher", srtcp_cipher,
                "rtcp-auth", srtcp_auth,
                NULL);
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
  } else {
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

  g_debug ("RTP/RTCP port before starting pipeline: %d/%d",
           calls_sip_media_pipeline_get_rtp_port (self),
           calls_sip_media_pipeline_get_rtcp_port (self));

  /* unlock the state of our udp sources, see setup_socket_reuse() */
  gst_element_set_locked_state (self->rtp_src, FALSE);
  gst_element_set_locked_state (self->rtcp_src, FALSE);

  gst_element_set_state (self->pipeline, GST_STATE_PLAYING);

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

  gst_element_set_locked_state (self->rtp_src, FALSE);
  gst_element_set_locked_state (self->rtcp_src, FALSE);
  gst_element_set_locked_state (self->rtp_sink, FALSE);
  gst_element_set_locked_state (self->rtcp_sink, FALSE);

  gst_element_set_state (self->pipeline, GST_STATE_NULL);

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


  /* leave udpsrc running to prevent timeouts */
  gst_element_set_locked_state (self->rtp_src, pause);
  gst_element_set_locked_state (self->rtcp_src, pause);
  gst_element_set_locked_state (self->rtp_sink, pause);
  gst_element_set_locked_state (self->rtcp_sink, pause);

  gst_element_set_state (self->pipeline, pause ?
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

  g_object_get (self->rtcp_src, "port", &port, NULL);

  return port;
}


CallsMediaPipelineState
calls_sip_media_pipeline_get_state (CallsSipMediaPipeline *self)
{
  g_return_val_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self),
                        CALLS_MEDIA_PIPELINE_STATE_UNKNOWN);

  return self->state;
}
