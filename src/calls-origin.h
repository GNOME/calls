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

#ifndef CALLS_ORIGIN_H__
#define CALLS_ORIGIN_H__

#include "calls-call.h"
#include "util.h"

#include <glib-object.h>

G_BEGIN_DECLS


#define CALLS_TYPE_ORIGIN (calls_origin_get_type ())

G_DECLARE_INTERFACE (CallsOrigin, calls_origin, CALLS, ORIGIN, GObject);


struct _CallsOriginInterface
{
  GTypeInterface parent_iface;

  void               (*dial)                                (CallsOrigin *self,
                                                             const char  *number);
  gboolean           (*supports_protocol)                   (CallsOrigin *self,
                                                             const char  *protocol);
};

typedef void (*CallsOriginForeachCallFunc) (gpointer param, CallsCall* call, CallsOrigin* origin);

char *                 calls_origin_get_name                (CallsOrigin *self);
char *                 calls_origin_get_id                  (CallsOrigin *self);
GList *                calls_origin_get_calls               (CallsOrigin *self);
void                   calls_origin_foreach_call            (CallsOrigin *self,
                                                             CallsOriginForeachCallFunc callback,
                                                             gpointer     param);
void                   calls_origin_dial                    (CallsOrigin *self,
                                                             const char  *number);
gboolean               calls_origin_supports_protocol       (CallsOrigin *self,
                                                             const char *protocol);
G_END_DECLS

#endif /* CALLS_ORIGIN_H__ */
