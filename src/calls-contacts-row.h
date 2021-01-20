/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* calls-contacts-row.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *   Julian Sparber <julian@sparber.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <folks/folks.h>

G_BEGIN_DECLS

#define CALLS_TYPE_CONTACTS_ROW (calls_contacts_row_get_type ())

G_DECLARE_FINAL_TYPE (CallsContactsRow, calls_contacts_row, CALLS, CONTACTS_ROW, GtkListBoxRow)

GtkWidget       *calls_contacts_row_new      (FolksIndividual  *item);
FolksIndividual *calls_contacts_row_get_item (CallsContactsRow *self);

G_END_DECLS
