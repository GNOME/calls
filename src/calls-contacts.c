/*
 * Copyright (C) 2019 Purism SPC
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

#include "calls-contacts.h"

#include <glib/gi18n.h>


struct _CallsContacts
{
  GObject parent_instance;

  FolksIndividualAggregator *big_pile_of_contacts;
  /** Map of call target (EPhoneNumber) to CallsBestMatch */
  GHashTable                *phone_number_best_matches;
};

G_DEFINE_TYPE (CallsContacts, calls_contacts, G_TYPE_OBJECT);


static guint
phone_number_hash (const EPhoneNumber *number)
{
  g_autofree gchar *str = NULL;

  str = e_phone_number_to_string
    (number, E_PHONE_NUMBER_FORMAT_E164);
  g_assert (str != NULL);

  return g_str_hash (str);
}


static gboolean
phone_number_equal (const EPhoneNumber *a,
                    const EPhoneNumber *b)
{
  EPhoneNumberMatch match = e_phone_number_compare (a, b);

  return
    match == E_PHONE_NUMBER_MATCH_EXACT
    ||
    match == E_PHONE_NUMBER_MATCH_NATIONAL;
}


static void
prepare_cb (FolksIndividualAggregator *aggregator,
            GAsyncResult              *res,
            CallsContacts             *self)
{
  GError *error = NULL;
  folks_individual_aggregator_prepare_finish (aggregator,
                                              res,
                                              &error);
  if (error)
    {
      g_warning ("Error preparing Folks individual aggregator: %s",
                 error->message);
      g_error_free (error);
    }
  else
    {
      g_debug ("Folks individual aggregator prepared");
    }
}


static void
constructed (GObject *object)
{
  CallsContacts *self = CALLS_CONTACTS (object);

  self->big_pile_of_contacts = folks_individual_aggregator_dup ();
  g_assert (self->big_pile_of_contacts != NULL);
  g_object_ref (self->big_pile_of_contacts);

  folks_individual_aggregator_prepare (self->big_pile_of_contacts,
                                       (GAsyncReadyCallback)prepare_cb,
                                       self);

  self->phone_number_best_matches = g_hash_table_new_full
    ((GHashFunc)phone_number_hash,
     (GEqualFunc)phone_number_equal,
     (GDestroyNotify)e_phone_number_free,
     g_object_unref);

  G_OBJECT_CLASS (calls_contacts_parent_class)->constructed (object);
}


static void
dispose (GObject *object)
{
  CallsContacts *self = CALLS_CONTACTS (object);

  g_clear_pointer (&self->phone_number_best_matches,
                   g_hash_table_unref);

  g_clear_object (&self->big_pile_of_contacts);

  G_OBJECT_CLASS (calls_contacts_parent_class)->dispose (object);
}


static void
calls_contacts_class_init (CallsContactsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;
}


static void
calls_contacts_init (CallsContacts *self)
{
}


CallsContacts *
calls_contacts_new ()
{
  return g_object_new (CALLS_TYPE_CONTACTS, NULL);
}


static void
search_view_prepare_cb (FolksSearchView *view,
                        GAsyncResult    *res,
                        CallsContacts   *self)
{
  GError *error = NULL;

  folks_search_view_prepare_finish (view, res, &error);

  if (error)
    {
      g_warning ("Error preparing Folks search view: %s",
                 error->message);
      g_error_free (error);
    }
}


CallsBestMatch *
calls_contacts_lookup_phone_number (CallsContacts *self,
                                    EPhoneNumber  *number)
{
  CallsBestMatch *best_match;
  CallsPhoneNumberQuery *query;
  CallsBestMatchView *view;

  best_match = g_hash_table_lookup (self->phone_number_best_matches, number);
  if (best_match)
    {
      return best_match;
    }

  query = calls_phone_number_query_new (number);

  view = calls_best_match_view_new
    (self->big_pile_of_contacts, FOLKS_QUERY (query));
  g_object_unref (query);

  folks_search_view_prepare
    (FOLKS_SEARCH_VIEW (view),
     (GAsyncReadyCallback)search_view_prepare_cb,
     self);

  best_match = calls_best_match_new (view);
  g_assert (best_match != NULL);
  g_object_unref (view);

  g_hash_table_insert (self->phone_number_best_matches,
                       e_phone_number_copy (number),
                       best_match);

  return best_match;
}
