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

#include "gst-rfc3551.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_SIP_MEDIA_PIPELINE (calls_sip_media_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (CallsSipMediaPipeline, calls_sip_media_pipeline, CALLS, SIP_MEDIA_PIPELINE, GObject)


CallsSipMediaPipeline* calls_sip_media_pipeline_new                    (MediaCodecInfo *codec);
void                   calls_sip_media_pipeline_set_codec              (CallsSipMediaPipeline *self,
                                                                        MediaCodecInfo        *info);
void                   calls_sip_media_pipeline_start                  (CallsSipMediaPipeline *self);
void                   calls_sip_media_pipeline_stop                   (CallsSipMediaPipeline *self);
void                   calls_sip_media_pipeline_pause                  (CallsSipMediaPipeline *self,
                                                                        gboolean               pause);

G_END_DECLS
