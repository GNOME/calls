/*
 * Copyright (C) 2023 Purism SPC
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
 * Authors: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_PLUGIN_MANAGER (calls_plugin_manager_get_type ())

G_DECLARE_FINAL_TYPE (CallsPluginManager, calls_plugin_manager, CALLS, PLUGIN_MANAGER, GObject)


CallsPluginManager *calls_plugin_manager_get_default (void);
gboolean            calls_plugin_manager_load_plugin (CallsPluginManager *self,
                                                      const char         *name,
                                                      GError            **error);
gboolean            calls_plugin_manager_unload_plugin (CallsPluginManager *self,
                                                        const char         *name,
                                                        GError            **error);
gboolean            calls_plugin_manager_unload_all_plugins (CallsPluginManager *self, GError **error);
const char        **calls_plugin_manager_get_plugin_names (CallsPluginManager *self,
                                                           guint              *length);
gboolean            calls_plugin_manager_has_plugin (CallsPluginManager *self,
                                                     const char         *name);
gboolean            calls_plugin_manager_has_any_plugins (CallsPluginManager *self);
GListModel         *calls_plugin_manager_get_plugins (CallsPluginManager *self);
GListModel         *calls_plugin_manager_get_providers (CallsPluginManager *self);

G_END_DECLS
