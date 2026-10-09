/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
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
