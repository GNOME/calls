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

#ifndef CALLS_ENUMERATE_PARAMS_H__
#define CALLS_ENUMERATE_PARAMS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_ENUMERATE_PARAMS (calls_enumerate_params_get_type ())

G_DECLARE_FINAL_TYPE (CallsEnumerateParams, calls_enumerate_params,
                      CALLS, ENUMERATE_PARAMS, GObject);

CallsEnumerateParams *calls_enumerate_params_new             (gpointer user_data);
gpointer              calls_enumerate_params_get_user_data   (CallsEnumerateParams *self);
gboolean              calls_enumerate_params_have_callbacks  (CallsEnumerateParams *self,
                                                              GType                 obj_type);
gboolean              calls_enumerate_params_get_enumerating (CallsEnumerateParams *self);
void                  calls_enumerate_params_set_enumerating (CallsEnumerateParams *self,
                                                              gboolean              enuming);
gboolean              calls_enumerate_params_add             (CallsEnumerateParams *self,
                                                              GType                 obj_type,
                                                              const gchar          *detail,
                                                              GCallback             callback);
void                  calls_enumerate_params_connect         (CallsEnumerateParams *self,
                                                              GObject              *object);
GCallback             calls_enumerate_params_lookup          (CallsEnumerateParams *self,
                                                              GType                 obj_type,
                                                              const gchar          *detail);

G_END_DECLS

#endif /* CALLS_ENUMERATE_PARAMS_H__ */
