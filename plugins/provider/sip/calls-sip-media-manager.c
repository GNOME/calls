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

  g_clear_list (&self->preferred_codecs, NULL);
  g_clear_object (&self->pipelines);

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
