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

#include <sofia-sip/nua.h>
#include <glib-object.h>

typedef struct {
  su_home_t  home[1];
  su_root_t *root;
} CallsSipContext;

typedef struct {
  nua_handle_t    *register_handle;
  nua_handle_t    *call_handle;
  CallsSipContext *context;
} CallsSipHandles;


/**
 * SipEngineState:
 * @SIP_ENGINE_NULL: Not initialized
 * @SIP_ENGINE_INITIALIZING: Need to complete initialization
 * @SIP_ENGINE_ERROR: Unrecoverable/Unhandled sofia-sip error
 * @SIP_ENGINE_READY: Ready for operation
 */
typedef enum {
  SIP_ENGINE_NULL = 0,
  SIP_ENGINE_INITIALIZING,
  SIP_ENGINE_ERROR,
  SIP_ENGINE_READY,
} SipEngineState;

/**
 * SipMediaEncryption:
 * @SIP_MEDIA_ENCRYPTION_NONE: Don't encrypt media streams
 * @SIP_MEDIA_ENCRYPTION_PREFERRED: Prefer using encryption, but also allow unencrypted media
 * @SIP_MEDIA_ENCRYPTION_FORCED: Force using encryption, drop unencrypted calls
 */
typedef enum {
  SIP_MEDIA_ENCRYPTION_NONE = 0,
  SIP_MEDIA_ENCRYPTION_PREFERRED,
  SIP_MEDIA_ENCRYPTION_FORCED,
} SipMediaEncryption;


gboolean    check_sips                         (const char *addr);
gboolean    check_ipv6                         (const char *host);
const char *get_protocol_prefix                (const char *protocol);
gboolean    protocol_is_valid                  (const char *protocol);
