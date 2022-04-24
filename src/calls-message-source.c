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

/**
 * SECTION:calls-message-source
 * @short_description: A source of messages for the user.
 * @Title: CallsMessageSource
 *
 * All three of the main interfaces, #CallsProvider, #CallsOrigin and
 * #CallsCall require #CallsMessageSource.  They use this interface's
 * message signal to emit messages intended for display to the user.
 */

G_DEFINE_INTERFACE (CallsMessageSource, calls_message_source, G_TYPE_OBJECT);

enum {
  SIGNAL_MESSAGE,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];


static void
calls_message_source_default_init (CallsMessageSourceInterface *iface)
{
  GType arg_types[2] = { G_TYPE_STRING, GTK_TYPE_MESSAGE_TYPE };

  /**
   * CallsMessageSource::message:
   * @self: The #CallsMessageSource instance.
   * @text: The message text.
   * @type: The type of the message; error, warning, etc.
   *
   * This signal is emitted when an implementing-object needs to emit
   * a message to the user.  The message should be suitable for
   * presentation to the user as-is. This means it should be translated
   * to the users locale.
   */
  signals[SIGNAL_MESSAGE] =
    g_signal_newv ("message",
                   G_TYPE_FROM_INTERFACE (iface),
                   G_SIGNAL_RUN_LAST,
                   NULL, NULL, NULL, NULL,
                   G_TYPE_NONE,
                   2, arg_types);
}

/**
 * calls_message_source_emit_message:
 * @self: A #CallsMessageSource
 * @message: The message to emit
 * @message_type: The type of the message: Error, warning, etc
 *
 * Emits a message which should be shown to the user in the user interface.
 * Messages should be translated into the users locale
 */
void
calls_message_source_emit_message (CallsMessageSource *self,
                                   const char         *message,
                                   GtkMessageType      message_type)
{
  g_return_if_fail (CALLS_IS_MESSAGE_SOURCE (self));
  g_return_if_fail (message || *message);

  g_signal_emit (self, signals[SIGNAL_MESSAGE], 0, message, message_type);
}
