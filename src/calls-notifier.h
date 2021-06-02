/*
 * Copyright (C) 2020 Purism SPC
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
 * Author: Guido Günther <agx@sigxcpu.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef CALLS_NOTIFIER_H__
#define CALLS_NOTIFIER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_NOTIFIER (calls_notifier_get_type ())

G_DECLARE_FINAL_TYPE (CallsNotifier, calls_notifier, CALLS, NOTIFIER, GObject);

CallsNotifier *calls_notifier_new (void);

G_END_DECLS

#endif /* CALLS_NOTIFIER_H__ */
