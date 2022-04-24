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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CALLS_TYPE_MESSAGE_SOURCE (calls_message_source_get_type ())

G_DECLARE_INTERFACE (CallsMessageSource, calls_message_source, CALLS, MESSAGE_SOURCE, GObject)

struct _CallsMessageSourceInterface {
  GTypeInterface parent_iface;
};



/**
 * CALLS_EMIT_MESSAGE:
 * @obj: an object which can be cast to a #CallsMessageSource
 * @text: the message text as a string
 * @type: the type of the message
 *
 * Emit a message signal with the specified information.  This is a
 * convenience macro for objects implementing interfaces that
 * require #CallsMessageSource.
 *
 */
#define CALLS_EMIT_MESSAGE(obj,text,type)               \
  g_signal_emit_by_name (CALLS_MESSAGE_SOURCE(obj),     \
                         "message", text, type)


/**
 * CALLS_ERROR:
 * @obj: an object which can be cast to a #CallsMessageSource
 * @error: a pointer to a #GError containing the error message
 *
 * Emit a message signal with an error type, the text of which is
 * contained as the message in a #GError.  This is a convenience
 * macro for objects implementing interfaces that require
 * #CallsMessageSource.
 *
 */
#define CALLS_ERROR(obj,error)                                  \
  CALLS_EMIT_MESSAGE (obj, error->message, GTK_MESSAGE_ERROR)

void calls_message_source_emit_message (CallsMessageSource *self,
                                        const char         *message,
                                        GtkMessageType      message_type);

G_END_DECLS
