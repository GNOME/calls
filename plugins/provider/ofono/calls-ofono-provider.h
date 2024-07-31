/*
 * Copyright (C) 2018 Purism SPC
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
 * Author: Bob Ham <bob.ham@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef CALLS_OFONO_PROVIDER_H__
#define CALLS_OFONO_PROVIDER_H__

#include "calls-provider.h"

#include <glib-object.h>
#include <gio/gio.h>
#include <libpeas.h>

G_BEGIN_DECLS

#define CALLS_TYPE_OFONO_PROVIDER (calls_ofono_provider_get_type ())

G_DECLARE_FINAL_TYPE (CallsOfonoProvider, calls_ofono_provider, CALLS, OFONO_PROVIDER, CallsProvider)

void peas_register_types                       (PeasObjectModule *module);

G_END_DECLS

#endif /* CALLS_OFONO_PROVIDER_H__ */
