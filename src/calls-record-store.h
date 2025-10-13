/*
 * Copyright (C) 2018, 2019 Purism SPC
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

#ifndef CALLS_RECORD_STORE_H__
#define CALLS_RECORD_STORE_H__

#include "gtk/gtk.h"

G_BEGIN_DECLS

#define CALLS_TYPE_RECORD_STORE (calls_record_store_get_type ())

G_DECLARE_FINAL_TYPE (CallsRecordStore, calls_record_store, CALLS, RECORD_STORE, GObject);

CallsRecordStore *calls_record_store_new (void);
GListModel *calls_record_store_get_list_model (CallsRecordStore *);
gboolean    calls_record_store_is_busy (CallsRecordStore *self);

G_END_DECLS

#endif /* CALLS_RECORD_STORE_H__ */
