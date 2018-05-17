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

#ifndef CALLS_OFONO_OBJECT_H__
#define CALLS_OFONO_OBJECT_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define CALLS_TYPE_OFONO_OBJECT (calls_ofono_object_get_type ())

G_DECLARE_DERIVABLE_TYPE (CallsOfonoObject, calls_ofono_object, CALLS, OFONO_OBJECT, GDBusProxy);


typedef void (*CallsOfonoSignalCallback) (CallsOfonoObject *object,
                                          gchar            *signal_name,
                                          GVariant         *parameters,
                                          gpointer          user_data);

/**
 * CallsOfonoObjectClass:
 * @parent_class: The parent class
 * @get_callbacks: An abstract function which must return a 
 * 
 */
struct _CallsOfonoObjectClass
{
  GDBusProxyClass parent_class;

  GHashTable *(*get_callbacks) (CallsOfonoObject *self);
};

CallsOfonoObject *calls_ofono_object_new ();

G_END_DECLS

#endif /* CALLS_OFONO_OBJECT_H__ */
