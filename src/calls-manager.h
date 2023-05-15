/* calls-manager.c
 *
 * Copyright (C) 2020, 2021 Purism SPC
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
 * Authors: Julian Sparber <julian.sparber@puri.sm>
 * Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "calls-contacts-provider.h"
#include "calls-origin.h"
#include "calls-settings.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_MANAGER (calls_manager_get_type ())

G_DECLARE_FINAL_TYPE (CallsManager, calls_manager, CALLS, MANAGER, GObject)

typedef enum /*< flags >*/
{
  CALLS_MANAGER_FLAGS_UNKNOWN               = 0,
  CALLS_MANAGER_FLAGS_HAS_CELLULAR_PROVIDER = (1<<0),
  CALLS_MANAGER_FLAGS_HAS_CELLULAR_MODEM    = (1<<1),
  CALLS_MANAGER_FLAGS_HAS_VOIP_PROVIDER     = (1<<2),
  CALLS_MANAGER_FLAGS_HAS_VOIP_ACCOUNT      = (1<<3),
} CallsManagerFlags;


CallsManager          *calls_manager_new                      (void);
CallsManager          *calls_manager_get_default              (void);
CallsContactsProvider *calls_manager_get_contacts_provider    (CallsManager *self);
CallsSettings         *calls_manager_get_settings             (CallsManager *self);
CallsManagerFlags      calls_manager_get_state_flags          (CallsManager *self);
GListModel            *calls_manager_get_origins              (CallsManager *self);
GList                 *calls_manager_get_calls                (CallsManager *self);
GListModel            *calls_manager_get_suitable_origins     (CallsManager *self,
                                                               const char   *target);
CallsOrigin           *calls_manager_get_origin_by_id         (CallsManager *self,
                                                               const char   *origin_id);
void                   calls_manager_hang_up_all_calls        (CallsManager *self);

G_END_DECLS
