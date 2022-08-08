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

#define G_LOG_DOMAIN "CallsGstRfc3551"

#include "gst-rfc3551.h"

#include <glib.h>
#include <gst/gst.h>

/* Use the following codecs in order of preference */
static MediaCodecInfo gst_codecs[] = {
  {8, "PCMA", 8000, 1, "rtppcmapay", "rtppcmadepay", "alawenc", "alawdec", "libgstalaw.so"},
  {0, "PCMU", 8000, 1, "rtppcmupay", "rtppcmudepay", "mulawenc", "mulawdec", "libgstmulaw.so"},
  {3, "GSM", 8000, 1, "rtpgsmpay", "rtpgsmdepay", "gsmenc", "gsmdec", "libgstgsm.so"},
  {9, "G722", 8000, 1, "rtpg722pay", "rtpg722depay", "avenc_g722", "avdec_g722", "libgstlibav.so"},
  {4, "G723", 8000, 1, "rtpg723pay", "rtpg723depay", "avenc_g723_1", "avdec_g723_1", "libgstlibav.so"}, // does not seem to work
};


/**
 * media_codec_available_in_gst:
 * @codec: A #MediaCodecInfo
 *
 * Returns: %TRUE if codec is available on your system, %FALSE otherwise
 */
gboolean
media_codec_available_in_gst (MediaCodecInfo *codec)
{
  gboolean available = FALSE;
  GstRegistry *registry = gst_registry_get ();
  GstPlugin *plugin = NULL;

  plugin = gst_registry_lookup (registry, codec->filename);
  available = !!plugin;

  if (plugin)
    gst_object_unref (plugin);

  g_debug ("Gstreamer plugin for %s %s available",
           codec->name, available ? "is" : "is not");
  return available;
}

/* media_codec_by_name:
 *
 * @name: The name of the codec
 *
 * Returns: (transfer none): A #MediaCodecInfo, if found.
 * You should check if the codec is available on your system before
 * trying to use it with media_codec_available_in_gst()
 */
MediaCodecInfo *
media_codec_by_name (const char *name)
{
  g_return_val_if_fail (name, NULL);

  for (guint i = 0; i < G_N_ELEMENTS (gst_codecs); i++) {
    if (g_strcmp0 (name, gst_codecs[i].name) == 0)
      return &gst_codecs[i];
  }

  return NULL;
}

/* media_codec_by_payload_id:
 *
 * @payload_id: The payload id (see RFC 3551, 3555, 4733, 4855)
 *
 * Returns: (transfer none): A #MediaCodecInfo, if found.
 * You should check if the codec is available on your system before
 * trying to use it with media_codec_available_in_gst()
 */
MediaCodecInfo *
media_codec_by_payload_id (guint payload_id)
{
  for (guint i = 0; i < G_N_ELEMENTS (gst_codecs); i++) {
    if (payload_id == gst_codecs[i].payload_id)
      return &gst_codecs[i];
  }

  return NULL;
}

/* media_codec_get_gst_capabilities:
 *
 * @codec: A #MediaCodecInfo
 * @use_srtp: Whether to use SRTP
 *
 * Returns: (transfer full): The capability string describing GstCaps.
 * Used for the RTP source element.
 */
gchar *
media_codec_get_gst_capabilities (MediaCodecInfo *codec,
                                  gboolean        use_srtp)
{
  return g_strdup_printf ("application/%s,media=(string)audio,clock-rate=(int)%u"
                          ",encoding-name=(string)%s,payload=(int)%u",
                          use_srtp ? "x-srtp" : "x-rtp",
                          codec->clock_rate,
                          codec->name,
                          codec->payload_id);
}

/* media_codecs_get_candidates:
 *
 * Returns: (transfer container): A #GList of codec candidates of type #MediaCodecInfo.
 * Free the list with g_list_free when done.
 */
GList *
media_codecs_get_candidates (void)
{
  GList *candidates = NULL;

  for (guint i = 0; i < G_N_ELEMENTS (gst_codecs); i++) {
    if (media_codec_available_in_gst (&gst_codecs[i])) {
      g_debug ("Adding %s to the codec candidates", gst_codecs[i].name);
      candidates = g_list_append (candidates, &gst_codecs[i]);
    }
  }

  return candidates;
}
