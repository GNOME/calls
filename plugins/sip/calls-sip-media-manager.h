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

#include <sofia-sip/nua.h>
#include <sofia-sip/sdp.h>
#include <glib-object.h>

#define CALLS_TYPE_SIP_MEDIA_MANAGER (calls_sip_media_manager_get_type ())

G_DECLARE_FINAL_TYPE (CallsSipMediaManager, calls_sip_media_manager, CALLS, SIP_MEDIA_MANAGER, GObject)


CallsSipMediaManager* calls_sip_media_manager_default                       (void);
gchar*                calls_sip_media_manager_get_capabilities              (CallsSipMediaManager *self,
                                                                             guint port,
                                                                             gboolean use_srtp,
                                                                             GList *supported_codecs);
gchar*                calls_sip_media_manager_static_capabilities           (CallsSipMediaManager *self,
                                                                             guint port,
                                                                             gboolean use_srtp);
gboolean              calls_sip_media_manager_supports_media                (CallsSipMediaManager *self,
                                                                             const char *media_type);
MediaCodecInfo*       get_best_codec                                        (CallsSipMediaManager *self);
GList *               calls_sip_media_manager_codec_candidates              (CallsSipMediaManager *self);
GList *               calls_sip_media_manager_get_codecs_from_sdp           (CallsSipMediaManager *self,
                                                                             sdp_media_t          *sdp_media);
