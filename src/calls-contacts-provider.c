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
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gee-0.8/gee.h>
#include <folks/folks.h>
#include <libebook-contacts/libebook-contacts.h>

#include "calls-contacts-provider.h"
#include "calls-best-match.h"


typedef struct
{
  GeeIterator           *iter;
  IdleCallback           callback;
  gpointer               user_data;
} IdleData;

struct _CallsContactsProvider
{
  GObject                    parent_instance;

  FolksIndividualAggregator *folks_aggregator;

  GHashTable                *phone_number_best_matches;
  gchar                     *country_code;
};

G_DEFINE_TYPE (CallsContactsProvider, calls_contacts_provider, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_COUNTRY_CODE,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

enum {
  SIGNAL_ADDED,
  SIGNAL_REMOVED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];

static void folks_remove_contact (CallsContactsProvider *self,
                                  FolksIndividual       *individual);
static void folks_add_contact    (CallsContactsProvider *self,
                                  FolksIndividual       *individual);

static gboolean
folks_individual_has_phone_numbers (FolksIndividual *individual)
{
  g_autoptr (GeeSet) phone_numbers;

  g_object_get (individual, "phone-numbers", &phone_numbers, NULL);

  return !gee_collection_get_is_empty (GEE_COLLECTION (phone_numbers));
}

static void
folks_individual_property_changed_cb (CallsContactsProvider *self,
                                      GParamSpec            *pspec,
                                      FolksIndividual       *individual)
{
  if (!folks_individual_has_phone_numbers (individual))
    folks_remove_contact (self, individual);
}

static int
do_on_idle (IdleData *data)
{
  if (gee_iterator_next (data->iter)) {
    data->callback (data->user_data, gee_iterator_get (data->iter));

    return G_SOURCE_CONTINUE;
  } else {
    return G_SOURCE_REMOVE;
  }
}

static void
folks_add_contact (CallsContactsProvider *self,
                   FolksIndividual       *individual)
{
  if (individual == NULL)
    return;

  if (!folks_individual_has_phone_numbers (individual))
    return;

  g_signal_connect_object (G_OBJECT (individual),
                           "notify::phone-numbers",
                           G_CALLBACK (folks_individual_property_changed_cb),
                           self, G_CONNECT_SWAPPED);

  g_signal_emit (self, signals[SIGNAL_ADDED], 0, individual);
}

static void
folks_remove_contact (CallsContactsProvider *self,
                      FolksIndividual       *individual)
{
  if (individual == NULL)
    return;

  g_signal_handlers_disconnect_by_func (individual, folks_individual_property_changed_cb, self);
  g_signal_emit (self, signals[SIGNAL_REMOVED], 0, individual);
}


static void
folks_individuals_changed_cb (CallsContactsProvider *self,
                              GeeMultiMap           *changes)
{
  g_autoptr (GeeCollection) removed = NULL;
  g_autoptr (GeeCollection) added = NULL;

  removed = GEE_COLLECTION (gee_multi_map_get_keys (changes));
  if (!gee_collection_get_is_empty (removed))
    calls_contacts_provider_consume_iter_on_idle (gee_iterable_iterator (GEE_ITERABLE (removed)),
                                                  (IdleCallback) folks_remove_contact,
                                                  self);

  added = gee_multi_map_get_values (changes);
  if (!gee_collection_get_is_empty (added))
    calls_contacts_provider_consume_iter_on_idle (gee_iterable_iterator (GEE_ITERABLE (added)),
                                                  (IdleCallback) folks_add_contact,
                                                  self);
}

static void
folks_prepare_cb (GObject      *obj,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  folks_individual_aggregator_prepare_finish (FOLKS_INDIVIDUAL_AGGREGATOR (obj), res, &error);

  if (error)
    g_warning ("Failed to load Folks contacts: %s", error->message);
}

static void
calls_contacts_provider_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CallsContactsProvider *self = CALLS_CONTACTS_PROVIDER (object);

  switch (property_id) {
  case PROP_COUNTRY_CODE:
    g_free (self->country_code);
    self->country_code = g_value_dup_string (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
calls_contacts_provider_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  CallsContactsProvider *self = CALLS_CONTACTS_PROVIDER (object);

  switch (property_id) {
  case PROP_COUNTRY_CODE:
    g_value_set_string (value, self->country_code);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_contacts_provider_finalize (GObject *object)
{
  CallsContactsProvider *self = CALLS_CONTACTS_PROVIDER (object);

  g_clear_object (&self->country_code);
  g_clear_object (&self->folks_aggregator);
  g_clear_pointer (&self->phone_number_best_matches, g_hash_table_unref);

  G_OBJECT_CLASS (calls_contacts_provider_parent_class)->finalize (object);
}


static void
calls_contacts_provider_class_init (CallsContactsProviderClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_contacts_provider_get_property;
  object_class->set_property = calls_contacts_provider_set_property;
  object_class->finalize = calls_contacts_provider_finalize;

  signals[SIGNAL_ADDED] =
   g_signal_new ("added",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST,
                 0,
                 NULL, NULL, NULL,
                 G_TYPE_NONE,
                 1,
                 FOLKS_TYPE_INDIVIDUAL);

  signals[SIGNAL_REMOVED] =
   g_signal_new ("removed",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST,
                 0,
                 NULL, NULL, NULL,
                 G_TYPE_NONE,
                 1,
                 FOLKS_TYPE_INDIVIDUAL);

  props[PROP_COUNTRY_CODE] = g_param_spec_string ("country-code",
                                                  "country code",
                                                  "The default country code to use",
                                                  NULL,
                                                  G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_contacts_provider_init (CallsContactsProvider *self)
{
  g_autoptr (GeeCollection) individuals = NULL;
  self->folks_aggregator = folks_individual_aggregator_dup ();

  individuals = calls_contacts_provider_get_individuals (self);

  g_signal_connect_object (self->folks_aggregator,
                           "individuals-changed-detailed",
                           G_CALLBACK (folks_individuals_changed_cb),
                           self, G_CONNECT_SWAPPED);

  if (!gee_collection_get_is_empty (individuals))
    calls_contacts_provider_consume_iter_on_idle (gee_iterable_iterator (GEE_ITERABLE (individuals)),
                                                  (IdleCallback) folks_add_contact,
                                                  self);

  folks_individual_aggregator_prepare (self->folks_aggregator, folks_prepare_cb, self);

  self->phone_number_best_matches = g_hash_table_new_full (g_str_hash,
                                                           g_str_equal,
                                                           g_free,
                                                           g_object_unref);
}

CallsContactsProvider *
calls_contacts_provider_new (void)
{
  return g_object_new (CALLS_TYPE_CONTACTS_PROVIDER, NULL);
}


/*
 * calls_contacts_provider_get_individuals:
 * @self: The #CallsContactsProvider
 *
 * Returns all individuals currrently loaded.
 */
GeeCollection *
calls_contacts_provider_get_individuals (CallsContactsProvider *self)
{
  g_return_val_if_fail (CALLS_IS_CONTACTS_PROVIDER (self), NULL);

  return gee_map_get_values (folks_individual_aggregator_get_individuals (self->folks_aggregator));
}

/*
 * calls_contacts_provider_lookup_phone_number:
 * @self: The #CallsContactsProvider
 * @number: The phonenumber
 *
 * Get a best contact match for a phone number
 *
 * Returns: (transfer: full): The best match as #CallsBestMatch
 */
CallsBestMatch *
calls_contacts_provider_lookup_phone_number (CallsContactsProvider *self,
                                             const gchar           *number)
{
  g_autoptr (CallsBestMatch) best_match = NULL;
  g_autofree gchar *country_code = NULL;

  g_return_val_if_fail (CALLS_IS_CONTACTS_PROVIDER (self), NULL);

  best_match = g_hash_table_lookup (self->phone_number_best_matches, number);

  if (best_match) {
    g_object_ref (best_match);

    g_object_get (best_match, "country-code", &country_code, NULL);
    if (g_strcmp0 (country_code, self->country_code) != 0)
      calls_best_match_set_phone_number (best_match, number);

    return g_steal_pointer (&best_match);
  }

  best_match = calls_best_match_new (number);

  g_hash_table_insert (self->phone_number_best_matches, g_strdup (number), g_object_ref (best_match));

  return g_steal_pointer (&best_match);
}

void
calls_contacts_provider_consume_iter_on_idle (GeeIterator *iter,
                                              IdleCallback callback,
                                              gpointer     user_data)
{
  IdleData *data = g_new (IdleData, 1);
  data->iter = iter;
  data->user_data = user_data;
  data->callback = callback;

  g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                   G_SOURCE_FUNC (do_on_idle),
                   data,
                   g_free);
}
