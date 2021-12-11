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

G_DECLARE_DERIVABLE_TYPE (CallsCall, calls_call, CALLS, CALL, GObject)

typedef enum
{
  CALLS_CALL_STATE_UNKNOWN = 0,
  CALLS_CALL_STATE_ACTIVE,
  CALLS_CALL_STATE_HELD,
  CALLS_CALL_STATE_DIALING,
  CALLS_CALL_STATE_ALERTING,
  CALLS_CALL_STATE_INCOMING,
  CALLS_CALL_STATE_WAITING,
  CALLS_CALL_STATE_DISCONNECTED
} CallsCallState;

struct _CallsCallClass
{
  GObjectClass parent_iface;

  const char     *(*get_protocol)         (CallsCall *self);
  void            (*answer)               (CallsCall *self);
  void            (*hang_up)              (CallsCall *self);
  void            (*send_dtmf_tone)       (CallsCall *self,
                                           char       key);
};

const char      *calls_call_get_id                 (CallsCall     *self);
void             calls_call_set_id                 (CallsCall     *self,
                                                    const char    *id);
const char      *calls_call_get_name               (CallsCall     *self);
void             calls_call_set_name               (CallsCall     *self,
                                                    const char    *name);
CallsCallState   calls_call_get_state              (CallsCall     *self);
void             calls_call_set_state              (CallsCall     *self,
                                                    CallsCallState state);
gboolean         calls_call_get_inbound            (CallsCall     *self);
const char      *calls_call_get_protocol           (CallsCall     *self);
void             calls_call_answer                 (CallsCall     *self);
void             calls_call_hang_up                (CallsCall     *self);
gboolean         calls_call_can_dtmf               (CallsCall     *self);
void             calls_call_send_dtmf_tone         (CallsCall     *self,
                                                    char           key);
CallsBestMatch  *calls_call_get_contact            (CallsCall     *self);
void             calls_call_silence_ring           (CallsCall     *self);
gboolean         calls_call_get_silenced           (CallsCall     *self);

void     calls_call_state_to_string  (GString         *string,
                                      CallsCallState   state);
gboolean calls_call_state_parse_nick (CallsCallState  *state,
                                      const char      *nick);


G_END_DECLS

#endif /* CALLS_CALL_H__ */
