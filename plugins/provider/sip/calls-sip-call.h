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

#include "calls-call.h"
#include "calls-sdp-crypto-context.h"
#include "calls-sip-media-pipeline.h"
#include "calls-sip-util.h"

#include <glib-object.h>
#include <sofia-sip/nua.h>

G_BEGIN_DECLS

#define CALLS_TYPE_SIP_CALL (calls_sip_call_get_type ())

G_DECLARE_FINAL_TYPE (CallsSipCall, calls_sip_call, CALLS, SIP_CALL, CallsCall)

CallsSipCall          *calls_sip_call_new                               (const char            *number,
                                                                         gboolean               inbound,
                                                                         const char            *own_ip,
                                                                         CallsSipMediaPipeline *pipeline,
                                                                         SipMediaEncryption     encryption,
                                                                         nua_handle_t          *handle);
void                   calls_sip_call_setup_remote_media_connection     (CallsSipCall *self,
                                                                         const char   *remote,
                                                                         guint         port_rtp,
                                                                         guint         port_rtcp);
void                   calls_sip_call_activate_media                    (CallsSipCall *self,
                                                                         gboolean      enabled);
void                   calls_sip_call_set_state                         (CallsSipCall  *self,
                                                                         CallsCallState state);
void                   calls_sip_call_set_codecs                        (CallsSipCall *self,
                                                                         GList        *codecs);
CallsSdpCryptoContext *calls_sip_call_get_sdp_crypto_context            (CallsSipCall *self);

G_END_DECLS
