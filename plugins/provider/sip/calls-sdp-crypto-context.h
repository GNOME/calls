/*
 * Copyright (C) 2022 Purism SPC
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

#include <glib-object.h>

#include <sofia-sip/sdp.h>

G_BEGIN_DECLS

/** XXX media line with cryptographic key parameters or session parameters that we
 * do not support MUST be rejected (https://datatracker.ietf.org/doc/html/rfc4568#section-7.1.2)
 */

typedef enum {
  CALLS_CRYPTO_CONTEXT_STATE_INIT = 0,
  CALLS_CRYPTO_CONTEXT_STATE_OFFER_LOCAL,
  CALLS_CRYPTO_CONTEXT_STATE_OFFER_REMOTE,
  CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_FAILED,
  CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_SUCCESS
} CallsCryptoContextState;


#define CALLS_TYPE_SDP_CRYPTO_CONTEXT (calls_sdp_crypto_context_get_type ())

G_DECLARE_FINAL_TYPE (CallsSdpCryptoContext, calls_sdp_crypto_context, CALLS, SDP_CRYPTO_CONTEXT, GObject);

CallsSdpCryptoContext       *calls_sdp_crypto_context_new                   (void);
gboolean                     calls_sdp_crypto_context_set_local_media       (CallsSdpCryptoContext *self,
                                                                             sdp_media_t           *media);
gboolean                     calls_sdp_crypto_context_set_remote_media      (CallsSdpCryptoContext *self,
                                                                             sdp_media_t           *media);
calls_srtp_crypto_attribute *calls_sdp_crypto_context_get_local_crypto      (CallsSdpCryptoContext *self);
calls_srtp_crypto_attribute *calls_sdp_crypto_context_get_remote_crypto     (CallsSdpCryptoContext *self);
gboolean                     calls_sdp_crypto_context_generate_offer        (CallsSdpCryptoContext *self);
gboolean                     calls_sdp_crypto_context_generate_answer       (CallsSdpCryptoContext *self);
gboolean                     calls_sdp_crypto_context_get_is_negotiated     (CallsSdpCryptoContext *self);
CallsCryptoContextState      calls_sdp_crypto_context_get_state             (CallsSdpCryptoContext *self);
GList                       *calls_sdp_crypto_context_get_crypto_candidates (CallsSdpCryptoContext *self,
                                                                             gboolean               remote);

G_END_DECLS
