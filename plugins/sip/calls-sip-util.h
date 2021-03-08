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

typedef struct
{
  su_home_t             home[1];
  su_root_t            *root;
} CallsSipContext;

typedef struct
{
  nua_handle_t        *register_handle;
  nua_handle_t        *call_handle;
  CallsSipContext     *context;
} CallsSipHandles;


/**
 * SipEngineState:
 * @SIP_ENGINE_NULL: Not initialized
 * @SIP_ENGINE_INITIALIZING: Need to complete initialization
 * @SIP_ENGINE_ERROR: Unrecoverable/Unhandled sofia-sip error
 * @SIP_ENGINE_READY: Ready for operation
 */
typedef enum
  {
    SIP_ENGINE_NULL = 0,
    SIP_ENGINE_INITIALIZING,
    SIP_ENGINE_ERROR,
    SIP_ENGINE_READY,
  } SipEngineState;

/**
 * SipAccountState:
 * @SIP_ACCOUNT_NULL: Not initialized
 * @SIP_ACCOUNT_OFFLINE: Account considered offline (not ready for placing or receiving calls)
 * @SIP_ACCOUNT_REGISTERING: REGISTER sent
 * @SIP_ACCOUNT_AUTHENTICATING: Authenticating using web-auth/proxy-auth
 * @SIP_ACCOUNT_ERROR: Unrecoverable error
 * @SIP_ACCOUNT_ERROR_RETRY: Recoverable error (f.e. using other credentials)
 * @SIP_ACCOUNT_ONLINE: Account considered online (can place and receive calls)
 */
typedef enum
  {
    SIP_ACCOUNT_NULL = 0,
    SIP_ACCOUNT_OFFLINE,
    SIP_ACCOUNT_REGISTERING,
    SIP_ACCOUNT_AUTHENTICATING,
    SIP_ACCOUNT_ERROR,
    SIP_ACCOUNT_ERROR_RETRY,
    SIP_ACCOUNT_ONLINE,
  } SipAccountState;


gboolean check_sips (const gchar *addr);
gboolean check_ipv6 (const gchar *host);
const gchar *get_protocol_prefix (const gchar *protocol);
gboolean protocol_is_valid (const gchar *protocol);
guint get_port_for_rtp (void);
