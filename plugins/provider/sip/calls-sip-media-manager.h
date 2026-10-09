/*
 * Copyright (C) 2021-2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 */

#pragma once

#include "calls-sip-media-pipeline.h"

#include <sofia-sip/sdp.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_SIP_MEDIA_MANAGER (calls_sip_media_manager_get_type ())

G_DECLARE_FINAL_TYPE (CallsSipMediaManager, calls_sip_media_manager, CALLS, SIP_MEDIA_MANAGER, GObject);


CallsSipMediaManager  *calls_sip_media_manager_default                 (void);
GList                 *calls_sip_media_manager_codec_candidates        (CallsSipMediaManager *self);
CallsSipMediaPipeline *calls_sip_media_manager_get_pipeline            (CallsSipMediaManager *self);

G_END_DECLS
