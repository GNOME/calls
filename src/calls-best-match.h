/*
 * Copyright (C) 2019 Purism SPC
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

#ifndef CALLS_BEST_MATCH_H__
#define CALLS_BEST_MATCH_H__

#include <gdk/gdk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CALLS_TYPE_BEST_MATCH (calls_best_match_get_type ())

G_DECLARE_FINAL_TYPE (CallsBestMatch, calls_best_match, CALLS, BEST_MATCH, GObject);

CallsBestMatch *calls_best_match_new                (const char *phone_number);
gboolean        calls_best_match_has_individual     (CallsBestMatch *self);
gboolean        calls_best_match_is_favourite       (CallsBestMatch *self);
const char     *calls_best_match_get_phone_number   (CallsBestMatch *self);
void            calls_best_match_set_phone_number   (CallsBestMatch *self,
                                                     const char     *phone_number);
const char     *calls_best_match_get_name           (CallsBestMatch *self);
GdkTexture     *calls_best_match_get_avatar         (CallsBestMatch *self);
const char     *calls_best_match_get_primary_info   (CallsBestMatch *self);
const char     *calls_best_match_get_secondary_info (CallsBestMatch *self);

G_END_DECLS

#endif /* CALLS_BEST_MATCH_H__ */
