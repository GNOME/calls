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

#include "calls-account.h"
#include "calls-message-source.h"
#include "calls-log.h"

#include "enum-types.h"

#include <glib/gi18n.h>

/**
 * SECTION:account
 * @short_description: An interface for online accounts
 * @Title: CallsAccount
 *
 * #CallsAccount is a type of #CallsOrigin for online accounts.
 */

G_DEFINE_INTERFACE (CallsAccount, calls_account, CALLS_TYPE_ORIGIN)

enum {
  SIGNAL_STATE_CHANGED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];


static void
calls_account_default_init (CallsAccountInterface *iface)
{
  g_object_interface_install_property (
    iface,
    g_param_spec_enum ("account-state",
                       "Account state",
                       "The state of the account",
                       CALLS_TYPE_ACCOUNT_STATE,
                       CALLS_ACCOUNT_STATE_UNKNOWN,
                       G_PARAM_READABLE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY));

  g_object_interface_install_property (
    iface,
    g_param_spec_string ("address",
                         "Address",
                         "The address of this account",
                         NULL,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY));

  /**
   * CallsAccount::account-state-changed:
   * @self: The #CallsAccount
   * @new_state: The new #CALLS_ACCOUNT_STATE of the account
   * @old_state: The old #CALLS_ACCOUNT_STATE of the account
   * @reason: The #CALLS_ACCOUNT_STATE_REASON for the change
   */
  signals[SIGNAL_STATE_CHANGED] =
    g_signal_new ("account-state-changed",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  3,
                  CALLS_TYPE_ACCOUNT_STATE,
                  CALLS_TYPE_ACCOUNT_STATE,
                  CALLS_TYPE_ACCOUNT_STATE_REASON);
}

/**
 * calls_account_go_online:
 * @self: a #CallsAccount
 * @online: %TRUE to try to go online, %FALSE to go offline
 *
 * Connect or disconnect to a server.
 */
void
calls_account_go_online (CallsAccount *self,
                         gboolean      online)
{
  CallsAccountInterface *iface;

  g_return_if_fail (CALLS_IS_ACCOUNT (self));

  iface = CALLS_ACCOUNT_GET_IFACE (self);
  g_return_if_fail (iface->go_online != NULL);

  return iface->go_online (self, online);
}

/**
 * calls_account_get_state:
 * @self: A #CallsAccount
 *
 * Returns: The current #CallsAccountState of this account
 */
CallsAccountState
calls_account_get_state (CallsAccount *self)
{
  CallsAccountState state;

  g_return_val_if_fail (CALLS_IS_ACCOUNT (self), CALLS_ACCOUNT_STATE_UNKNOWN);

  g_object_get (self, "account-state", &state, NULL);

  return state;
}

/**
 * calls_account_get_address:
 * @self: A #CallsAccount
 *
 * Returns: The address under which this account can be reached.
 * For example: alice@example.org for SIP and XMPP/Jingle or @alice:example.org for Matrix
 */
const char *
calls_account_get_address (CallsAccount *self)
{
  CallsAccountInterface *iface;

  g_return_val_if_fail (CALLS_IS_ACCOUNT (self), NULL);

  iface = CALLS_ACCOUNT_GET_IFACE (self);
  g_return_val_if_fail (iface->get_address, NULL);

  return iface->get_address (self);
}

/**
 * calls_account_state_to_string:
 * @state: A #CallsAccountState
 *
 * Returns: (transfer none): A human readable description of the account state
 */
const char *
calls_account_state_to_string (CallsAccountState state)
{
  switch (state) {
  case CALLS_ACCOUNT_STATE_UNKNOWN:
    return _("Default (uninitialized) state");

  case CALLS_ACCOUNT_STATE_INITIALIZING:
    return _("Initializing account…");

  case CALLS_ACCOUNT_STATE_DEINITIALIZING:
    return _("Uninitializing account…");

  case CALLS_ACCOUNT_STATE_CONNECTING:
    return _("Connecting to server…");

  case CALLS_ACCOUNT_STATE_ONLINE:
    return _("Account is online");

  case CALLS_ACCOUNT_STATE_DISCONNECTING:
    return _("Disconnecting from server…");

  case CALLS_ACCOUNT_STATE_OFFLINE:
    return _("Account is offline");

  case CALLS_ACCOUNT_STATE_ERROR:
    return _("Account encountered an error");

  default:
    return NULL;
  }
}

/**
 * calls_account_state_reason_to_string:
 * @reason: A #CallsAccountStateReason
 *
 * Returns: (transfer none): A human readable description for why an account state changed
 */
const char *
calls_account_state_reason_to_string (CallsAccountStateReason reason)
{
  switch (reason) {
  case CALLS_ACCOUNT_STATE_REASON_UNKNOWN:
    return _("No reason given");

  case CALLS_ACCOUNT_STATE_REASON_INITIALIZATION_STARTED:
    return _("Initialization started");

  case CALLS_ACCOUNT_STATE_REASON_INITIALIZED:
    return _("Initialization complete");

  case CALLS_ACCOUNT_STATE_REASON_DEINITIALIZATION_STARTED:
    return _("Uninitialization started");

  case CALLS_ACCOUNT_STATE_REASON_DEINITIALIZED:
    return _("Uninitialization complete");

  case CALLS_ACCOUNT_STATE_REASON_NO_CREDENTIALS:
    return _("No credentials set");

  case CALLS_ACCOUNT_STATE_REASON_CONNECT_STARTED:
    return _("Starting to connect");

  case CALLS_ACCOUNT_STATE_REASON_CONNECTION_TIMEOUT:
    return _("Connection timed out");

  case CALLS_ACCOUNT_STATE_REASON_CONNECTION_DNS_ERROR:
    return _("Domain name could not be resolved");

  case CALLS_ACCOUNT_STATE_REASON_AUTHENTICATION_FAILURE:
    return _("Server did not accept username or password");

  case CALLS_ACCOUNT_STATE_REASON_CONNECTED:
    return _("Connecting complete");

  case CALLS_ACCOUNT_STATE_REASON_DISCONNECT_STARTED:
    return _("Starting to disconnect");

  case CALLS_ACCOUNT_STATE_REASON_DISCONNECTED:
    return _("Disconnecting complete");

  case CALLS_ACCOUNT_STATE_REASON_INTERNAL_ERROR:
    return _("Internal error occurred");

  default:
    return NULL;
  }
}

/**
 * calls_account_emit_message_for_state_change:
 * @account: A #CallsAccount
 * @new_state: The new #CallsAccountState
 * @reason: The #CallsAccountStateReason for the state change
 *
 * Returns: Emits a human readable message for a state change
 */
void
calls_account_emit_message_for_state_change (CallsAccount           *account,
                                             CallsAccountState       new_state,
                                             CallsAccountStateReason reason)
{
  g_autofree char *message = NULL;
  gboolean state_is_for_ui = FALSE;
  gboolean reason_is_for_ui = FALSE;

  g_return_if_fail (CALLS_IS_ACCOUNT (account));

  state_is_for_ui = calls_account_state_is_for_ui (new_state);
  reason_is_for_ui = calls_account_state_reason_is_for_ui (reason);

  if (!state_is_for_ui && !reason_is_for_ui)
    return;

  if (reason_is_for_ui || calls_log_get_verbosity () >= 3)
    message = g_strdup_printf ("%s: %s",
                               calls_account_state_to_string (new_state),
                               calls_account_state_reason_to_string (reason));
  else
    message = g_strdup (calls_account_state_to_string (new_state));

  calls_message_source_emit_message (CALLS_MESSAGE_SOURCE (account),
                                     message,
                                     reason_is_for_ui ? GTK_MESSAGE_ERROR : GTK_MESSAGE_INFO);
}


/**
 * calls_account_state_is_for_ui:
 * @state: a #CallsAccountState
 *
 * Helper function to decide which account states are important.
 * Is used f.e. to decide if message should be shown to the user.
 *
 * Returns: %TRUE if account state is considered important, %FALSE otherwise
 */
gboolean
calls_account_state_is_for_ui (CallsAccountState state)
{
  if (calls_log_get_verbosity () >= 3)
    return TRUE;

  switch (state) {
  case CALLS_ACCOUNT_STATE_UNKNOWN:
  case CALLS_ACCOUNT_STATE_INITIALIZING:
  case CALLS_ACCOUNT_STATE_DEINITIALIZING:
  case CALLS_ACCOUNT_STATE_CONNECTING:
  case CALLS_ACCOUNT_STATE_DISCONNECTING:
    return FALSE;

  case CALLS_ACCOUNT_STATE_ERROR:
  case CALLS_ACCOUNT_STATE_OFFLINE:
  case CALLS_ACCOUNT_STATE_ONLINE:
    return TRUE;

  default:
    return FALSE;
  }
}


/**
 * calls_account_state_reason_is_for_ui:
 * @reason: a #CallsAccountStateReason
 *
 * Helper function to decide which account states reasons are important.
 * Is used f.e. to decide if message should be shown to the user.
 *
 * Returns: %TRUE if account state reason is considered important, %FALSE otherwise
 */
gboolean
calls_account_state_reason_is_for_ui (CallsAccountStateReason reason)
{
  switch (reason) {
  case CALLS_ACCOUNT_STATE_REASON_NO_CREDENTIALS:
  case CALLS_ACCOUNT_STATE_REASON_CONNECTION_TIMEOUT:
  case CALLS_ACCOUNT_STATE_REASON_CONNECTION_DNS_ERROR:
  case CALLS_ACCOUNT_STATE_REASON_AUTHENTICATION_FAILURE:
  case CALLS_ACCOUNT_STATE_REASON_INTERNAL_ERROR:
    return TRUE;

  default:
    return FALSE;
  }
}
