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

#include "calls-srtp-utils.h"
#include "gst-rfc3551.h"

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * CallsMediaPipelineState:
 * @CALLS_MEDIA_PIPELINE_STATE_UNKNOWN: Default state for new pipelines
 * @CALLS_MEDIA_PIPELINE_STATE_ERROR: Pipeline is in an error state
 * @CALLS_MEDIA_PIPELINE_STATE_INITIALIZING: Pipeline is initializing
 * @CALLS_MEDIA_PIPELINE_STATE_NEED_CODEC: Pipeline was initialized and needs a codec set
 * @CALLS_MEDIA_PIPELINE_STATE_READY: Pipeline is ready to be set into playing state
 * @CALLS_MEDIA_PIPELINE_STATE_PLAY_PENDING: Request to start pipeline pending
 * @CALLS_MEDIA_PIPELINE_STATE_PLAYING: Pipeline is currently playing
 * @CALLS_MEDIA_PIPELINE_STATE_PAUSE_PENDING: Request to pause pipeline pending
 * @CALLS_MEDIA_PIPELINE_STATE_PAUSED: Pipeline is currently paused
 * @CALLS_MEDIA_PIPELINE_STATE_STOP_PENDING: Request to stop pipeline pending
 * @CALLS_MEDIA_PIPELINE_STATE_STOPPED: Pipeline has stopped playing (f.e. received BYE packet)
 */
typedef enum {
  CALLS_MEDIA_PIPELINE_STATE_UNKNOWN = 0,
  CALLS_MEDIA_PIPELINE_STATE_ERROR,
  CALLS_MEDIA_PIPELINE_STATE_INITIALIZING,
  CALLS_MEDIA_PIPELINE_STATE_NEED_CODEC,
  CALLS_MEDIA_PIPELINE_STATE_READY,
  CALLS_MEDIA_PIPELINE_STATE_PLAY_PENDING,
  CALLS_MEDIA_PIPELINE_STATE_PLAYING,
  CALLS_MEDIA_PIPELINE_STATE_PAUSE_PENDING,
  CALLS_MEDIA_PIPELINE_STATE_PAUSED,
  CALLS_MEDIA_PIPELINE_STATE_STOP_PENDING,
  CALLS_MEDIA_PIPELINE_STATE_STOPPED
} CallsMediaPipelineState;


#define CALLS_TYPE_SIP_MEDIA_PIPELINE (calls_sip_media_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (CallsSipMediaPipeline, calls_sip_media_pipeline, CALLS, SIP_MEDIA_PIPELINE, GObject)


CallsSipMediaPipeline*  calls_sip_media_pipeline_new              (MediaCodecInfo *codec);
void                    calls_sip_media_pipeline_set_codec        (CallsSipMediaPipeline *self,
                                                                   MediaCodecInfo        *info);
void                    calls_sip_media_pipeline_set_crypto       (CallsSipMediaPipeline *self,
                                                                   calls_srtp_crypto_attribute *crypto_own,
                                                                   calls_srtp_crypto_attribute *crypto_theirs);
void                    calls_sip_media_pipeline_start            (CallsSipMediaPipeline *self);
void                    calls_sip_media_pipeline_stop             (CallsSipMediaPipeline *self);
void                    calls_sip_media_pipeline_pause            (CallsSipMediaPipeline *self,
                                                                   gboolean               pause);
int                     calls_sip_media_pipeline_get_rtp_port     (CallsSipMediaPipeline *self);
int                     calls_sip_media_pipeline_get_rtcp_port    (CallsSipMediaPipeline *self);
CallsMediaPipelineState calls_sip_media_pipeline_get_state        (CallsSipMediaPipeline *self);

G_END_DECLS
