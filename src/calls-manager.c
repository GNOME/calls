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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "config.h"
#include "calls-ussd.h"
#include "calls-manager.h"
#include "calls-contacts.h"
#include "enum-types.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

struct _CallsManager
{
  GObject parent_instance;

  CallsProvider *provider;
  gchar *provider_name;
  CallsOrigin *default_origin;
  CallsManagerState state;
};

G_DEFINE_TYPE (CallsManager, calls_manager, G_TYPE_OBJECT);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EPhoneNumber, e_phone_number_free)

enum {
  PROP_0,
  PROP_PROVIDER,
  PROP_DEFAULT_ORIGIN,
  PROP_STATE,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


enum {
  SIGNAL_ORIGIN_ADD,
  SIGNAL_ORIGIN_REMOVE,
  SIGNAL_CALL_ADD,
  SIGNAL_CALL_REMOVE,
  /* TODO: currently this event isn't emitted since the plugins don't give use
   * a usable error or error message. */
  SIGNAL_ERROR,
  USSD_ADDED,
  USSD_CANCELLED,
  USSD_STATE_CHANGED,
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

static CallsProvider *
load_provider (const gchar* name)
{
  g_autoptr (GError) error = NULL;
  PeasEngine *plugins;
  PeasPluginInfo *info;
  PeasExtension *extension;

  // Add Calls search path and rescan
  plugins = peas_engine_get_default ();
  peas_engine_add_search_path (plugins, PLUGIN_LIBDIR, NULL);
  g_debug ("Scanning for plugins in `%s'", PLUGIN_LIBDIR);

  // Find the plugin
  info = peas_engine_get_plugin_info (plugins, name);
  if (!info)
    {
      g_debug ("Could not find plugin `%s'", name);
      return NULL;
    }

  // Possibly load the plugin
  if (!peas_plugin_info_is_loaded (info))
    {
      peas_engine_load_plugin (plugins, info);

      if (!peas_plugin_info_is_available (info, &error))
        {
          g_debug ("Error loading plugin `%s': %s", name, error->message);
          return NULL;
        }

      g_debug ("Loaded plugin `%s'", name);
    }

  // Check the plugin provides CallsProvider
  if (!peas_engine_provides_extension (plugins, info, CALLS_TYPE_PROVIDER))
    {
      g_debug ("Plugin `%s' does not have a provider extension", name);
      return NULL;
    }

  // Get the extension
  extension = peas_engine_create_extensionv (plugins, info, CALLS_TYPE_PROVIDER, 0, NULL);
  if (!extension)
    {
      g_debug ("Could not create provider from plugin `%s'", name);
      return NULL;
    }

  g_debug ("Created provider from plugin `%s'", name);
  return CALLS_PROVIDER (extension);
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
add_origin (CallsManager *self, CallsOrigin *origin, CallsProvider *provider)
{
  g_return_if_fail (CALLS_IS_ORIGIN (origin));

  g_signal_connect_swapped (origin, "call-added", G_CALLBACK (add_call), self);
  g_signal_connect_swapped (origin, "call-removed", G_CALLBACK (remove_call), self);

  if (CALLS_IS_USSD (origin))
    {
      g_signal_connect_swapped (origin, "ussd-added", G_CALLBACK (ussd_added_cb), self);
      g_signal_connect_swapped (origin, "ussd-cancelled", G_CALLBACK (ussd_cancelled_cb), self);
      g_signal_connect_swapped (origin, "ussd-state-changed", G_CALLBACK (ussd_state_changed_cb), self);
    }

  calls_origin_foreach_call(origin, (CallsOriginForeachCallFunc)add_call, self);

  set_state (self, CALLS_MANAGER_STATE_READY);
  g_signal_emit (self, signals[SIGNAL_ORIGIN_ADD], 0, origin);
}

static void
remove_call_cb (gpointer self, CallsCall *call, CallsOrigin *origin)
{
  remove_call(self, call, NULL, origin);
}

static void
remove_origin (CallsManager *self, CallsOrigin *origin, CallsProvider *provider)
{
  g_autoptr (GList) origins = NULL;

  g_return_if_fail (CALLS_IS_ORIGIN (origin));

  g_signal_handlers_disconnect_by_data (origin, self);

  calls_origin_foreach_call(origin, remove_call_cb, self);

  if (self->default_origin == origin)
    calls_manager_set_default_origin (self, NULL);

  origins = calls_manager_get_origins (self);
   if (origins == NULL)
     set_state (self, CALLS_MANAGER_STATE_NO_ORIGIN);

  g_signal_emit (self, signals[SIGNAL_ORIGIN_REMOVE], 0, origin);
}

static void
remove_provider (CallsManager *self)
{
  PeasEngine *engine = peas_engine_get_default ();
  PeasPluginInfo *plugin = peas_engine_get_plugin_info (engine, self->provider_name);
  g_autoptr (GList) origins = NULL;
  GList *o;

  g_debug ("Remove provider: %s", calls_provider_get_name (self->provider));
  g_signal_handlers_disconnect_by_data (self->provider, self);

  origins = calls_provider_get_origins (self->provider);

  for (o = origins; o != NULL; o = o->next)
    {
      remove_origin (self, o->data, self->provider);
    }

  g_clear_pointer (&self->provider_name, g_free);
  peas_engine_unload_plugin (engine, plugin);
  set_state (self, CALLS_MANAGER_STATE_NO_PROVIDER);
}

static void
add_provider (CallsManager *self, const gchar *name)
{
  g_autoptr (GList) origins = NULL;
  GList *o;

  /* We could eventually enable more then one provider, but for now let's use
     only one */
  if (self->provider != NULL)
    remove_provider (self);

  if (name == NULL)
    return;

  self->provider = load_provider (name);

  if (self->provider == NULL) {
    set_state (self, CALLS_MANAGER_STATE_NO_PLUGIN);
    return;
  }

  set_state (self, CALLS_MANAGER_STATE_NO_ORIGIN);

  origins = calls_provider_get_origins (self->provider);

  g_signal_connect_swapped (self->provider, "origin-added", G_CALLBACK (add_origin), self);
  g_signal_connect_swapped (self->provider, "origin-removed", G_CALLBACK (remove_origin), self);

  for (o = origins; o != NULL; o = o->next)
    {
      add_origin (self, o->data, self->provider);
    }

  self->provider_name = g_strdup (name);
}

static void
calls_manager_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  CallsManager *self = CALLS_MANAGER (object);

  switch (property_id) {
  case PROP_PROVIDER:
    g_value_set_string (value, calls_manager_get_provider (self));
    break;

  case PROP_DEFAULT_ORIGIN:
    g_value_set_object (value, calls_manager_get_default_origin (self));
    break;

  case PROP_STATE:
    g_value_set_enum (value, calls_manager_get_state (self));
    break;
  
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
} 

static void
calls_manager_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  CallsManager *self = CALLS_MANAGER (object);

  switch (property_id) {
  case PROP_PROVIDER:
    calls_manager_set_provider (self, g_value_get_string (value));
    break;

  case PROP_DEFAULT_ORIGIN:
    calls_manager_set_default_origin (self, g_value_get_object (value));
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

  g_clear_object (&self->provider);
  g_clear_pointer (&self->provider_name, g_free);

  G_OBJECT_CLASS (calls_manager_parent_class)->finalize (object);
}


static void
calls_manager_class_init (CallsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_manager_get_property;
  object_class->set_property = calls_manager_set_property;
  object_class->finalize = calls_manager_finalize;

  signals[SIGNAL_ORIGIN_ADD] =
   g_signal_new ("origin-add",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_FIRST,
                 0,
                 NULL, NULL, NULL,
                 G_TYPE_NONE,
                 1,
                 CALLS_TYPE_ORIGIN);

  signals[SIGNAL_ORIGIN_REMOVE] =
   g_signal_new ("origin-remove",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_FIRST,
                 0,
                 NULL, NULL, NULL,
                 G_TYPE_NONE,
                 1,
                 CALLS_TYPE_ORIGIN);

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

  props[PROP_PROVIDER] = g_param_spec_string ("provider",
                                              "provider", 
                                              "The name of the currently loaded provider",
                                              NULL,
                                              G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_STATE] = g_param_spec_enum ("state",
                                         "state",
                                         "The state of the Manager",
                                         CALLS_TYPE_MANAGER_STATE,
                                         CALLS_MANAGER_STATE_NO_PROVIDER,
                                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DEFAULT_ORIGIN] = g_param_spec_object ("default-origin",
                                                    "default origin",
                                                    "The default origin, if any",
                                                    CALLS_TYPE_ORIGIN,
                                                    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_manager_init (CallsManager *self)
{
  self->state = CALLS_MANAGER_STATE_NO_PROVIDER;
  self->provider_name = NULL;
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

const gchar *
calls_manager_get_provider (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return self->provider_name;
}

void
calls_manager_set_provider (CallsManager *self, const gchar *name)
{
  g_return_if_fail (CALLS_IS_MANAGER (self));

  if (self->provider != NULL && g_strcmp0 (calls_provider_get_name (self->provider), name) == 0)
    return;

  add_provider (self, name);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROVIDER]);
}

/* FIXME: This function should be removed since we don't want to hand out the
   provider */
CallsProvider *
calls_manager_get_real_provider (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return self->provider;
}

CallsManagerState
calls_manager_get_state (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), CALLS_MANAGER_STATE_UNKNOWN);

  return self->state;
}

GList *
calls_manager_get_origins (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  if (self->provider == NULL)
    return NULL;

  return calls_provider_get_origins (self->provider);
}

GList *
calls_manager_get_calls (CallsManager *self)
{
  g_autoptr (GList) origins = NULL;
  g_autoptr (GList) calls = NULL;
  GList *o;

  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  origins = calls_manager_get_origins (self);

  for (o = origins; o != NULL; o = o->next)
    calls = g_list_concat (calls, calls_origin_get_calls (o->data));

  return g_steal_pointer (&calls);
}

CallsOrigin *
calls_manager_get_default_origin (CallsManager *self)
{
  g_return_val_if_fail (CALLS_IS_MANAGER (self), NULL);

  return self->default_origin;
}

void
calls_manager_set_default_origin (CallsManager *self,
                                  CallsOrigin *origin)
{
  g_return_if_fail (CALLS_IS_MANAGER (self));

  if (self->default_origin == origin)
    return;

  g_clear_object (&self->default_origin);

  if (origin)
    self->default_origin = g_object_ref (origin);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEFAULT_ORIGIN]);
}

/**
 * calls_manager_get_contact_name:
 * @call: a #CallsCall
 *
 * Looks up the contact name for @call. If the lookup
 * succeeds, the contact name will be returned. NULL if
 * no match has been found in the contact list.
 * If no number is associated with the @call, then
 * a translatable string will be returned.
 *
 * Returns: (transfer none): The caller's name, a string representing
 * an unknown caller or %NULL
 */
const gchar *
calls_manager_get_contact_name (CallsCall *call)
{
  g_autoptr (EPhoneNumber) phone_number = NULL;
  g_autoptr (GError) err = NULL;
  const gchar *number;
  CallsBestMatch *match;

  number = calls_call_get_number (call);
  if (!number || g_strcmp0 (number, "") == 0)
    return _("Anonymous caller");

  phone_number = e_phone_number_from_string (number, NULL, &err);
  if (!phone_number)
    {
      g_warning ("Failed to convert %s to a phone number: %s", number, err->message);
      return NULL;
    }

  match = calls_contacts_lookup_phone_number (calls_contacts_get_default (),
                                              phone_number);
  if (!match)
    return NULL;

  return calls_best_match_get_name (match);
}
