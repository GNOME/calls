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

#include "calls-account-provider.h"

/**
 * SECTION:account-provider
 * @short_description: An interface for CallsProvider to use online accounts
 * @Title: CallsOnlineAccount
 *
 * #CallsAccountProvider is an interface that should be implemented by a
 * #CallsProvider when it provides accounts. See #CallsAccount and
 * #CallsCredentials.
 */

G_DEFINE_INTERFACE (CallsAccountProvider, calls_account_provider, CALLS_TYPE_PROVIDER)


static void
calls_account_provider_default_init (CallsAccountProviderInterface *iface)
{
}

/**
 * calls_account_provider_add_account:
 * @self: A #CallsAccountProvider
 * @credentials: A #CallsCredentials
 *
 * Add an account.
 *
 * Returns: %TRUE if successfully added, %FALSE otherwise
 */
gboolean
calls_account_provider_add_account (CallsAccountProvider *self,
                                    CallsCredentials     *credentials)
{
  CallsAccountProviderInterface *iface;

  g_return_val_if_fail (CALLS_IS_ACCOUNT_PROVIDER (self), FALSE);

  iface = CALLS_ACCOUNT_PROVIDER_GET_IFACE (self);
  g_return_val_if_fail (iface->add_account != NULL, FALSE);

  g_debug ("Trying to add account for %s", calls_credentials_get_name (credentials));

  return iface->add_account (self, credentials);
}

/**
 * calls_account_provider_remove_account:
 * @self: A #CallsAccountProvider
 * @credentials: A #CallsCredentials
 *
 * Removes an account.
 *
 * Returns: %TRUE if successfully removed, %FALSE otherwise
 */
gboolean
calls_account_provider_remove_account (CallsAccountProvider *self,
                                       CallsCredentials     *credentials)
{
  CallsAccountProviderInterface *iface;

  g_return_val_if_fail (CALLS_IS_ACCOUNT_PROVIDER (self), FALSE);

  iface = CALLS_ACCOUNT_PROVIDER_GET_IFACE (self);
  g_return_val_if_fail (iface->remove_account != NULL, FALSE);

  g_debug ("Trying to remove account from %s", calls_credentials_get_name (credentials));

  return iface->remove_account (self, credentials);
}

/**
 * calls_account_provider_get_account:
 * @self: A #CallsAccountProvider
 * @credentials: A #CallsCredentials
 *
 * Get the account which is using #CallsCredentials
 */
CallsAccount *
calls_account_provider_get_account (CallsAccountProvider *self,
                                    CallsCredentials     *credentials)
{
  CallsAccountProviderInterface *iface;

  g_return_val_if_fail (CALLS_IS_ACCOUNT_PROVIDER (self), NULL);

  iface = CALLS_ACCOUNT_PROVIDER_GET_IFACE (self);
  g_return_val_if_fail (iface->get_account != NULL, NULL);

  g_debug ("Trying to get account from %s", calls_credentials_get_name (credentials));

  return iface->get_account (self, credentials);
}
