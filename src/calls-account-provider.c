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

#define G_LOG_DOMAIN "CallsAccountProvider"

#include "calls-account-provider.h"

/**
 * SECTION:account-provider
 * @short_description: An interface for #CallsProvider using online accounts
 * @Title: CallsAccountProvider
 *
 * A provider for accounts.
 */

G_DEFINE_INTERFACE (CallsAccountProvider, calls_account_provider, CALLS_TYPE_PROVIDER)

enum {
  WIDGET_EDIT_DONE,
  LAST_SIGNAL,
};
static guint signals[LAST_SIGNAL];

static void
calls_account_provider_default_init (CallsAccountProviderInterface *iface)
{
  /* The account provider should emit this signal when the widget is not needed anymore */
  signals[WIDGET_EDIT_DONE] =
    g_signal_new ("widget-edit-done",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
}


/**
 * calls_account_provider_get_account_widget:
 * @self: A #CallsAccountProvider
 *
 * Returns: (transfer none): A #GtkWidget for adding or editing account credentials.
 */
GtkWidget *
calls_account_provider_get_account_widget (CallsAccountProvider *self)
{
  CallsAccountProviderInterface *iface;

  g_return_val_if_fail (CALLS_IS_ACCOUNT_PROVIDER (self), NULL);

  iface = CALLS_ACCOUNT_PROVIDER_GET_IFACE (self);
  g_return_val_if_fail (iface->get_account_widget, NULL);

  return iface->get_account_widget (self);
}

/**
 * calls_account_provider_add_new_account:
 * @self: A #CallsAccountProvider
 *
 * Prepares the #GtkWidget to add a new account (clear any forms).
 * See calls_account_provider_get_account_widget().
 *
 * The caller is responsible for embedding the widget somewhere visible.
 */
void
calls_account_provider_add_new_account (CallsAccountProvider *self)
{
  CallsAccountProviderInterface *iface;

  g_return_if_fail (CALLS_IS_ACCOUNT_PROVIDER (self));

  iface = CALLS_ACCOUNT_PROVIDER_GET_IFACE (self);
  g_return_if_fail (iface->add_new_account);

  iface->add_new_account (self);
}

/**
 * calls_account_provider_edit_account:
 * @self: A #CallsAccountProvider
 * @account: A #CallsAccount to edit
 *
 * Prepares the #GtkWidget to edit the given @account (prepulate forms).
 * See calls_account_provider_get_account_widget().
 */
void
calls_account_provider_edit_account (CallsAccountProvider *self,
                                     CallsAccount         *account)
{
  CallsAccountProviderInterface *iface;

  g_return_if_fail (CALLS_IS_ACCOUNT_PROVIDER (self));

  iface = CALLS_ACCOUNT_PROVIDER_GET_IFACE (self);
  g_return_if_fail (iface->edit_account);

  iface->edit_account (self, account);
}
