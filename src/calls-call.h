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

#ifndef CALLS_CALL_H__
#define CALLS_CALL_H__

#include "calls-best-match.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_CALL (calls_call_get_type ())

G_DECLARE_INTERFACE (CallsCall, calls_call, CALLS, CALL, GObject);

typedef enum
{
  CALLS_CALL_STATE_ACTIVE = 1,
  CALLS_CALL_STATE_HELD,
  CALLS_CALL_STATE_DIALING,
  CALLS_CALL_STATE_ALERTING,
  CALLS_CALL_STATE_INCOMING,
  CALLS_CALL_STATE_WAITING,
  CALLS_CALL_STATE_DISCONNECTED
} CallsCallState;

void     calls_call_state_to_string  (GString        *string,
                                      CallsCallState  state);
gboolean calls_call_state_parse_nick (CallsCallState *state,
                                      const gchar    *nick);

struct _CallsCallInterface
{
  GTypeInterface parent_iface;

  void           (*answer)     (CallsCall *self);
  void           (*hang_up)    (CallsCall *self);
  void           (*tone_start) (CallsCall *self,
                                gchar      key);
  void           (*tone_stop)  (CallsCall *self,
                                gchar      key);
};


const gchar *    calls_call_get_number     (CallsCall *self);
const gchar *    calls_call_get_name       (CallsCall *self);
CallsCallState   calls_call_get_state      (CallsCall *self);
gboolean         calls_call_get_inbound    (CallsCall *self);
void             calls_call_answer         (CallsCall *self);
void             calls_call_hang_up        (CallsCall *self);
void             calls_call_tone_start     (CallsCall *self,
                                            gchar      key);
gboolean         calls_call_tone_stoppable (CallsCall *self);
void             calls_call_tone_stop      (CallsCall *self,
                                            gchar      key);
CallsBestMatch * calls_call_get_contact    (CallsCall *self);


G_END_DECLS

#endif /* CALLS_CALL_H__ */
