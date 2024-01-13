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
 * Author: Adrien Plazas <adrien.plazas@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef CALLS_HISTORY_BOX_H__
#define CALLS_HISTORY_BOX_H__

#include "calls-new-call-box.h"

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CALLS_TYPE_HISTORY_BOX (calls_history_box_get_type ())

G_DECLARE_FINAL_TYPE (CallsHistoryBox, calls_history_box, CALLS, HISTORY_BOX, AdwBin);

CallsHistoryBox *calls_history_box_new (GListModel *model);

G_END_DECLS

#endif /* CALLS_HISTORY_BOX_H__ */
