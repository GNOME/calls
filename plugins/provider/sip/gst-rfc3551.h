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

#pragma once

#include <glib.h>

/*
 * For more information
 * see: https://tools.ietf.org/html/rfc3551#section-6
 */

typedef struct {
  guint payload_id;
  char *name;
  gint  clock_rate;
  gint  channels;
  char *gst_payloader_name;
  char *gst_depayloader_name;
  char *gst_encoder_name;
  char *gst_decoder_name;
  char *filename;
} MediaCodecInfo;


gboolean        media_codec_available_in_gst (MediaCodecInfo *codec);
MediaCodecInfo *media_codec_by_name (const char *name);
MediaCodecInfo *media_codec_by_payload_id (uint payload_id);
gchar          *media_codec_get_gst_capabilities (MediaCodecInfo *codec,
                                                  gboolean        use_srtp);
GList          *media_codecs_get_candidates (void);
