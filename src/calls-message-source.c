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

#include "calls-message-source.h"


G_DEFINE_INTERFACE (CallsMessageSource, calls_message_source, G_TYPE_OBJECT);

enum {
  SIGNAL_MESSAGE,
  SIGNAL_LAST_SIGNAL,
};
static guint signals [SIGNAL_LAST_SIGNAL];


static void
calls_message_source_default_init (CallsMessageSourceInterface *iface)
{
  signals[SIGNAL_MESSAGE] =
    g_signal_newv ("message",
		   G_TYPE_FROM_INTERFACE (iface),
		   G_SIGNAL_RUN_LAST,
		   NULL, NULL, NULL, NULL,
		   G_TYPE_NONE,
		   2, calls_message_signal_arg_types());
}


GType *
calls_message_signal_arg_types()
{
  static gsize initialization_value = 0;
  static GType arg_types[2];

  if (g_once_init_enter (&initialization_value))
    {
      arg_types[0] = G_TYPE_STRING;
      arg_types[1] = GTK_TYPE_MESSAGE_TYPE;

      g_once_init_leave (&initialization_value, 1);
    }

  return arg_types;
}
