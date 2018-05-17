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

#ifndef CALLS_CALL_HOLDER_H__
#define CALLS_CALL_HOLDER_H__

#include "calls-call-data.h"
#include "calls-call-display.h"
#include "calls-call-selector-item.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_CALL_HOLDER (calls_call_holder_get_type ())

G_DECLARE_FINAL_TYPE (CallsCallHolder, calls_call_holder, CALLS, CALL_HOLDER, GObject);

CallsCallHolder *calls_call_holder_new (CallsCall *call);
CallsCallData *calls_call_holder_get_data (CallsCallHolder *holder);
CallsCallDisplay *calls_call_holder_get_display (CallsCallHolder *holder);
CallsCallSelectorItem *calls_call_holder_get_selector_item (CallsCallHolder *holder);

G_END_DECLS

#endif /* CALLS_CALL_HOLDER_H__ */
