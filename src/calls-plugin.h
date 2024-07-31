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
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "calls-provider.h"

#include <libpeas.h>

G_BEGIN_DECLS

#define CALLS_TYPE_PLUGIN (calls_plugin_get_type ())

G_DECLARE_FINAL_TYPE (CallsPlugin, calls_plugin, CALLS, PLUGIN, GObject)

CallsPlugin       *calls_plugin_new                    (PeasPluginInfo *info);
gboolean           calls_plugin_load                   (CallsPlugin *self,
                                                        GError     **error);
gboolean           calls_plugin_unload                 (CallsPlugin *self,
                                                        GError     **error);
gboolean           calls_plugin_is_loaded              (CallsPlugin *self);
gboolean           calls_plugin_has_provider           (CallsPlugin *self);
CallsProvider     *calls_plugin_get_provider           (CallsPlugin *self);
const char        *calls_plugin_get_module_name        (CallsPlugin *self);
const char        *calls_plugin_get_name               (CallsPlugin *self);
const char        *calls_plugin_get_description        (CallsPlugin *self);
const char* const *calls_plugin_get_authors            (CallsPlugin *self);
const char        *calls_plugin_get_copyright          (CallsPlugin *self);
const char        *calls_plugin_get_version            (CallsPlugin *self);

G_END_DECLS
