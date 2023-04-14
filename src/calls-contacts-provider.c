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
 * Author(s):
 *   Bob Ham <bob.ham@puri.sm>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *   Julian Sparber <julian@sparber.net>
 *   Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "CallsContactsProvider"

#include "calls-contacts-provider.h"
#include "calls-best-match.h"
#include "calls-settings.h"
#include "calls-util.h"

#include <gee-0.8/gee.h>
#include <folks/folks.h>
#include <libebook-contacts/libebook-contacts.h>

#define DBUS_BUS_NAME "org.gnome.Contacts"


/**
 * SECTION:calls-contacts-provider
 * @short_description: Provides contact information
 * @Title: CallsContactsProvider
 *
 * This object tracks contacts reported by libfolks,
 * allow to perform contact lookups and provides functions
 * for adding new contacts.
 */


typedef struct {
  GeeIterator *iter;
  IdleCallback callback;
  gpointer     user_data;
} IdleData;


struct _CallsContactsProvider {
  GObject                    parent_instance;

  FolksIndividualAggregator *folks_aggregator;
  CallsSettings             *settings;

  GHashTable                *best_matches;

  guint                      bus_watch_id;
  GDBusActionGroup          *contacts_action_group;
  gboolean                   can_add_contacts;
};

G_DEFINE_TYPE (CallsContactsProvider, calls_contacts_provider, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CAN_ADD_CONTACTS,
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
set_can_add_contacts (CallsContactsProvider *self,
                      gboolean               can_add)
{
  g_assert (CALLS_IS_CONTACTS_PROVIDER (self));

  g_info ("Can%s add contacts", can_add ? "" : "not");

  if (self->can_add_contacts == can_add)
    return;

  self->can_add_contacts = can_add;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CAN_ADD_CONTACTS]);
}


static void
on_contacts_actions_updated (CallsContactsProvider *self)
{
  gboolean has_action;
  gboolean action_enabled;
  const char *contact_action_name = "new-contact-data";

  g_assert (CALLS_IS_CONTACTS_PROVIDER (self));

  has_action =
    g_action_group_has_action (G_ACTION_GROUP (self->contacts_action_group),
                               contact_action_name);
  action_enabled =
    g_action_group_get_action_enabled (G_ACTION_GROUP (self->contacts_action_group),
                                       contact_action_name);

  set_can_add_contacts (self, has_action && action_enabled);
}


static void
on_contacts_appeared (GDBusConnection *connection,
                      const char      *name,
                      const char      *owner_name,
                      gpointer         user_data)
{
  CallsContactsProvider *self;

  g_assert (CALLS_IS_CONTACTS_PROVIDER (user_data));

  self = user_data;
  g_clear_object (&self->contacts_action_group);
  self->contacts_action_group = g_dbus_action_group_get (connection,
                                                         name,
                                                         "/org/gnome/Contacts");

  g_signal_connect_swapped (self->contacts_action_group,
                            "action-added",
                            G_CALLBACK (on_contacts_actions_updated),
                            self);

  on_contacts_actions_updated (self);
}


static void
calls_contacts_provider_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  CallsContactsProvider *self = CALLS_CONTACTS_PROVIDER (object);

  switch (property_id) {
  case PROP_CAN_ADD_CONTACTS:
    g_value_set_boolean (value, calls_contacts_provider_get_can_add_contacts (self));
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

  if (self->contacts_action_group)
    g_signal_handlers_disconnect_by_data (self->contacts_action_group, self);

  g_clear_handle_id (&self->bus_watch_id, g_bus_unwatch_name);
  g_clear_object (&self->contacts_action_group);
  g_clear_object (&self->folks_aggregator);
  g_clear_pointer (&self->best_matches, g_hash_table_unref);

  G_OBJECT_CLASS (calls_contacts_provider_parent_class)->finalize (object);
}


static void
calls_contacts_provider_class_init (CallsContactsProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_contacts_provider_get_property;
  object_class->finalize = calls_contacts_provider_finalize;

  /**
   * CallsContactsProvider::added:
   * @self: The #CallsContactsProvider instance
   * @individual: A #FolksIndividual
   *
   * This signal is emitted when the backend reports a new contact
   * having been added.
   */
  signals[SIGNAL_ADDED] =
    g_signal_new ("added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  FOLKS_TYPE_INDIVIDUAL);
  /**
   * CallsContactsProvider::removed:
   * @self: The #CallsContactsProvider instance
   * @individual: A #FolksIndividual
   *
   * This signal is emitted when the backend reports a contact
   * having been removed.
   */
  signals[SIGNAL_REMOVED] =
    g_signal_new ("removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  FOLKS_TYPE_INDIVIDUAL);

  /**
   * CallsContactsProvider::can-add-contacts:
   *
   * Whether we can add contacts or not.
   */
  props[PROP_CAN_ADD_CONTACTS] =
    g_param_spec_boolean ("can-add-contacts",
                          "Can add contacts",
                          "Whether we can add contacts",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_contacts_provider_init (CallsContactsProvider *self)
{
  g_autoptr (GeeCollection) individuals = NULL;

  self->settings = calls_settings_get_default ();
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

  self->best_matches = g_hash_table_new_full (g_str_hash,
                                              g_str_equal,
                                              g_free,
                                              g_object_unref);

  self->bus_watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                         DBUS_BUS_NAME,
                                         G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                         on_contacts_appeared,
                                         NULL,
                                         self,
                                         NULL);
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
 * calls_contacts_provider_lookup_id:
 * @self: The #CallsContactsProvider
 * @id: The id (f.e. a phone number)
 *
 * Get a best contact match for a id
 *
 * Returns: (transfer full): The best match as #CallsBestMatch
 */
CallsBestMatch *
calls_contacts_provider_lookup_id (CallsContactsProvider *self,
                                   const char            *id)
{
  CallsBestMatch *best_match;

  g_return_val_if_fail (CALLS_IS_CONTACTS_PROVIDER (self), NULL);

  if (STR_IS_NULL_OR_EMPTY (id))
    best_match = g_hash_table_lookup (self->best_matches, "");
  else
    best_match = g_hash_table_lookup (self->best_matches, id);

  if (best_match)
    return g_object_ref (best_match);

  best_match = calls_best_match_new (id);

  g_object_bind_property (self->settings, "country-code",
                          best_match, "country-code",
                          G_BINDING_SYNC_CREATE);

  g_hash_table_insert (self->best_matches, g_strdup (id ?: ""), best_match);

  return g_object_ref (best_match);
}

/**
 * calls_contacts_provider_consume_iter_on_idle:
 * @iter: A #GeeIterator
 * @callback: A callback to be called on all items of @iter
 * @user_data: A pointer to be passed to the @callback
 *
 * Queue's processing of all #FolksIndividual items of @iter with @callback one
 * individual per event loop iteration. Can be used to split up operating
 * on potentially large set of individuals to prevent the
 * event loop from being blocked for too long making the UI unresponsive.
 */
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

/**
 * calls_contacts_provider_get_can_add_contacts:
 * @self: The #CallsContactsProvider
 *
 * Returns: %TRUE if contacts can be added, %FALSE otherwise
 */
gboolean
calls_contacts_provider_get_can_add_contacts (CallsContactsProvider *self)
{
  g_return_val_if_fail (CALLS_IS_CONTACTS_PROVIDER (self), FALSE);

  return self->can_add_contacts;
}

/**
 * calls_contacts_provider_add_new_contact:
 * @self: The #CallsContactsProvider
 * @phone_number: The phone number of the new contact
 *
 * Opens GNOME contacts and prepopulates the phone number for a new contact
 * to be added.
 */
void
calls_contacts_provider_add_new_contact (CallsContactsProvider *self,
                                         const char            *phone_number)
{
  GVariant *contact_parameter;
  GVariantBuilder contact_builder;
  CallsBestMatch *best_match;

  g_return_if_fail (CALLS_IS_CONTACTS_PROVIDER (self));
  g_return_if_fail (phone_number || *phone_number);
  g_return_if_fail (self->can_add_contacts);

  best_match = g_hash_table_lookup (self->best_matches, phone_number);
  if (best_match && calls_best_match_has_individual (best_match)) {
    g_warning ("Cannot add contact. Contact '%s' with number '%s' already exists",
               calls_best_match_get_name (best_match), phone_number);
    return;
  }

  g_variant_builder_init (&contact_builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add (&contact_builder, "(ss)",
                         "phone-numbers",
                         phone_number);
  contact_parameter = g_variant_builder_end (&contact_builder);

  g_action_group_activate_action (G_ACTION_GROUP (self->contacts_action_group),
                                  "new-contact-data",
                                  contact_parameter);
}

#undef DBUS_BUS_NAME
