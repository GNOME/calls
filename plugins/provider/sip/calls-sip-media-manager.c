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

#define G_LOG_DOMAIN "CallsSipMediaManager"

#include "calls-settings.h"
#include "calls-sip-media-manager.h"
#include "calls-sip-media-pipeline.h"
#include "calls-srtp-utils.h"
#include "gst-rfc3551.h"
#include "calls-util.h"

#include <gio/gio.h>
#include <gst/gst.h>

#include <sys/socket.h>


/**
 * SECTION:sip-media-manager
 * @short_description: The media manager singleton
 * @Title: CallsSipMediaManager
 *
 * #CallsSipMediaManager is mainly responsible for generating appropriate
 * SDP messages for the set of supported codecs. It also holds a list of
 * #CallsSipMediaPipeline objects that are ready to be used.
 */

typedef struct _CallsSipMediaManager {
  GObject                 parent;

  CallsSettings          *settings;
  GList                  *preferred_codecs;
  GListStore             *pipelines;
} CallsSipMediaManager;

G_DEFINE_TYPE (CallsSipMediaManager, calls_sip_media_manager, G_TYPE_OBJECT);


static void
on_notify_preferred_audio_codecs (CallsSipMediaManager *self)
{
  GList *supported_codecs;

  g_auto (GStrv) settings_codec_preference = NULL;

  g_assert (CALLS_IS_SIP_MEDIA_MANAGER (self));

  g_clear_list (&self->preferred_codecs, NULL);
  supported_codecs = media_codecs_get_candidates ();

  if (!supported_codecs) {
    g_warning ("There aren't any supported codecs installed on your system");
    return;
  }

  settings_codec_preference = calls_settings_get_preferred_audio_codecs (self->settings);

  if (!settings_codec_preference) {
    g_debug ("No audio codec preference set. Using all supported codecs");
    self->preferred_codecs = supported_codecs;
    return;
  }

  for (guint i = 0; settings_codec_preference[i] != NULL; i++) {
    MediaCodecInfo *codec = media_codec_by_name (settings_codec_preference[i]);

    if (!codec) {
      g_debug ("Did not find audio codec %s", settings_codec_preference[i]);
      continue;
    }
    if (media_codec_available_in_gst (codec))
      self->preferred_codecs = g_list_append (self->preferred_codecs, codec);
  }

  if (!self->preferred_codecs) {
    g_warning ("Cannot satisfy audio codec preference, "
               "falling back to all supported codecs");
    self->preferred_codecs = supported_codecs;
  } else {
    g_list_free (supported_codecs);
  }
}


static void
add_new_pipeline (CallsSipMediaManager *self)
{
  CallsSipMediaPipeline *pipeline;

  g_assert (CALLS_IS_SIP_MEDIA_MANAGER (self));

  pipeline = calls_sip_media_pipeline_new (NULL);
  g_list_store_append (self->pipelines, pipeline);
}


static void
calls_sip_media_manager_finalize (GObject *object)
{
  CallsSipMediaManager *self = CALLS_SIP_MEDIA_MANAGER (object);

  g_list_free (self->preferred_codecs);
  g_object_unref (self->pipelines);

  G_OBJECT_CLASS (calls_sip_media_manager_parent_class)->finalize (object);
}


static void
calls_sip_media_manager_class_init (CallsSipMediaManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = calls_sip_media_manager_finalize;
}


static void
calls_sip_media_manager_init (CallsSipMediaManager *self)
{
  if (!gst_is_initialized ())
    gst_init (NULL, NULL);

  self->settings = calls_settings_get_default ();
  g_signal_connect_swapped (self->settings,
                            "notify::preferred-audio-codecs",
                            G_CALLBACK (on_notify_preferred_audio_codecs),
                            self);
  on_notify_preferred_audio_codecs (self);

  self->pipelines = g_list_store_new (CALLS_TYPE_SIP_MEDIA_PIPELINE);

  add_new_pipeline (self);
}


/* Public functions */

CallsSipMediaManager *
calls_sip_media_manager_default (void)
{
  static CallsSipMediaManager *instance = NULL;

  if (instance == NULL) {
    g_debug ("Creating CallsSipMediaManager");
    instance = g_object_new (CALLS_TYPE_SIP_MEDIA_MANAGER, NULL);
    g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *) &instance);
  }
  return instance;
}


/* helper for calls_sip_media_manager_get_capabilities() */
static char *
get_connection_line (const char *ip)
{
  int af_family;

  if (STR_IS_NULL_OR_EMPTY(ip))
    goto invalid;

  af_family = get_address_family_for_ip (ip, TRUE);
  if (af_family == AF_UNSPEC)
    goto invalid;

  return g_strdup_printf ("c=IN %s %s\r\n",
                          af_family == AF_INET ? "IP4" : "IP6",
                          ip);

  invalid:
  return NULL;
}


/* calls_sip_media_manager_get_capabilities:
 *
 * @self: A #CallsSipMediaManager
 * @port: Should eventually come from the ICE stack
 * @crypto_attributes: A #GList of #calls_srtp_crypto_attribute
 * @supported_codecs: A #GList of #MediaCodecInfo
 *
 * Returns: (transfer full): string describing capabilities
 * to be used in the session description (SDP)
 */
char *
calls_sip_media_manager_get_capabilities (CallsSipMediaManager *self,
                                          const char           *own_ip,
                                          gint                  rtp_port,
                                          gint                  rtcp_port,
                                          GList                *crypto_attributes,
                                          GList                *supported_codecs)
{
  char *payload_type = crypto_attributes ? "SAVP" : "AVP";

  g_autoptr (GString) media_line = NULL;
  g_autoptr (GString) attribute_lines = NULL;
  g_autofree char *connection_line = NULL;
  GList *node;

  g_return_val_if_fail (CALLS_IS_SIP_MEDIA_MANAGER (self), NULL);

  media_line = g_string_new (NULL);
  attribute_lines = g_string_new (NULL);

  if (supported_codecs == NULL) {
    g_warning ("No supported codecs found. Can't build meaningful SDP message");
    g_string_append_printf (media_line, "m=audio 0 RTP/AVP");
    goto done;
  }

  /* media lines look f.e like "audio 31337 RTP/AVP 9 8 0" */
  g_string_append_printf (media_line,
                          "m=audio %d RTP/%s", rtp_port, payload_type);

  for (node = supported_codecs; node != NULL; node = node->next) {
    MediaCodecInfo *codec = node->data;

    g_string_append_printf (media_line, " %u", codec->payload_id);
    g_string_append_printf (attribute_lines,
                            "a=rtpmap:%u %s/%u%s",
                            codec->payload_id,
                            codec->name,
                            codec->clock_rate,
                            "\r\n");
  }

  for (node = crypto_attributes; node != NULL; node = node->next) {
    calls_srtp_crypto_attribute *attr = node->data;
    g_autoptr (GError) error = NULL;
    g_autofree char *crypto_line =
      calls_srtp_print_sdp_crypto_attribute (attr, &error);

    if (!crypto_line) {
      g_warning ("Could not print SDP crypto line for tag %d: %s", attr->tag, error->message);
      continue;
    }
    g_string_append_printf (attribute_lines, "%s\r\n", crypto_line);
  }

  g_string_append_printf (attribute_lines, "a=rtcp:%d\r\n", rtcp_port);

done:
  connection_line = get_connection_line (own_ip);

  return g_strdup_printf ("v=0\r\n"
                          "%s"
                          "%s\r\n"
                          "%s\r\n",
                          connection_line ?: "",
                          media_line->str,
                          attribute_lines->str);
}


/* calls_sip_media_manager_static_capabilities:
 *
 * @self: A #CallsSipMediaManager
 * @rtp_port: Port to use for RTP. Should eventually come from the ICE stack
 * @rtcp_port: Port to use for RTCP.Should eventually come from the ICE stack
 * @crypto_attributes: A #GList of #calls_srtp_crypto_attribute
 *
 * Returns: (transfer full): string describing capabilities
 * to be used in the session description (SDP)
 */
char *
calls_sip_media_manager_static_capabilities (CallsSipMediaManager *self,
                                             const char           *own_ip,
                                             gint                  rtp_port,
                                             gint                  rtcp_port,
                                             GList                *crypto_attributes)
{
  g_return_val_if_fail (CALLS_IS_SIP_MEDIA_MANAGER (self), NULL);

  return calls_sip_media_manager_get_capabilities (self,
                                                   own_ip,
                                                   rtp_port,
                                                   rtcp_port,
                                                   crypto_attributes,
                                                   self->preferred_codecs);
}


/* calls_sip_media_manager_codec_candiates:
 *
 * @self: A #CallsSipMediaManager
 *
 * Returns: (transfer none): A #GList of supported
 * #MediaCodecInfo
 */
GList *
calls_sip_media_manager_codec_candidates (CallsSipMediaManager *self)
{
  g_return_val_if_fail (CALLS_IS_SIP_MEDIA_MANAGER (self), NULL);

  return self->preferred_codecs;
}


/* calls_sip_media_manager_get_codecs_from_sdp
 *
 * @self: A #CallsSipMediaManager
 * @sdp: A #sdp_media_t media description
 *
 * Returns: (transfer container): A #GList of codecs found in the
 * SDP message
 */
GList *
calls_sip_media_manager_get_codecs_from_sdp (CallsSipMediaManager *self,
                                             sdp_media_t          *sdp_media)
{
  GList *codecs = NULL;
  sdp_rtpmap_t *rtpmap = NULL;

  g_return_val_if_fail (CALLS_IS_SIP_MEDIA_MANAGER (self), NULL);
  g_return_val_if_fail (sdp_media, NULL);

  if (sdp_media->m_type != sdp_media_audio) {
    g_warning ("Only the 'audio' media type is supported");
    return NULL;
  }

  for (rtpmap = sdp_media->m_rtpmaps; rtpmap != NULL; rtpmap = rtpmap->rm_next) {
    MediaCodecInfo *codec = media_codec_by_payload_id (rtpmap->rm_pt);
    if (codec)
      codecs = g_list_append (codecs, codec);
  }

  if (sdp_media->m_next != NULL)
    g_warning ("Currently only a single media session is supported");

  if (codecs == NULL)
    g_warning ("Did not find any common codecs");

  return codecs;
}

/**
 * calls_sip_media_manager_get_pipeline:
 * @self: A #CallsSipMediaManager
 *
 * Returns: (transfer full): A #CallsSipMediaPipeline
 */
CallsSipMediaPipeline *
calls_sip_media_manager_get_pipeline (CallsSipMediaManager *self)
{
  g_autoptr (CallsSipMediaPipeline) pipeline = NULL;

  g_return_val_if_fail (CALLS_IS_SIP_MEDIA_MANAGER (self), NULL);

  pipeline = g_list_model_get_item (G_LIST_MODEL (self->pipelines), 0);

  g_list_store_remove (self->pipelines, 0);

  /* add a pipeline for the one we just removed */
  add_new_pipeline (self);

  return pipeline;
}
