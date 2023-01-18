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

#include <libmm-glib.h>
#include <glib-object.h>

#include "calls-call.h"

G_BEGIN_DECLS

#define CALLS_TYPE_MM_CALL (calls_mm_call_get_type ())

G_DECLARE_FINAL_TYPE (CallsMMCall, calls_mm_call, CALLS, MM_CALL, CallsCall)

CallsMMCall *calls_mm_call_new                   (MMCall *mm_call);
const char  *calls_mm_call_get_object_path       (CallsMMCall *call);
const char  *calls_mm_call_get_disconnect_reason (CallsMMCall *call);

G_END_DECLS

