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
#include "calls-account-provider.h"

#include <adwaita.h>


G_BEGIN_DECLS

#define CALLS_TYPE_ACCOUNT_ROW (calls_account_row_get_type ())

G_DECLARE_FINAL_TYPE (CallsAccountRow, calls_account_row, CALLS, ACCOUNT_ROW, AdwActionRow);

CallsAccountRow      *calls_account_row_new                  (CallsAccountProvider *provider,
                                                              CallsAccount         *account);
gboolean              calls_account_row_get_online           (CallsAccountRow *self);
void                  calls_account_row_set_online           (CallsAccountRow *self,
                                                              gboolean         online);
CallsAccount         *calls_account_row_get_account          (CallsAccountRow *self);
CallsAccountProvider *calls_account_row_get_account_provider (CallsAccountRow *self);

G_END_DECLS
