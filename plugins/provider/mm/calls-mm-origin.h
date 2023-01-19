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

#pragma once

#include <glib-object.h>
#include <libmm-glib.h>

G_BEGIN_DECLS

#define CALLS_TYPE_MM_ORIGIN (calls_mm_origin_get_type ())

G_DECLARE_FINAL_TYPE (CallsMMOrigin, calls_mm_origin, CALLS, MM_ORIGIN, GObject);

CallsMMOrigin *calls_mm_origin_new (MMObject   *modem,
                                    const char *id);
gboolean       calls_mm_origin_matches (CallsMMOrigin *self,
                                        MMObject      *modem);

G_END_DECLS
