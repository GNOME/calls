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

typedef enum
{
  CALLS_MANAGER_STATE_UNKNOWN = 1,
  CALLS_MANAGER_STATE_NO_PLUGIN,
  CALLS_MANAGER_STATE_NO_PROVIDER,
  CALLS_MANAGER_STATE_NO_ORIGIN,
  CALLS_MANAGER_STATE_NO_VOICE_MODEM,
  CALLS_MANAGER_STATE_READY,
} CallsManagerState;


CallsManager          *calls_manager_new                      (void);
CallsManager          *calls_manager_get_default              (void);
CallsContactsProvider *calls_manager_get_contacts_provider    (CallsManager *self);
CallsSettings         *calls_manager_get_settings             (CallsManager *self);
void                   calls_manager_add_provider             (CallsManager *self,
                                                               const char   *name);
void                   calls_manager_remove_provider          (CallsManager *self,
                                                               const char   *name);
gboolean               calls_manager_has_provider             (CallsManager *self,
                                                               const char   *name);
gboolean               calls_manager_is_modem_provider        (CallsManager *self,
                                                               const char   *name);
CallsManagerState      calls_manager_get_state                (CallsManager *self);
GListModel            *calls_manager_get_origins              (CallsManager *self);
GList                 *calls_manager_get_calls                (CallsManager *self);
void                   calls_manager_dial                     (CallsManager *self,
                                                               CallsOrigin  *origin,
                                                               const char   *target);
GListModel            *calls_manager_get_suitable_origins     (CallsManager *self,
                                                               const char   *target);
const gchar           *calls_manager_get_contact_name         (CallsCall    *call);
gboolean               calls_manager_has_active_call          (CallsManager *self);
void                   calls_manager_hang_up_all_calls        (CallsManager *self);
gboolean               calls_manager_has_any_provider         (CallsManager *self);
const char           **calls_manager_get_provider_names       (CallsManager *self,
                                                               guint        *length);
GList                 *calls_manager_get_providers            (CallsManager *self);

G_END_DECLS
