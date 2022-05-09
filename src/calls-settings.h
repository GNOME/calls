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

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_SETTINGS (calls_settings_get_type ())

G_DECLARE_FINAL_TYPE (CallsSettings, calls_settings, CALLS, SETTINGS, GObject);

CallsSettings *calls_settings_get_default                    (void);
gboolean       calls_settings_get_use_default_origins        (CallsSettings *self);
void           calls_settings_set_use_default_origins        (CallsSettings *self,
                                                              gboolean       enable);
char          *calls_settings_get_country_code               (CallsSettings *self);
void           calls_settings_set_country_code               (CallsSettings *self,
                                                              const char    *country_code);
char         **calls_settings_get_autoload_plugins           (CallsSettings *self);
void           calls_settings_set_autoload_plugins           (CallsSettings      *self,
                                                              const char * const *plugins);
char         **calls_settings_get_preferred_audio_codecs     (CallsSettings *self);
void           calls_settings_set_preferred_audio_codecs     (CallsSettings      *self,
                                                              const char * const *codecs);
gboolean       calls_settings_get_always_allow_sdes          (CallsSettings *self);
void           calls_settings_set_always_allow_sdes          (CallsSettings *self,
                                                              gboolean       enabled);

G_END_DECLS
