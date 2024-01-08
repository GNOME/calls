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

#include "calls-in-app-notification.h"

#define DEFAULT_TIMEOUT_SECONDS 3

void
calls_in_app_notification_show (AdwToastOverlay *overlay, const gchar *message)
{
  AdwToast* toast;

  g_return_if_fail (ADW_IS_TOAST_OVERLAY (overlay));

  toast = adw_toast_new (message);
  adw_toast_set_timeout (toast, DEFAULT_TIMEOUT_SECONDS);
  adw_toast_overlay_add_toast (overlay, toast);
}
