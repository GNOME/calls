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
 * Author: Julian Sparber <julian@sparber.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef CALLS_IN_APP_NOTIFICATION_H__
#define CALLS_IN_APP_NOTIFICATION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CALLS_TYPE_IN_APP_NOTIFICATION (calls_in_app_notification_get_type ())

G_DECLARE_FINAL_TYPE (CallsInAppNotification, calls_in_app_notification, CALLS, IN_APP_NOTIFICATION, GtkRevealer)

CallsInAppNotification * calls_in_app_notification_new ();
void calls_in_app_notification_show (CallsInAppNotification *self, const gchar *message);
void calls_in_app_notification_hide (CallsInAppNotification *self);

G_END_DECLS

#endif /* CALLS_IN_APP_NOTIFICATION_H__ */
