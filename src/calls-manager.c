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

#include "calls-config.h"

#include "calls-account.h"
#include "calls-contacts-provider.h"
#include "calls-manager.h"
#include "calls-message-source.h"
#include "calls-plugin-manager.h"
#include "calls-provider.h"
#include "calls-settings.h"
#include "calls-ui-call-data.h"
#include "calls-ussd.h"
#include "calls-util.h"

#include "enum-types.h"

#include <glib/gi18n.h>
#include <libpeas.h>

#define LIBFEEDBACK_USE_UNSTABLE_API
#include <libfeedback.h>


/**
 * SECTION:manager
 * @short_description: Central management object
 * @Title: CallsManager
 *
 * #CallsManager is a singleton that manages lists of loaded #CallsProvider,
 * #CallsOrigin and #CallsCall objects. It keeps track of which #CallsOrigin
 * supports which protocol. It also checks which #CallsCall are ongoing and
 * emits signals for the UI and other parts of the application to act on.
 */

static const char * const protocols[] = {
  "tel",
  "sip",
  "sips"
};


static gboolean
match_origin_supports_protocol (gpointer item,
                                gpointer protocol)
{
  g_assert (CALLS_IS_ORIGIN (item));

  return calls_origin_supports_protocol (CALLS_ORIGIN (item), (char *) protocol);
}


struct _CallsManager {
  GObject                parent_instance;

  GListStore            *origins;
  GtkFlattenListModel   *origins_flat;

  /* origins_by_protocol maps protocol names to GListStore's of suitable origins */
  GHashTable            *origins_by_protocol;
  /* dial_actions_by_protocol maps protocol names to GSimpleActions */
  GHashTable            *dial_actions_by_protocol;

  /* map CallsCall to CallsUiCallData */
  GHashTable            *calls;

  CallsContactsProvider *contacts_provider;

  CallsManagerFlags      state_flags;
  CallsSettings         *settings;
};

static void
calls_manager_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}

G_DEFINE_TYPE_WITH_CODE (CallsManager, calls_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_manager_message_source_interface_init))

enum {
  PROP_0,
  PROP_STATE_FLAGS,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


enum {
  UI_CALL_ADDDED,
  UI_CALL_REMOVED,
  USSD_ADDED,
  USSD_CANCELLED,
  USSD_STATE_CHANGED,
  PROVIDERS_CHANGED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];


static void
set_state_flags (CallsManager *self, CallsManagerFlags state_flags)
{
  if (self->state_flags == state_flags)
    return;

  self->state_flags = state_flags;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE_FLAGS]);
}


static void
update_state_flags (CallsManager *self)
{
  CallsManagerFlags state_flags = CALLS_MANAGER_FLAGS_UNKNOWN;
  GListModel *providers;
  guint n_providers;

  g_assert (CALLS_IS_MANAGER (self));

  providers = calls_plugin_manager_get_providers (calls_plugin_manager_get_default ());

  n_providers = g_list_model_get_n_items (providers);
  for (guint i = 0; i < n_providers; i++) {
    g_autoptr (CallsProvider) provider = g_list_model_get_item (providers, i);

    if (calls_provider_is_modem (provider)) {
      state_flags |= CALLS_MANAGER_FLAGS_HAS_CELLULAR_PROVIDER;

      if (calls_provider_is_operational (provider))
        state_flags |= CALLS_MANAGER_FLAGS_HAS_CELLULAR_MODEM;
    } else {
      state_flags |= CALLS_MANAGER_FLAGS_HAS_VOIP_PROVIDER;

      if (calls_provider_is_operational (provider))
        state_flags |= CALLS_MANAGER_FLAGS_HAS_VOIP_ACCOUNT;
    }
  }
  set_state_flags (self, state_flags);
}


static void
on_dial_protocol_activated (GSimpleAction *action,
                            GVariant      *parameter,
                            CallsManager  *self)
{
  GApplication *application;
  CallsOrigin *origin;
  g_autofree char *target = NULL;
  g_autofree char *origin_id = NULL;

  g_variant_get (parameter, "(ss)", &target, &origin_id);
  origin = calls_manager_get_origin_by_id (self, origin_id);

  if (origin) {
    calls_origin_dial (origin, target);
    return;
  }

  if (origin_id && *origin_id)
    g_debug ("Origin ID '%s' given, but was not found for call to '%s'",
             origin_id, target);

  /* fall back to the default action if we could not determine origin to place call from */
  application = g_application_get_default ();
  if (!application) {
    g_warning ("Could not get default application, cannot activate action '%s'",
               g_action_get_name (G_ACTION (action)));
    return;
  }

  g_action_group_activate_action (G_ACTION_GROUP (application),
                                  "dial",
                                  g_variant_new_string (target));
}


static void
update_protocol_dial_actions (CallsManager *self)
{
  g_assert (CALLS_IS_MANAGER (self));

  for (guint i = 0; i < G_N_ELEMENTS (protocols); i++) {
    GSimpleAction *action = g_hash_table_lookup (self->dial_actions_by_protocol,
                                                 protocols[i]);
    GListModel *protocol_origin = g_hash_table_lookup (self->origins_by_protocol,
                                                       protocols[i]);
    /* TODO take into account if origin is active: modem registered or VoIP account online */
    gboolean action_enabled = g_list_model_get_n_items (protocol_origin) > 0;

    g_simple_action_set_enabled (action, action_enabled);
  }
}


static void
on_message (CallsMessageSource *source,
            const char         *message,
            GtkMessageType      message_type,
            CallsManager       *self)
{
  g_autofree char *notification = NULL;

  g_assert (CALLS_IS_MESSAGE_SOURCE (source));
  g_assert (CALLS_IS_MANAGER (self));

  /* Prefix the message with the name of the source, if known */
  if (CALLS_IS_ACCOUNT (source)) {
    notification = g_strdup_printf ("%s: %s",
                                    calls_account_get_address (CALLS_ACCOUNT (source)),
                                    message);
  }

  calls_message_source_emit_message (CALLS_MESSAGE_SOURCE (self),
                                     notification ?: message,
                                     message_type);
}


static void
add_call (CallsManager *self, CallsCall *call, CallsOrigin *origin)
{
  CallsUiCallData *call_data;
  g_autofree char *origin_id = NULL;

  g_return_if_fail (CALLS_IS_MANAGER (self));
  g_return_if_fail (CALLS_IS_ORIGIN (origin));
  g_return_if_fail (CALLS_IS_CALL (call));

  origin_id = calls_origin_get_id (origin);
  call_data = calls_ui_call_data_new (call, origin_id);
  g_hash_table_insert (self->calls, call, call_data);

  g_object_set_data (G_OBJECT (call), "call-origin", origin);

  g_signal_emit (self, signals[UI_CALL_ADDDED], 0, call_data);
}


struct CallsRemoveData {
  CallsManager *manager;
  CallsCall    *call;
};


static gboolean
on_remove_delayed (struct CallsRemoveData *data)
{
  CallsUiCallData *call_data;

  g_assert (CALLS_IS_MANAGER (data->manager));
  g_assert (CALLS_IS_CALL (data->call));

  call_data = g_hash_table_lookup (data->manager->calls, data->call);
  if (!call_data) {
    g_warning ("Could not find call %s in UI call hash table",
               calls_call_get_id (data->call));
  } else {
    g_signal_emit (data->manager, signals[UI_CALL_REMOVED], 0, call_data);
    g_hash_table_remove (data->manager->calls, data->call);
  }

  g_free (data);

  return G_SOURCE_REMOVE;
}


#define DELAY_CALL_REMOVE_MS 3000
static void
remove_call (CallsManager *self, CallsCall *call, gchar *reason, CallsOrigin *origin)
{
  struct CallsRemoveData *data;

  g_return_if_fail (CALLS_IS_MANAGER (self));
  g_return_if_fail (CALLS_IS_ORIGIN (origin));
  g_return_if_fail (CALLS_IS_CALL (call));

  data = g_new (struct CallsRemoveData, 1);
  data->manager = self;
  data->call = call;

  /** Having the delay centrally managed makes sure our UI
   *  and the DBus side stays consistent
   */
  g_timeout_add (DELAY_CALL_REMOVE_MS,
                 G_SOURCE_FUNC (on_remove_delayed),
                 data);

  /* Notify that remote side hung up */
  if (!calls_call_get_we_hung_up (call) && lfb_is_initted ()) {
    LfbEvent *event = lfb_event_new ("phone-hangup");

    lfb_event_trigger_feedback_async (event, NULL, NULL, NULL);
  }
}
#undef DELAY_CALL_REMOVE_MS


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

  g_signal_connect_object (origin,
                           "message",
                           G_CALLBACK (on_message),
                           self,
                           G_CONNECT_AFTER);

  g_signal_connect_object (origin,
                           "notify::country-code",
                           G_CALLBACK (update_country_code_cb),
                           self,
                           G_CONNECT_AFTER);
  g_signal_connect_object (origin, "call-added", G_CALLBACK (add_call), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (origin, "call-removed", G_CALLBACK (remove_call), self, G_CONNECT_SWAPPED);

  if (CALLS_IS_USSD (origin)) {
    g_signal_connect_object (origin, "ussd-added", G_CALLBACK (ussd_added_cb), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (origin, "ussd-cancelled", G_CALLBACK (ussd_cancelled_cb), self, G_CONNECT_SWAPPED);
    g_signal_connect_object (origin, "ussd-state-changed", G_CALLBACK (ussd_state_changed_cb), self, G_CONNECT_SWAPPED);
  }

  calls_origin_foreach_call (origin, (CallsOriginForeachCallFunc) add_call, self);
}

static void
on_origins_changed (GListModel   *model,
                    guint         position,
                    guint         removed,
                    guint         added,
                    CallsManager *self)
{
  g_assert (CALLS_IS_MANAGER (self));
  g_assert (model == G_LIST_MODEL (self->origins_flat));

  for (guint i = 0; i < added; i++) {
    g_autoptr (CallsOrigin) origin = g_list_model_get_item (model, position + i);

    add_origin (self, origin);
  }

  update_state_flags (self);
  update_protocol_dial_actions (self);
}


static void
calls_manager_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  CallsManager *self = CALLS_MANAGER (object);

  switch (property_id) {
  case PROP_STATE_FLAGS:
    g_value_set_flags (value, calls_manager_get_state_flags (self));
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

  g_clear_object (&self->contacts_provider);
  g_clear_object (&self->origins);
  g_clear_object (&self->origins_flat);

  g_clear_pointer (&self->origins_by_protocol, g_hash_table_unref);
  g_clear_pointer (&self->dial_actions_by_protocol, g_hash_table_unref);
  g_clear_pointer (&self->calls, g_hash_table_unref);

  G_OBJECT_CLASS (calls_manager_parent_class)->finalize (object);
}


static void
calls_manager_class_init (CallsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_manager_get_property;
  object_class->finalize = calls_manager_finalize;

  signals[UI_CALL_ADDDED] =
    g_signal_new ("ui-call-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  CALLS_TYPE_UI_CALL_DATA);

  signals[UI_CALL_REMOVED] =
    g_signal_new ("ui-call-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  CALLS_TYPE_UI_CALL_DATA);

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

  props[PROP_STATE_FLAGS] =
    g_param_spec_flags ("state",
                        "state",
                        "The state of the Manager",
                        CALLS_TYPE_MANAGER_FLAGS,
                        CALLS_MANAGER_FLAGS_UNKNOWN,
                        G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
on_providers_changed (GListModel   *model,
                      guint         position,
                      guint         removed,
                      guint         added,
                      CallsManager *self)
{
  g_autoptr (GPtrArray) origins_add = g_ptr_array_sized_new (added);

  g_assert (CALLS_IS_MANAGER (self));

  g_debug ("Provider changed at pos %u; removed %u; added %u",
           position, removed, added);

  for (guint i = 0; i < added; i++) {
    g_autoptr (CallsProvider) provider =
      g_list_model_get_item (model, position + i);

    g_ptr_array_add (origins_add, calls_provider_get_origins (provider));
    g_debug ("Adding provider %s with %u items",
             calls_provider_get_name (provider),
             g_list_model_get_n_items (calls_provider_get_origins (provider)));
  }

  g_list_store_splice (self->origins,
                       position,
                       removed,
                       origins_add->pdata,
                       added);

  if (removed > 0) {
    GHashTableIter calls_iter;
    gpointer key;

    g_hash_table_iter_init (&calls_iter, self->calls);
    while (g_hash_table_iter_next (&calls_iter, &key, NULL)) {
      gpointer origin = g_object_get_data (G_OBJECT (key), "call-origin");

      if (!calls_find_in_model (G_LIST_MODEL (self->origins_flat), origin, NULL))
        calls_call_hang_up (CALLS_CALL (key));
    }
  }

  update_state_flags (self);
}


static void
calls_manager_init (CallsManager *self)
{
  g_autoptr (GVariantType) variant_type = NULL;
  GApplication *application;
  CallsPluginManager *plugin_manager = calls_plugin_manager_get_default ();
  GListModel *providers;

  self->state_flags = CALLS_MANAGER_FLAGS_UNKNOWN;

  self->origins = g_list_store_new (G_TYPE_LIST_MODEL); /* list of lists */
  self->origins_flat = gtk_flatten_list_model_new (G_LIST_MODEL (g_object_ref (self->origins)));

  providers = calls_plugin_manager_get_providers (plugin_manager);
  g_signal_connect_object (providers,
                           "items-changed",
                           G_CALLBACK (on_providers_changed),
                           self,
                           0); /* G_CONNECT_DEFAULT */
  if (g_list_model_get_n_items (providers) > 0)
    on_providers_changed (providers, 0, 0, g_list_model_get_n_items (providers), self);

  self->origins_by_protocol = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     NULL,
                                                     g_object_unref);

  for (guint i = 0; i < G_N_ELEMENTS (protocols); i++) {
    GtkFilter* filter =
      GTK_FILTER (gtk_custom_filter_new (match_origin_supports_protocol, (gpointer) protocols[i], NULL));
    GtkFilterListModel *f_list =
      gtk_filter_list_model_new (G_LIST_MODEL (g_object_ref (self->origins_flat)), filter);

    g_debug ("Adding filter list model for protocol '%s'", protocols[i]);
    g_hash_table_insert (self->origins_by_protocol,
                         (gpointer) protocols[i],
                         f_list);
  }

  g_signal_connect_object (self->origins_flat,
                           "items-changed",
                           G_CALLBACK (on_origins_changed),
                           self,
                           0);

  self->dial_actions_by_protocol = g_hash_table_new_full (g_str_hash,
                                                          g_str_equal,
                                                          NULL,
                                                          g_object_unref);

  application = g_application_get_default ();
  variant_type = g_variant_type_new ("(ss)");

  for (guint i = 0; i < G_N_ELEMENTS (protocols); i++) {
    g_autofree char *action_name = g_strdup_printf ("dial-%s", protocols[i]);
    GSimpleAction *action = g_simple_action_new (action_name, variant_type);
    g_signal_connect (action,
                      "activate",
                      G_CALLBACK (on_dial_protocol_activated),
                      self);

    g_hash_table_insert (self->dial_actions_by_protocol,
                         (gpointer) protocols[i],
                         g_object_ref (action));

    /* Enable action if there are suitable origins */
    g_simple_action_set_enabled (action, FALSE);

    /* application can be NULL when running tests */
    if (application)
      g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));
  }

  /* This hash table only owns the value, not the key */
  self->calls = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

  self->settings = calls_settings_get_default ();
  /* Load the contacts provider */
  self->contacts_provider = calls_contacts_provider_new ();
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
    g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *) &instance);
  }
  return instance;
}


CallsContactsProvider *
calls_manager_get_contacts_provider (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return self->contacts_provider;
}


CallsManagerFlags
calls_manager_get_state_flags (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), CALLS_MANAGER_FLAGS_UNKNOWN);

  return self->state_flags;
}


GListModel *
calls_manager_get_origins (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return G_LIST_MODEL (self->origins_flat);
}


/**
 * calls_manager_get_calls:
 * @self: A #CallsManager
 *
 * Returns: (transfer container): Returns a list of all known calls.
 * The calls are objects of type #CallsUiCallData. Use g_list_free() when
 * done using the list.
 */
GList *
calls_manager_get_calls (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return g_hash_table_get_values (self->calls);
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
  GListModel *origins;
  GList *node;
  CallsCall *call;
  uint n_items;

  g_return_if_fail (CALLS_IS_MANAGER (self));

  origins = G_LIST_MODEL (self->origins_flat);
  n_items = g_list_model_get_n_items (origins);

  for (uint i = 0; i < n_items; i++) {
    g_autoptr (CallsOrigin) origin = g_list_model_get_item (origins, i);
    g_autoptr (GList) calls = calls_origin_get_calls (origin);

    for (node = calls; node; node = node->next) {
      call = node->data;
      g_debug ("Hanging up on call %s", calls_call_get_name (call));
      calls_call_hang_up (call);
    }
  }

  g_debug ("Hung up on all calls");
}

/**
 * calls_manager_get_suitable_origins:
 * @self: The #CallsManager
 * @target: The target number/address
 *
 * Returns (transfer none): A #GListModel of suitable origins as long
 * as the protocol to be used for @target is supported, %NULL otherwise
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


CallsSettings *
calls_manager_get_settings (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return self->settings;
}

/**
 * calls_manager_get_origin_by_id:
 * @self: The #CallsManager
 * @origin_id: The id to use for the lookup
 *
 * Returns: (transfer none): The #CallsOrigin if found, %NULL otherwise
 */
CallsOrigin *
calls_manager_get_origin_by_id (CallsManager *self,
                                const char   *origin_id)
{
  uint n_origins;

  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  /* TODO Turn this into a critical once https://gitlab.gnome.org/GNOME/calls/-/merge_requests/505 is in */
  if (STR_IS_NULL_OR_EMPTY (origin_id))
    return NULL;

  n_origins = g_list_model_get_n_items (G_LIST_MODEL (self->origins_flat));
  for (uint i = 0; i < n_origins; i++) {
    g_autoptr (CallsOrigin) origin =
      g_list_model_get_item (G_LIST_MODEL (self->origins_flat), i);
    g_autofree char *id = calls_origin_get_id (origin);

    if (g_strcmp0 (id, origin_id) == 0)
      return origin;
  }

  return NULL;
}
