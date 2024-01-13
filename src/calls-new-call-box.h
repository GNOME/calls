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
 * Author: Adrien Plazas <adrien.plazas@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CALLS_TYPE_NEW_CALL_BOX (calls_new_call_box_get_type ())

G_DECLARE_FINAL_TYPE (CallsNewCallBox, calls_new_call_box, CALLS, NEW_CALL_BOX, AdwBin)

CallsNewCallBox *calls_new_call_box_new  (void);
void             calls_new_call_box_dial (CallsNewCallBox *self,
                                          const gchar     *target);
void             calls_new_call_box_send_ussd_async  (CallsNewCallBox    *self,
                                                      const char         *target,
                                                      GCancellable       *cancellable,
                                                      GAsyncReadyCallback callback,
                                                      gpointer            user_data);
char            *calls_new_call_box_send_ussd_finish (CallsNewCallBox *self,
                                                      GAsyncResult    *result,
                                                      GError         **error);

G_END_DECLS
