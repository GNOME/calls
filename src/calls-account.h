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

G_DECLARE_INTERFACE (CallsAccount, calls_account, CALLS, ACCOUNT, CallsOrigin)

/**
 * CallsAccountState:
 * @CALLS_ACCOUNT_STATE_UNKNOWN: Default state for new accounts
 * @CALLS_ACCOUNT_STATE_INITIALIZING: Account is initializing
 * @CALLS_ACCOUNT_STATE_DEINITIALIZING: Account is being shutdown
 * @CALLS_ACCOUNT_STATE_CONNECTING: Connecting to server
 * @CALLS_ACCOUNT_STATE_ONLINE: Account is online
 * @CALLS_ACCOUNT_STATE_DISCONNECTING: Disconnecting from server
 * @CALLS_ACCOUNT_STATE_OFFLINE: Account is offline
 * @CALLS_ACCOUNT_STATE_ERROR: Account is in an error state
 */
typedef enum {
  CALLS_ACCOUNT_STATE_UNKNOWN = 0,
  CALLS_ACCOUNT_STATE_INITIALIZING,
  CALLS_ACCOUNT_STATE_DEINITIALIZING,
  CALLS_ACCOUNT_STATE_CONNECTING,
  CALLS_ACCOUNT_STATE_ONLINE,
  CALLS_ACCOUNT_STATE_DISCONNECTING,
  CALLS_ACCOUNT_STATE_OFFLINE,
  CALLS_ACCOUNT_STATE_ERROR,
} CallsAccountState;

/**
 * CallsAccountStateReason:
 * @CALLS_ACCOUNT_STATE_REASON_UNKNOWN: Default unspecified reason
 * @CALLS_ACCOUNT_STATE_REASON_INITIALIZATION_STARTED: Initialization started
 * @CALLS_ACCOUNT_STATE_REASON_INITIALIZED: Initialization done
 * @CALLS_ACCOUNT_STATE_REASON_DEINITIALIZATION_STARTED: Deinitialization started
 * @CALLS_ACCOUNT_STATE_REASON_DEINITIALIZED: Deinitialization done
 * @CALLS_ACCOUNT_STATE_REASON_NO_CREDENTIALS: No credentials were set
 * @CALLS_ACCOUNT_STATE_REASON_CONNECT_STARTED: Starting to connect
 * @CALLS_ACCOUNT_STATE_REASON_CONNECTION_TIMEOUT: A connection has timed out
 * @CALLS_ACCOUNT_STATE_REASON_CONNECTION_DNS_ERROR: A domain name could not be resolved
 * @CALLS_ACCOUNT_STATE_REASON_AUTHENTICATION_FAILURE: Could not authenticate, possibly wrong credentials
 * @CALLS_ACCOUNT_STATE_REASON_CONNECTED: Connected successfully
 * @CALLS_ACCOUNT_STATE_REASON_DISCONNECTED: Disconnected successfully
 * @CALLS_ACCOUNT_STATE_REASON_INTERNAL_ERROR: An internal error has occurred
 */
typedef enum {
  CALLS_ACCOUNT_STATE_REASON_UNKNOWN = 0,
  CALLS_ACCOUNT_STATE_REASON_INITIALIZATION_STARTED,
  CALLS_ACCOUNT_STATE_REASON_INITIALIZED,
  CALLS_ACCOUNT_STATE_REASON_DEINITIALIZATION_STARTED,
  CALLS_ACCOUNT_STATE_REASON_DEINITIALIZED,
  CALLS_ACCOUNT_STATE_REASON_NO_CREDENTIALS,
  CALLS_ACCOUNT_STATE_REASON_CONNECT_STARTED,
  CALLS_ACCOUNT_STATE_REASON_CONNECTION_TIMEOUT,
  CALLS_ACCOUNT_STATE_REASON_CONNECTION_DNS_ERROR,
  CALLS_ACCOUNT_STATE_REASON_AUTHENTICATION_FAILURE,
  CALLS_ACCOUNT_STATE_REASON_CONNECTED,
  CALLS_ACCOUNT_STATE_REASON_DISCONNECT_STARTED,
  CALLS_ACCOUNT_STATE_REASON_DISCONNECTED,
  CALLS_ACCOUNT_STATE_REASON_INTERNAL_ERROR,
} CallsAccountStateReason;


struct _CallsAccountInterface {
  GTypeInterface parent_iface;

  void           (*go_online)                           (CallsAccount *self,
                                                         gboolean      online);
  const char    *(*get_address)                         (CallsAccount *self);
};


void              calls_account_go_online                     (CallsAccount *self,
                                                               gboolean      online);
const char       *calls_account_get_address                   (CallsAccount *self);
CallsAccountState calls_account_get_state                     (CallsAccount *self);
const char       *calls_account_state_to_string               (CallsAccountState state);
const char       *calls_account_state_reason_to_string        (CallsAccountStateReason reason);
void              calls_account_emit_message_for_state_change (CallsAccount           *account,
                                                               CallsAccountState       new_state,
                                                               CallsAccountStateReason reason);
gboolean          calls_account_state_is_for_ui               (CallsAccountState state);
gboolean          calls_account_state_reason_is_for_ui        (CallsAccountStateReason reason);

G_END_DECLS
