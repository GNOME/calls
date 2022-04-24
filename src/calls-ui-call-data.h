/*
 * Copyright (C) 2021, 2022 Purism SPC
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
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include "calls-call.h"

#include <cui-call.h>
#include <glib.h>

G_BEGIN_DECLS

#define CALLS_TYPE_UI_CALL_DATA (calls_ui_call_data_get_type ())

G_DECLARE_FINAL_TYPE (CallsUiCallData, calls_ui_call_data, CALLS, UI_CALL_DATA, GObject)

CallsUiCallData         *calls_ui_call_data_new               (CallsCall       *call,
                                                               const char      *origin_id);
void          calls_ui_call_data_silence_ring      (CallsUiCallData *self);
gboolean      calls_ui_call_data_get_silenced      (CallsUiCallData *self);
gboolean      calls_ui_call_data_get_ui_active     (CallsUiCallData *self);
CallsCallType calls_ui_call_data_get_call_type     (CallsUiCallData *self);
const char   *calls_ui_call_data_get_origin_id     (CallsUiCallData *self);
char         *calls_ui_call_data_dup_origin_name   (CallsUiCallData *self);

CuiCallState  calls_call_state_to_cui_call_state   (CallsCallState state);

G_END_DECLS
