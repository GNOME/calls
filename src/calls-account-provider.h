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

#include "calls-account.h"
#include "calls-provider.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CALLS_TYPE_ACCOUNT_PROVIDER (calls_account_provider_get_type ())

G_DECLARE_INTERFACE (CallsAccountProvider, calls_account_provider, CALLS, ACCOUNT_PROVIDER, CallsProvider)

struct _CallsAccountProviderInterface {
  GTypeInterface parent_iface;

  GtkWidget     *(*get_account_widget)              (CallsAccountProvider *self);
  void           (*add_new_account)                 (CallsAccountProvider *self);
  void           (*edit_account)                    (CallsAccountProvider *self,
                                                     CallsAccount         *account);
};

GtkWidget *calls_account_provider_get_account_widget (CallsAccountProvider *self);
void       calls_account_provider_add_new_account    (CallsAccountProvider *self);
void       calls_account_provider_edit_account       (CallsAccountProvider *self,
                                                      CallsAccount         *account);

G_END_DECLS
