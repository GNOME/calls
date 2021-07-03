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

#include "calls-origin.h"

#include <glib-object.h>

G_BEGIN_DECLS


#define CALLS_TYPE_ACCOUNT (calls_account_get_type ())

G_DECLARE_INTERFACE (CallsAccount, calls_account, CALLS, ACCOUNT, CallsOrigin);


struct _CallsAccountInterface
{
  GTypeInterface parent_iface;

  void               (*go_online)                           (CallsAccount *self,
                                                             gboolean      online);
  const char        *(*get_address)                         (CallsAccount *self);
};
/**
 * CallsAccountState:
 * @CALLS_ACCOUNT_NULL: Not initialized or error
 * @CALLS_ACCOUNT_OFFLINE: Account considered offline (not ready for placing or receiving calls)
 * @CALLS_ACCOUNT_CONNECTING: Trying to connect to server
 * @CALLS_ACCOUNT_CONNECTION_FAILURE: Could not connect to server (f.e. DNS error, unreachable)
 * @CALLS_ACCOUNT_AUTHENTICATING: Authenticating using web-auth/proxy-auth
 * @CALLS_ACCOUNT_AUTHENTICATION_FAILURE: Could not authenticate to server (f.e. wrong credentials)
 * @CALLS_ACCOUNT_NO_CREDENTIALS: Credentials are missing
 * @CALLS_ACCOUNT_ONLINE: Account considered online (can place and receive calls)
 */
typedef enum {
  CALLS_ACCOUNT_NULL = 0,
  CALLS_ACCOUNT_OFFLINE,
  CALLS_ACCOUNT_CONNECTING,
  CALLS_ACCOUNT_CONNECTION_FAILURE,
  CALLS_ACCOUNT_AUTHENTICATING,
  CALLS_ACCOUNT_AUTHENTICATION_FAILURE,
  CALLS_ACCOUNT_NO_CREDENTIALS,
  CALLS_ACCOUNT_UNKNOWN_ERROR,
  CALLS_ACCOUNT_ONLINE
} CallsAccountState;


void                   calls_account_go_online          (CallsAccount *self,
                                                         gboolean      online);
const char            *calls_account_get_address        (CallsAccount *self);
CallsAccountState      calls_account_get_state          (CallsAccount *self);

G_END_DECLS
