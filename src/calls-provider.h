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

#ifndef CALLS_PROVIDER_H__
#define CALLS_PROVIDER_H__

#include "calls-message-source.h"
#include "calls-origin.h"
#include "calls-call.h"
#include "util.h"

#include <glib-object.h>

G_BEGIN_DECLS


#define CALLS_TYPE_PROVIDER (calls_provider_get_type ())

G_DECLARE_INTERFACE (CallsProvider, calls_provider, CALLS, PROVIDER, GObject);


struct _CallsProviderInterface
{
  GTypeInterface parent_iface;

  const gchar * (*get_name) (CallsProvider *self);
  GList * (*get_origins) (CallsProvider *self);
};


const gchar * calls_provider_get_name    (CallsProvider *self);
gchar *       calls_provider_get_status  (CallsProvider *self);
GList *       calls_provider_get_origins (CallsProvider *self);


G_END_DECLS

#endif /* CALLS_PROVIDER_H__ */
