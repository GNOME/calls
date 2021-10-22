/*
 * Copyright (C) 2021 Purism SPC
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
 * Author(s):
 *   Bob Ham <bob.ham@puri.sm>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *   Julian Sparber <julian@sparber.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include "calls-settings.h"

#include <glib-object.h>
#include <folks/folks.h>
#include <libebook-contacts/libebook-contacts.h>

#include "calls-best-match.h"

G_BEGIN_DECLS

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GeeMap, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GeeSet, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GeeSortedSet, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GeeCollection, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EPhoneNumber, e_phone_number_free)

typedef void (*IdleCallback) (gpointer        user_data,
                              FolksIndividual *individual);

#define CALLS_TYPE_CONTACTS_PROVIDER (calls_contacts_provider_get_type ())

G_DECLARE_FINAL_TYPE (CallsContactsProvider, calls_contacts_provider, CALLS, CONTACTS_PROVIDER, GObject)

CallsContactsProvider  *calls_contacts_provider_new                  (CallsSettings         *settings);
GeeCollection          *calls_contacts_provider_get_individuals      (CallsContactsProvider *self);
CallsBestMatch         *calls_contacts_provider_lookup_phone_number  (CallsContactsProvider *self,
                                                                      const gchar           *number);
void                    calls_contacts_provider_consume_iter_on_idle (GeeIterator           *iter,
                                                                      IdleCallback           callback,
                                                                      gpointer               user_data);

G_END_DECLS
