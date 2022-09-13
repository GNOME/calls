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

/* TODO docstring */
typedef enum {
  CALLS_RING_STATE_INACTIVE,
  CALLS_RING_STATE_RINGING,
  CALLS_RING_STATE_RINGING_SOFT,
  CALLS_RING_STATE_ERROR
} CallsRingState;


#define CALLS_TYPE_RINGER (calls_ringer_get_type ())

G_DECLARE_FINAL_TYPE (CallsRinger, calls_ringer, CALLS, RINGER, GObject)


CallsRinger *calls_ringer_new                 (void);
CallsRingState calls_ringer_get_state         (CallsRinger *self);
void         calls_ringer_start_ringing       (CallsRinger *self,
                                               gboolean quiet);
void         calls_ringer_stop_ringing        (CallsRinger *self);

G_END_DECLS
