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
 * Author: Julian Sparber <julian@sparber.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef CALLS_CONTACTS_BOX_H__
#define CALLS_CONTACTS_BOX_H__

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CALLS_TYPE_CONTACTS_BOX (calls_contacts_box_get_type ())

G_DECLARE_FINAL_TYPE (CallsContactsBox, calls_contacts_box, CALLS, CONTACTS_BOX, AdwBin);

GtkWidget *calls_contacts_box_new (void);

G_END_DECLS

#endif /* CALLS_CONTACTS_BOX_H__ */
