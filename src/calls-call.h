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

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_CALL (calls_call_get_type ())

G_DECLARE_DERIVABLE_TYPE (CallsCall, calls_call, CALLS, CALL, GObject)

/**
 * CallsCallState:
 * @CALLS_CALL_STATE_UNKNOWN: Call state unknown
 * @CALLS_CALL_STATE_ACTIVE: Call is active
 * @CALLS_CALL_STATE_HELD: Call is being held
 * @CALLS_CALL_STATE_DIALING: Outgoing call being established
 * @CALLS_CALL_STATE_ALERTING: Remote party is being alerted
 * @CALLS_CALL_STATE_INCOMING: New incoming call
 * @CALLS_CALL_STATE_DISCONNECTED: Call was successfully terminated
 */
typedef enum {
  CALLS_CALL_STATE_UNKNOWN = 0,
  CALLS_CALL_STATE_ACTIVE,
  CALLS_CALL_STATE_HELD,
  CALLS_CALL_STATE_DIALING,
  CALLS_CALL_STATE_ALERTING,
  CALLS_CALL_STATE_INCOMING,
  CALLS_CALL_STATE_DISCONNECTED
} CallsCallState;

typedef enum {
  CALLS_CALL_TYPE_UNKNOWN = 0,
  CALLS_CALL_TYPE_CELLULAR,
  CALLS_CALL_TYPE_SIP_VOICE,
} CallsCallType;

/**
 * CallsCallClass:
 * @hang_up: Called to hang up a call.
 */
struct _CallsCallClass {
  GObjectClass parent_class;

  const char  *(*get_protocol)         (CallsCall *self);
  void         (*answer)               (CallsCall *self);
  void         (*hang_up)              (CallsCall *self);
  void         (*send_dtmf_tone)       (CallsCall *self,
                                        char       key);
};

const char    *calls_call_get_id                 (CallsCall *self);
void           calls_call_set_id                 (CallsCall  *self,
                                                  const char *id);
const char    *calls_call_get_name               (CallsCall *self);
void           calls_call_set_name               (CallsCall  *self,
                                                  const char *name);
CallsCallState calls_call_get_state              (CallsCall *self);
void           calls_call_set_state              (CallsCall     *self,
                                                  CallsCallState state);
gboolean       calls_call_get_encrypted          (CallsCall *self);
void           calls_call_set_encrypted          (CallsCall *self,
                                                  gboolean   encrypted);
CallsCallType  calls_call_get_call_type          (CallsCall *self);
gboolean       calls_call_get_inbound            (CallsCall *self);
const char    *calls_call_get_protocol           (CallsCall *self);
void           calls_call_answer                 (CallsCall *self);
void           calls_call_hang_up                (CallsCall *self);
gboolean       calls_call_can_dtmf               (CallsCall *self);
void           calls_call_send_dtmf_tone         (CallsCall *self,
                                                  char       key);
gboolean       calls_call_get_we_hung_up         (CallsCall *self);

gboolean       calls_call_state_parse_nick (CallsCallState *state,
                                            const char     *nick);


G_END_DECLS
