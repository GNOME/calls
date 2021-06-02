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

#define CALLS_TYPE_CREDENTIALS (calls_credentials_get_type ())

G_DECLARE_FINAL_TYPE (CallsCredentials, calls_credentials, CALLS, CREDENTIALS, GObject);


CallsCredentials       *calls_credentials_new                     (void);
gboolean                calls_credentials_update_from_keyfile     (CallsCredentials *self,
                                                                   GKeyFile         *key_file,
                                                                   const char       *name);
void                    calls_credentials_set_name                (CallsCredentials *self,
                                                                   const char       *name);
const char             *calls_credentials_get_name                (CallsCredentials *self);

G_END_DECLS

