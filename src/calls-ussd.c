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
 * Author: Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "calls-ussd.h"
#include "enum-types.h"
#include "calls-util.h"

/**
 * SECTION:calls-ussd
 * @short_description: An interface for handling USSD
 * @Title: CallsUssd
 */


G_DEFINE_INTERFACE (CallsUssd, calls_ussd, G_TYPE_OBJECT)

enum {
  USSD_ADDED,
  USSD_CANCELLED,
  USSD_STATE_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
calls_ussd_default_init (CallsUssdInterface *iface)
{
  /**
   * CallsUssd::ussd-added:
   * @self: a #CallsUssd
   * @response: a string
   *
   * Emitted when some USSD response is recieved
   */
  signals[USSD_ADDED] =
    g_signal_new ("ussd-added",
                  CALLS_TYPE_USSD,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  /**
   * CallsUssd::ussd-cancelled:
   * @self: a #CallsUssd
   *
   * Emitted when the active USSD is cancelled.  Shouldn't
   * be emitted if cancelled to immidiately request again.
   */
  signals[USSD_CANCELLED] =
    g_signal_new ("ussd-cancelled",
                  CALLS_TYPE_USSD,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * CallsUssd::ussd-state-changed:
   * @self: a #CallsUssd
   *
   * Emitted when the USSD state changes.  Use
   * calls_ussd_get_state() to get the state.
   */
  signals[USSD_STATE_CHANGED] =
    g_signal_new ("ussd-state-changed",
                  CALLS_TYPE_USSD,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

CallsUssdState
calls_ussd_get_state (CallsUssd *self)
{
  CallsUssdInterface *iface;

  g_return_val_if_fail (CALLS_IS_USSD (self), CALLS_USSD_STATE_UNKNOWN);

  iface = CALLS_USSD_GET_IFACE (self);

  if (!iface->get_state)
    return CALLS_USSD_STATE_UNKNOWN;

  return iface->get_state (self);
}

void
calls_ussd_initiate_async (CallsUssd          *self,
                           const char         *command,
                           GCancellable       *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer            user_data)
{
  CallsUssdInterface *iface;

  g_return_if_fail (CALLS_IS_USSD (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (command);

  iface = CALLS_USSD_GET_IFACE (self);

  if (iface->initiate_async)
    iface->initiate_async (self, command, cancellable, callback, user_data);
}

char *
calls_ussd_initiate_finish (CallsUssd    *self,
                            GAsyncResult *result,
                            GError      **error)
{
  CallsUssdInterface *iface;

  g_return_val_if_fail (CALLS_IS_USSD (self), NULL);

  iface = CALLS_USSD_GET_IFACE (self);

  if (iface->initiate_finish)
    return iface->initiate_finish (self, result, error);

  return NULL;
}

void
calls_ussd_respond_async (CallsUssd          *self,
                          const char         *response,
                          GCancellable       *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer            user_data)
{
  CallsUssdInterface *iface;

  g_return_if_fail (CALLS_IS_USSD (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (response);

  iface = CALLS_USSD_GET_IFACE (self);

  if (iface->respond_async)
    iface->respond_async (self, response, cancellable, callback, user_data);
}

char *
calls_ussd_respond_finish (CallsUssd    *self,
                           GAsyncResult *result,
                           GError      **error)
{
  CallsUssdInterface *iface;

  g_return_val_if_fail (CALLS_IS_USSD (self), NULL);

  iface = CALLS_USSD_GET_IFACE (self);

  if (iface->respond_finish)
    return iface->respond_finish (self, result, error);

  return NULL;
}

void
calls_ussd_cancel_async (CallsUssd          *self,
                         GCancellable       *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer            user_data)
{
  CallsUssdInterface *iface;

  g_return_if_fail (CALLS_IS_USSD (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  iface = CALLS_USSD_GET_IFACE (self);

  if (iface->cancel_async)
    iface->cancel_async (self, cancellable, callback, user_data);
}

gboolean
calls_ussd_cancel_finish (CallsUssd    *self,
                          GAsyncResult *result,
                          GError      **error)
{
  CallsUssdInterface *iface;

  g_return_val_if_fail (CALLS_IS_USSD (self), FALSE);

  iface = CALLS_USSD_GET_IFACE (self);

  if (iface->cancel_finish)
    return iface->cancel_finish (self, result, error);

  return FALSE;
}
