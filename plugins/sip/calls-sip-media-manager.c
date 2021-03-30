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

#define G_LOG_DOMAIN "CallsCallsSipMediaManager"

#include "calls-sip-media-pipeline.h"
#include "gst-rfc3551.h"
#include "calls-sip-media-manager.h"

#include <gst/gst.h>

typedef struct _CallsSipMediaManager
{
  GObject parent;
} CallsSipMediaManager;

G_DEFINE_TYPE (CallsSipMediaManager, calls_sip_media_manager, G_TYPE_OBJECT);


static void
calls_sip_media_manager_finalize (GObject *object)
{
  gst_deinit ();

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
  gst_init (NULL, NULL);
}


/* Public functions */

CallsSipMediaManager *
calls_sip_media_manager_default ()
{
  static CallsSipMediaManager *instance = NULL;

  if (instance == NULL) {
    g_debug ("Creating CallsSipMediaManager");
    instance = g_object_new (CALLS_TYPE_SIP_MEDIA_MANAGER, NULL);
    g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *) &instance);
  }
  return instance;
}


/* calls_sip_media_manager_static_capabilities:
 *
 * @port: Should eventually come from the ICE stack
 * @use_srtp: Whether to use srtp (not really handled)
 *
 * Returns: (full-control) string describing capabilities
 * to be used in the session description (SDP)
 */
char *
calls_sip_media_manager_static_capabilities (CallsSipMediaManager *self,
                                             guint                 port,
                                             gboolean              use_srtp)
{
  char *payload_type = use_srtp ? "SAVP" : "AVP";
  g_autofree char *media_line = NULL;
  g_autofree char *attribute_line = NULL;
  MediaCodecInfo *codec;

  g_return_val_if_fail (CALLS_IS_SIP_MEDIA_MANAGER (self), NULL);

  codec = get_best_codec (self);
  /* TODO support multiplice codecs: f.e. audio 31337 RTP/AVP 9 8 0 96 */
  media_line = g_strdup_printf ("audio %d RTP/%s %s",
                                port, payload_type, codec->payload_id);
  attribute_line = g_strdup_printf ("rtpmap:%s %s/%s",
                                    codec->payload_id, codec->name, codec->clock_rate);

  /* TODO add attribute describing RTCP stream */
  return g_strdup_printf ("v=0\r\n"
                          "m=%s\r\n"
                          "a=%s\r\n",
                          media_line,
                          attribute_line);
}


/* TODO lookup plugins in GStreamer */
gboolean
calls_sip_media_manager_supports_media (CallsSipMediaManager *self,
                                        const char *media_type)
{
  return TRUE;
}


MediaCodecInfo*
get_best_codec (CallsSipMediaManager *self)
{
  return media_codec_by_name ("PCMA");
}
