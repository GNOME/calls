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

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define CALLS_TYPE_USSD (calls_ussd_get_type ())

G_DECLARE_INTERFACE (CallsUssd, calls_ussd, CALLS, USSD, GObject)

/* This is basically MMModem3gppUssdSessionState */
typedef enum
{
  CALLS_USSD_STATE_UNKNOWN,
  CALLS_USSD_STATE_IDLE,
  CALLS_USSD_STATE_ACTIVE,
  CALLS_USSD_STATE_USER_RESPONSE
} CallsUssdState;

struct _CallsUssdInterface {
  GTypeInterface parent_iface;

  CallsUssdState (*get_state)       (CallsUssd *self);
  void           (*initiate_async)  (CallsUssd          *self,
                                     const char         *command,
                                     GCancellable       *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer            user_data);
  char          *(*initiate_finish) (CallsUssd    *self,
                                     GAsyncResult *result,
                                     GError      **error);
  void           (*respond_async)    (CallsUssd          *self,
                                      const char         *response,
                                      GCancellable       *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer            user_data);
  char          *(*respond_finish)   (CallsUssd    *self,
                                      GAsyncResult *result,
                                      GError      **error);
  void           (*cancel_async)     (CallsUssd          *self,
                                      GCancellable       *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer            user_data);
  gboolean       (*cancel_finish)    (CallsUssd    *self,
                                      GAsyncResult *result,
                                      GError      **error);
};


CallsUssdState calls_ussd_get_state       (CallsUssd *self);
void           calls_ussd_initiate_async  (CallsUssd          *self,
                                           const char         *command,
                                           GCancellable       *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer            user_data);
char          *calls_ussd_initiate_finish (CallsUssd    *self,
                                           GAsyncResult *result,
                                           GError      **error);
void           calls_ussd_respond_async   (CallsUssd          *self,
                                           const char         *response,
                                           GCancellable       *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer            user_data);
char          *calls_ussd_respond_finish  (CallsUssd    *self,
                                           GAsyncResult *result,
                                           GError      **error);
void           calls_ussd_cancel_async    (CallsUssd          *self,
                                           GCancellable       *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer            user_data);
gboolean       calls_ussd_cancel_finish   (CallsUssd    *self,
                                           GAsyncResult *result,
                                           GError      **error);


G_END_DECLS
