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
#include "enum-types.h"

/**
 * SECTION:account
 * @short_description: An interface for online accounts
 * @Title: CallsAccount
 *
 * #CallsAccount is a type of #CallsOrigin for online accounts.
 */

G_DEFINE_INTERFACE (CallsAccount, calls_account, CALLS_TYPE_ORIGIN)


static void
calls_account_default_init (CallsAccountInterface *iface)
{
  g_object_interface_install_property (iface,
    g_param_spec_enum ("account-state",
                       "Account state",
                       "The state of the account",
                       CALLS_TYPE_ACCOUNT_STATE,
                       CALLS_ACCOUNT_NULL,
                       G_PARAM_READABLE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY));

  g_object_interface_install_property (iface,
    g_param_spec_string ("address",
                         "Address",
                         "The address of this account",
                         NULL,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY));
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

  g_return_val_if_fail (CALLS_IS_ACCOUNT (self), CALLS_ACCOUNT_NULL);

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
