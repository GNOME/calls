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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Calls. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Julian Sparber <julian.sparber@puri.sm>
 * Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define G_LOG_DOMAIN "CallsManager"

#include "config.h"

#include "calls-application.h"
#include "calls-account-provider.h"
#include "calls-contacts-provider.h"
#include "calls-manager.h"
#include "calls-provider.h"
#include "calls-settings.h"
#include "calls-ussd.h"

#include "enum-types.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

struct _CallsManager
{
  GObject parent_instance;

  GHashTable *providers;
  /* This is the protocols supported in principle. This is collected from the loaded
     providers and does not imply that there are any origins able to handle a given protocol.
     See origins_by_protocol for a GListStore of suitable origins per protocol.
  */
  GPtrArray *supported_protocols;

  GListStore *origins;
  /* origins_by_protocol maps protocol names to GListStore's of suitable origins */
  GHashTable *origins_by_protocol;

  CallsContactsProvider *contacts_provider;

  CallsManagerState state;
  CallsSettings *settings;
};

G_DEFINE_TYPE (CallsManager, calls_manager, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_STATE,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


enum {
  SIGNAL_CALL_ADD,
  SIGNAL_CALL_REMOVE,
  /* TODO: currently this event isn't emitted since the plugins don't give use
   * a usable error or error message. */
  SIGNAL_ERROR,
  USSD_ADDED,
  USSD_CANCELLED,
  USSD_STATE_CHANGED,
  PROVIDERS_CHANGED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals [SIGNAL_LAST_SIGNAL];

static void
set_state (CallsManager *self, CallsManagerState state)
{
  if (self->state == state)
    return;

  self->state = state;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}

static void
update_state (CallsManager *self)
{
  guint n_items;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_assert (CALLS_IS_MANAGER (self));

  if (g_hash_table_size (self->providers) == 0) {
    set_state (self, CALLS_MANAGER_STATE_NO_PROVIDER);
    return;
  }

  g_hash_table_iter_init (&iter, self->providers);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    CallsProvider *provider = CALLS_PROVIDER (value);

    if (calls_provider_is_modem (provider) && !calls_provider_is_operational (provider)) {
      set_state (self, CALLS_MANAGER_STATE_NO_VOICE_MODEM);
      return;
    }
  }

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->origins));

  if (n_items)
    set_state (self, CALLS_MANAGER_STATE_READY);
  else
    set_state (self, CALLS_MANAGER_STATE_NO_ORIGIN);
}

static gboolean
check_supported_protocol (CallsManager *self,
                          const char   *protocol)
{
  guint index;
  g_assert (CALLS_IS_MANAGER (self));
  g_assert (protocol);

  if (self->supported_protocols->len > 0)
    return g_ptr_array_find_with_equal_func (self->supported_protocols,
                                             protocol,
                                             g_str_equal,
                                             &index);

  return FALSE;
}

/* This function will update self->supported_protocols from available provider plugins */
static void
update_protocols (CallsManager *self)
{
  GHashTableIter iter;
  gpointer key, value;
  const char * const *protocols;

  g_assert (CALLS_IS_MANAGER (self));

  g_ptr_array_remove_range (self->supported_protocols,
                            0, self->supported_protocols->len);

  g_hash_table_iter_init (&iter, self->providers);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    const char *name = key;
    CallsProvider *provider = CALLS_PROVIDER (value);

    protocols = calls_provider_get_protocols (provider);

    if (protocols == NULL) {
      g_debug ("Plugin %s does not provide any protocols", name);
      continue;
    }
    for (guint i = 0; protocols[i] != NULL; i++) {
      if (!check_supported_protocol (self, protocols[i]))
        g_ptr_array_add (self->supported_protocols, g_strdup (protocols[i]));

      if (!g_hash_table_contains (self->origins_by_protocol, protocols[i])) {
        /* Add a new GListStore if there's none already.
         * Actually adding origins to self->origins_by_protocol is done
         * in rebuild_origins_by_protocol()
         */
        GListStore *store = g_list_store_new (CALLS_TYPE_ORIGIN);
        g_hash_table_insert (self->origins_by_protocol,
                             g_strdup (protocols[i]),
                             store);
      }
    }
  }

  update_state (self);
}

static void
add_call (CallsManager *self, CallsCall *call, CallsOrigin *origin)
{
  g_return_if_fail (CALLS_IS_MANAGER (self));
  g_return_if_fail (CALLS_IS_ORIGIN (origin));
  g_return_if_fail (CALLS_IS_CALL (call));

  g_signal_emit (self, signals[SIGNAL_CALL_ADD], 0, call, origin);
}

static void
remove_call (CallsManager *self, CallsCall *call, gchar *reason, CallsOrigin *origin)
{
  g_return_if_fail (CALLS_IS_MANAGER (self));
  g_return_if_fail (CALLS_IS_ORIGIN (origin));
  g_return_if_fail (CALLS_IS_CALL (call));

  /* We ignore the reason for now, because it doesn't give any usefull information */
  g_signal_emit (self, signals[SIGNAL_CALL_REMOVE], 0, call, origin);
}

static void
ussd_added_cb (CallsManager *self,
               char         *response,
               CallsUssd    *ussd)
{
  g_assert (CALLS_IS_MANAGER (self));
  g_assert (CALLS_IS_USSD (ussd));

  g_signal_emit (self, signals[USSD_ADDED], 0, ussd, response);
}

static void
ussd_cancelled_cb (CallsManager *self,
                   CallsUssd    *ussd,
                   char         *response)
{
  g_assert (CALLS_IS_MANAGER (self));
  g_assert (CALLS_IS_USSD (ussd));

  g_signal_emit (self, signals[USSD_CANCELLED], 0, ussd);
}

static void
ussd_state_changed_cb (CallsManager *self,
                       CallsUssd    *ussd)
{
  g_assert (CALLS_IS_MANAGER (self));
  g_assert (CALLS_IS_USSD (ussd));

  g_signal_emit (self, signals[USSD_STATE_CHANGED], 0, ussd);
}

static void
update_country_code_cb (CallsOrigin  *origin,
                        GParamSpec   *pspec,
                        CallsManager *self)
{
  g_autofree char *country_code = NULL;

  g_assert (CALLS_IS_MANAGER (self));

  g_object_get (G_OBJECT (origin), "country-code", &country_code, NULL);

  if (!country_code || !*country_code)
    return;

  calls_settings_set_country_code (self->settings, country_code);
}

static void
add_origin (CallsManager *self, CallsOrigin *origin)
{
  g_autofree const char *name = NULL;
  g_assert (CALLS_IS_MANAGER (self));
  g_assert (CALLS_IS_ORIGIN (origin));

  name = calls_origin_get_name (origin);
  g_debug ("Adding origin %s (%p)", name, origin);

  g_list_store_append (self->origins, origin);

  g_signal_connect_object (origin,
                           "notify::country-code",
                           G_CALLBACK (update_country_code_cb),
                           self,
                           G_CONNECT_AFTER);
  g_signal_connect_swapped (origin, "call-added", G_CALLBACK (add_call), self);
  g_signal_connect_swapped (origin, "call-removed", G_CALLBACK (remove_call), self);

  if (CALLS_IS_USSD (origin))
    {
      g_signal_connect_swapped (origin, "ussd-added", G_CALLBACK (ussd_added_cb), self);
      g_signal_connect_swapped (origin, "ussd-cancelled", G_CALLBACK (ussd_cancelled_cb), self);
      g_signal_connect_swapped (origin, "ussd-state-changed", G_CALLBACK (ussd_state_changed_cb), self);
    }

  calls_origin_foreach_call (origin, (CallsOriginForeachCallFunc) add_call, self);
}

static void
remove_call_cb (gpointer self, CallsCall *call, CallsOrigin *origin)
{
  remove_call (self, call, NULL, origin);
}

static void
remove_origin (CallsManager *self, CallsOrigin *origin)
{
  g_autofree const char *name = NULL;
  guint position;

  g_assert (CALLS_IS_MANAGER (self));
  g_assert (CALLS_IS_ORIGIN (origin));

  name = calls_origin_get_name (origin);
  g_debug ("Removing origin %s (%p)", name, origin);

  g_signal_handlers_disconnect_by_data (origin, self);

  calls_origin_foreach_call (origin, remove_call_cb, self);

  if (!g_list_store_find (self->origins, origin, &position))
    g_warning ("Origin %p not found in list store while trying to remove it",
               origin);
  else
    g_list_store_remove (self->origins, position);

  update_state (self);
}

/* rebuild_origins_by_protocols() when any origins were added or removed */
static void
rebuild_origins_by_protocols (CallsManager *self)
{
  GHashTableIter iter;
  gpointer key, value;
  guint n_origins;

  g_assert (CALLS_IS_MANAGER (self));

  /* Remove everything */
  g_hash_table_iter_init (&iter, self->origins_by_protocol);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    GListStore *store = G_LIST_STORE (value);
    g_list_store_remove_all (store);
  }

  /* Iterate over all origins and check which protocols they support */
  n_origins = g_list_model_get_n_items (G_LIST_MODEL (self->origins));

  for (guint i = 0; i < n_origins; i++) {
    g_autoptr (CallsOrigin) origin =
      g_list_model_get_item (G_LIST_MODEL (self->origins), i);

    for (guint j = 0; j < self->supported_protocols->len; j++) {
      char *protocol = g_ptr_array_index (self->supported_protocols, j);
      GListStore *store =
        G_LIST_STORE (g_hash_table_lookup (self->origins_by_protocol, protocol));

      g_assert (store);

      if (calls_origin_supports_protocol (origin, protocol))
        g_list_store_append (store, origin);
    }
  }
}

static void
remove_provider (CallsManager *self,
                 const char   *name)
{
  GListModel *origins;
  guint n_items;
  g_autoptr (CallsProvider) provider = NULL;

  g_assert (CALLS_IS_MANAGER (self));
  g_assert (name);

  provider = g_hash_table_lookup (self->providers, name);
  if (provider) {
    /* Hold a ref since g_hash_table_remove () might drop the last one */
    g_object_ref (provider);
  } else {
    g_warning ("Trying to remove provider %s which has not been found", name);
    return;
  }

  g_debug ("Remove provider: %s", name);
  g_signal_handlers_disconnect_by_data (provider, self);

  origins = calls_provider_get_origins (provider);
  g_signal_handlers_disconnect_by_data (origins, self);
  n_items = g_list_model_get_n_items (origins);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(CallsOrigin) origin = NULL;

    origin = g_list_model_get_item (origins, i);
    remove_origin (self, origin);
  }

  g_hash_table_remove (self->providers, name);
  calls_provider_unload_plugin (name);

  update_protocols (self);
  update_state (self);
  rebuild_origins_by_protocols (self);

  g_signal_emit (self, signals[PROVIDERS_CHANGED], 0);
}

static gboolean
origin_found_in_any_provider (CallsManager *self,
                              CallsOrigin  *origin)
{
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (CALLS_IS_MANAGER (self), FALSE);
  g_return_val_if_fail (CALLS_IS_ORIGIN (origin), FALSE);

  g_hash_table_iter_init (&iter, self->providers);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    guint position;
    CallsProvider *provider = CALLS_PROVIDER (value);
    GListModel *origins = calls_provider_get_origins (provider);

    if (origins && calls_find_in_store (origins,
                                        origin,
                                        &position))
      return TRUE;
  }

  return FALSE;
}



static void
origin_items_changed_cb (GListModel   *model,
                         guint         position,
                         guint         removed,
                         guint         added,
                         CallsManager *self)
{
  guint i;
  CallsOrigin *origin;
  guint purged = 0;
  guint total_origins;

  g_assert (CALLS_IS_MANAGER (self));

  total_origins = g_list_model_get_n_items (G_LIST_MODEL (self->origins));
  g_debug ("origins changed: pos=%d rem=%d added=%d total=%d",
           position, removed, added, g_list_model_get_n_items (model));

  /* Check stale/removed origins: We need to look up */
  if (removed == 0)
    goto skip_remove;

  for (i = 0; i < total_origins - purged; i++) {
    origin = g_list_model_get_item (G_LIST_MODEL (self->origins), i - purged);

    if (!origin_found_in_any_provider (self, origin)) {
      remove_origin (self, origin);
      purged++;
    }
  }

  /** The number of purged entries from self->origins must be equal to removed
   *  origins from the providers list
   */
  if (purged != removed) {
    g_warning ("Managed origins are not in sync anymore!");
  }

 skip_remove:
  for (i = 0; i < added; i++) {
    g_debug ("before adding: %d",
             g_list_model_get_n_items (G_LIST_MODEL (self->origins)));

    origin = g_list_model_get_item (model, position + i);
    add_origin (self, origin); // add to list store
    g_object_unref (origin);

    g_debug ("after adding: %d",
             g_list_model_get_n_items (G_LIST_MODEL (self->origins)));
  }

  rebuild_origins_by_protocols (self);
  update_state (self);
}

static void
add_provider (CallsManager *self, const gchar *name)
{
  GListModel *origins;
  CallsProvider *provider;
  guint n_items;

  g_assert (CALLS_IS_MANAGER (self));
  g_assert (name);

  if (g_hash_table_lookup (self->providers, name))
    return;

  provider = calls_provider_load_plugin (name);
  if (provider == NULL) {
    g_warning ("Could not load a plugin with name `%s'", name);
    return;
  }

  g_hash_table_insert (self->providers, g_strdup (name), provider);

  update_protocols (self);

  origins = calls_provider_get_origins (provider);

  g_signal_connect_object (origins, "items-changed",
                           G_CALLBACK (origin_items_changed_cb), self,
                           G_CONNECT_AFTER);

  n_items = g_list_model_get_n_items (origins);
  origin_items_changed_cb (origins, 0, 0, n_items, self);

  g_signal_emit (self, signals[PROVIDERS_CHANGED], 0);
}

static void
calls_manager_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  CallsManager *self = CALLS_MANAGER (object);

  switch (property_id) {
  case PROP_STATE:
    g_value_set_enum (value, calls_manager_get_state (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_manager_finalize (GObject *object)
{
  CallsManager *self = CALLS_MANAGER (object);

  g_clear_object (&self->origins);
  g_clear_object (&self->contacts_provider);
  g_clear_object (&self->settings);

  g_clear_pointer (&self->providers, g_hash_table_unref);
  g_clear_pointer (&self->origins_by_protocol, g_hash_table_unref);
  g_clear_pointer (&self->supported_protocols, g_ptr_array_unref);

  G_OBJECT_CLASS (calls_manager_parent_class)->finalize (object);
}


static void
calls_manager_class_init (CallsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_manager_get_property;
  object_class->finalize = calls_manager_finalize;

  signals[SIGNAL_CALL_ADD] =
   g_signal_new ("call-add",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_FIRST,
                 0,
                 NULL, NULL, NULL,
                 G_TYPE_NONE,
                 2,
                 CALLS_TYPE_CALL,
                 CALLS_TYPE_ORIGIN);

  signals[SIGNAL_CALL_REMOVE] =
   g_signal_new ("call-remove",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_FIRST,
                 0,
                 NULL, NULL, NULL,
                 G_TYPE_NONE,
                 2,
                 CALLS_TYPE_CALL,
                 CALLS_TYPE_ORIGIN);

  signals[SIGNAL_ERROR] =
   g_signal_new ("error",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_FIRST,
                 0,
                 NULL, NULL, NULL,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_STRING);

  signals[USSD_ADDED] =
    g_signal_new ("ussd-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  CALLS_TYPE_USSD,
                  G_TYPE_STRING);

  signals[USSD_CANCELLED] =
    g_signal_new ("ussd-cancelled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  CALLS_TYPE_USSD);

  signals[USSD_STATE_CHANGED] =
    g_signal_new ("ussd-state-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  CALLS_TYPE_USSD);

  signals[PROVIDERS_CHANGED] =
    g_signal_new ("providers-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  props[PROP_STATE] =
    g_param_spec_enum ("state",
                       "state",
                       "The state of the Manager",
                       CALLS_TYPE_MANAGER_STATE,
                       CALLS_MANAGER_STATE_NO_PROVIDER,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_manager_init (CallsManager *self)
{
  PeasEngine *peas;
  const gchar *dir;

  self->state = CALLS_MANAGER_STATE_NO_PROVIDER;
  self->providers = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           g_object_unref);

  self->origins_by_protocol = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     g_free,
                                                     g_object_unref);

  self->origins = g_list_store_new (calls_origin_get_type ());
  self->supported_protocols = g_ptr_array_new_full (5, g_free);

  self->settings = calls_settings_new ();
  // Load the contacts provider
  self->contacts_provider = calls_contacts_provider_new (self->settings);

  peas = peas_engine_get_default ();

  dir = g_getenv ("CALLS_PLUGIN_DIR");
  if (dir && dir[0] != '\0') {
    /** Add the directory to the search path. prepend_search_path() does not work
     * as expected. see https://gitlab.gnome.org/GNOME/libpeas/-/issues/19
     */
    g_debug ("Adding %s to plugin search path", dir);
    peas_engine_add_search_path (peas, dir, NULL);
  }

  peas_engine_add_search_path (peas, PLUGIN_LIBDIR, NULL);
  g_debug ("Scanning for plugins in `%s'", PLUGIN_LIBDIR);
}


CallsManager *
calls_manager_new (void)
{
  return g_object_new (CALLS_TYPE_MANAGER, NULL);
}

CallsManager *
calls_manager_get_default (void)
{
  static CallsManager *instance;

  if (instance == NULL) {
    instance = calls_manager_new ();
    g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
  }
  return instance;
}

CallsContactsProvider *
calls_manager_get_contacts_provider (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return self->contacts_provider;
}

void
calls_manager_add_provider (CallsManager *self,
                            const char   *name)
{
  g_return_if_fail (CALLS_IS_MANAGER (self));
  g_return_if_fail (name);

  add_provider (self, name);
}

void
calls_manager_remove_provider (CallsManager *self,
                               const char   *name)
{
  g_return_if_fail (CALLS_IS_MANAGER (self));
  g_return_if_fail (name);

  remove_provider (self, name);
  update_protocols (self);
}

gboolean
calls_manager_has_provider (CallsManager *self,
                            const char   *name)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), FALSE);
  g_return_val_if_fail (name, FALSE);

  return !!g_hash_table_lookup (self->providers, name);
}

gboolean
calls_manager_is_modem_provider (CallsManager *self,
                                 const char   *name)
{
  CallsProvider *provider;

  g_return_val_if_fail (CALLS_IS_MANAGER (self), FALSE);
  g_return_val_if_fail (name, FALSE);

  provider = g_hash_table_lookup (self->providers, name);
  g_return_val_if_fail (provider, FALSE);

  return calls_provider_is_modem (provider);
}


CallsManagerState
calls_manager_get_state (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), CALLS_MANAGER_STATE_UNKNOWN);

  return self->state;
}

GListModel *
calls_manager_get_origins (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return G_LIST_MODEL (self->origins);
}

GList *
calls_manager_get_calls (CallsManager *self)
{
  GListModel *origins = NULL;
  g_autoptr (GList) calls = NULL;
  guint n_items = 0;

  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  origins = calls_manager_get_origins (self);
  if (origins)
    n_items = g_list_model_get_n_items (origins);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr (CallsOrigin) origin = NULL;

      origin = g_list_model_get_item (origins, i);
      calls = g_list_concat (calls, calls_origin_get_calls (origin));
    }

  return g_steal_pointer (&calls);
}

/**
 * calls_manager_hang_up_all_calls:
 * @self: a #CallsManager
 *
 * Hangs up on every call known to @self.
 */
void
calls_manager_hang_up_all_calls (CallsManager *self)
{
  g_autoptr (GList) calls = NULL;
  GList *node;
  CallsCall *call;

  g_return_if_fail (CALLS_IS_MANAGER (self));

  calls = calls_manager_get_calls (self);

  for (node = calls; node; node = node->next)
    {
      call = node->data;
      g_debug ("Hanging up on call %s", calls_call_get_name (call));
      calls_call_hang_up (call);
    }

  g_debug ("Hanged up on all calls");
}

/**
 * calls_manager_has_active_call
 * @self: a #CallsManager
 *
 * Checks if @self has any active call
 *
 * Returns: %TRUE if there are active calls, %FALSE otherwise
 */
gboolean
calls_manager_has_active_call (CallsManager *self)
{
  g_autoptr (GList) calls = NULL;
  GList *node;
  CallsCall *call;

  g_return_val_if_fail (CALLS_IS_MANAGER (self), FALSE);

  calls = calls_manager_get_calls (self);

  for (node = calls; node; node = node->next)
    {
      call = node->data;
      if (calls_call_get_state (call) != CALLS_CALL_STATE_DISCONNECTED)
        return TRUE;
    }

  return FALSE;
}

/**
 * calls_manager_get_suitable_origins:
 * @self: The #CallsManager
 * @target: The target number/address
 *
 * Returns (transfer none): A #GListModel of suitable origins
 */
GListModel *
calls_manager_get_suitable_origins (CallsManager *self,
                                    const char   *target)
{
  const char *protocol;
  GListModel *model;

  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);
  g_return_val_if_fail (target, NULL);

  protocol = get_protocol_from_address_with_fallback (target);

  model = g_hash_table_lookup (self->origins_by_protocol, protocol);
  if (model && G_IS_LIST_MODEL (model))
    return model;

  return NULL;
}

/**
 * calls_manager_has_any_provider:
 * @self: The #CallsManager
 *
 * Returns: %TRUE if any provider is loaded, %FALSE otherwise
 */
gboolean
calls_manager_has_any_provider (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), FALSE);

  return !!g_hash_table_size (self->providers);
}

/**
 * calls_manager_get_provider_names:
 * @self: The #CallsManager
 * @length: (optional) (out): the length of the returned array
 *
 * Retrieves the names of all providers loaded by @self, as an array.
 *
 * You should free the return value with g_free().
 *
 * Returns: (array length=length) (transfer container): a
 * %NULL-terminated array containing the names of providers.
 */
const char **
calls_manager_get_provider_names (CallsManager *self,
                                  guint        *length)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return (const char **) g_hash_table_get_keys_as_array (self->providers, length);
}

/**
 * calls_manager_get_providers:
 * @self: A #CallsManager
 *
 * Get the currently loaded providers
 *
 * Returns: (transfer container): A #GList of #CallsProvider.
 * Use g_list_free() when done using the list.
 */
GList *
calls_manager_get_providers (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return g_hash_table_get_values (self->providers);
}


CallsSettings *
calls_manager_get_settings (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return self->settings;
}
