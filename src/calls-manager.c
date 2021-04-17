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
#include "calls-contacts-provider.h"
#include "enum-types.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

struct _CallsManager
{
  GObject parent_instance;

  CallsProvider *provider;
  CallsContactsProvider *contacts_provider;
  gchar *provider_name;
  CallsOrigin *default_origin;
  CallsManagerState state;
  CallsCall *primary_call;
  char *country_code;
  GBinding *country_code_binding;
};

G_DEFINE_TYPE (CallsManager, calls_manager, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_PROVIDER,
  PROP_DEFAULT_ORIGIN,
  PROP_STATE,
  PROP_COUNTRY_CODE,
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
add_call (CallsManager *self, CallsCall *call, CallsOrigin *origin)
{
  g_return_if_fail (CALLS_IS_MANAGER (self));
  g_return_if_fail (CALLS_IS_ORIGIN (origin));
  g_return_if_fail (CALLS_IS_CALL (call));

  g_signal_emit (self, signals[SIGNAL_CALL_ADD], 0, call, origin);

  if (self->primary_call == NULL)
    self->primary_call = call;
  else
    calls_call_hang_up (call);
}

static void
remove_call (CallsManager *self, CallsCall *call, gchar *reason, CallsOrigin *origin)
{
  g_return_if_fail (CALLS_IS_MANAGER (self));
  g_return_if_fail (CALLS_IS_ORIGIN (origin));
  g_return_if_fail (CALLS_IS_CALL (call));

  /* We ignore the reason for now, because it doesn't give any usefull information */
  g_signal_emit (self, signals[SIGNAL_CALL_REMOVE], 0, call, origin);

  if (self->primary_call == call)
    self->primary_call = NULL;
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
add_origin (CallsManager *self, CallsOrigin *origin)
{
  g_assert (CALLS_IS_MANAGER (self));
  g_assert (CALLS_IS_ORIGIN (origin));

  g_debug ("Adding origin %s (%p)", calls_origin_get_name (origin), origin);

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
}

static void
remove_call_cb (gpointer self, CallsCall *call, CallsOrigin *origin)
{
  remove_call(self, call, NULL, origin);
}

static void
remove_origin (CallsManager *self, CallsOrigin *origin)
{
  GListModel *origins;

  g_return_if_fail (CALLS_IS_ORIGIN (origin));

  g_debug ("Removing origin %s (%p)", calls_origin_get_name (origin), origin);

  g_signal_handlers_disconnect_by_data (origin, self);

  calls_origin_foreach_call(origin, remove_call_cb, self);

  if (self->default_origin == origin)
    calls_manager_set_default_origin (self, NULL);

  origins = calls_manager_get_origins (self);
  if (!origins || g_list_model_get_n_items (origins) == 0)
    set_state (self, CALLS_MANAGER_STATE_NO_ORIGIN);
}

static void
remove_provider (CallsManager *self)
{
  GListModel *origins;
  guint n_items;

  g_debug ("Remove provider: %s", calls_provider_get_name (self->provider));
  g_signal_handlers_disconnect_by_data (self->provider, self);

  origins = calls_provider_get_origins (self->provider);
  g_signal_handlers_disconnect_by_data (origins, self);
  n_items = g_list_model_get_n_items (origins);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(CallsOrigin) origin = NULL;

      origin = g_list_model_get_item (origins, i);
      remove_origin (self, origin);
    }

  calls_provider_unload_plugin (self->provider_name);

  g_clear_pointer (&self->provider_name, g_free);
  g_clear_object (&self->provider);
  set_state (self, CALLS_MANAGER_STATE_NO_PROVIDER);
}

static void
origin_items_changed_cb (CallsManager *self)
{
  GListModel *origins;
  guint n_items;
  gboolean has_default_origin = FALSE;

  g_assert (CALLS_IS_MANAGER (self));

  has_default_origin = !!self->default_origin;
  origins = calls_provider_get_origins (self->provider);
  n_items = g_list_model_get_n_items (origins);

  if (n_items)
    set_state (self, CALLS_MANAGER_STATE_READY);
  else
    set_state (self, CALLS_MANAGER_STATE_NO_ORIGIN);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(CallsOrigin) origin = NULL;

      origin = g_list_model_get_item (origins, i);
      add_origin (self, origin);
      if (!has_default_origin)
        {
          /* XXX
            This actually doesn't work correctly when we default origin is removed.
            This will require a rework when supporting multiple providers anyway
            and also isn't really used outside of getting the country code
            it's not really a problem.
          */
          calls_manager_set_default_origin (self, origin);
          has_default_origin = TRUE;
        }
    }
}

static void
add_provider (CallsManager *self, const gchar *name)
{
  GListModel *origins;

  /* We could eventually enable more then one provider, but for now let's use
     only one */
  if (self->provider != NULL)
    remove_provider (self);

  if (name == NULL)
    return;

  self->provider = calls_provider_load_plugin (name);

  if (self->provider == NULL) {
    set_state (self, CALLS_MANAGER_STATE_NO_PLUGIN);
    return;
  }

  if (g_strcmp0 (name, "dummy") == 0)
    set_state (self, CALLS_MANAGER_STATE_READY);
  else
    set_state (self, CALLS_MANAGER_STATE_NO_ORIGIN);

  origins = calls_provider_get_origins (self->provider);

  g_signal_connect_object (origins, "items-changed",
                           G_CALLBACK (origin_items_changed_cb), self,
                           G_CONNECT_SWAPPED);
  origin_items_changed_cb (self);

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

  case PROP_COUNTRY_CODE:
    g_value_set_string (value, self->country_code);
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
calls_manager_finalize (GObject *object)
{
  CallsManager *self = CALLS_MANAGER (object);

  g_clear_object (&self->provider);
  g_clear_pointer (&self->provider_name, g_free);
  g_clear_object (&self->contacts_provider);
  g_clear_pointer (&self->country_code, g_free);

  G_OBJECT_CLASS (calls_manager_parent_class)->finalize (object);
}


static void
calls_manager_class_init (CallsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_manager_get_property;
  object_class->set_property = calls_manager_set_property;
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

  props[PROP_COUNTRY_CODE] = g_param_spec_string ("country-code",
                                                  "country code",
                                                  "The default country code to use",
                                                  NULL,
                                                  G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_manager_init (CallsManager *self)
{
  self->state = CALLS_MANAGER_STATE_NO_PROVIDER;
  self->provider_name = NULL;
  self->primary_call = NULL;

  // Load the contacts provider
  self->contacts_provider = calls_contacts_provider_new ();
  g_object_bind_property (self, "country-code",
                          self->contacts_provider, "country-code",
                          G_BINDING_DEFAULT);
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

  if (self->provider == NULL)
    return NULL;

  return calls_provider_get_origins (self->provider);
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
      g_autoptr(CallsOrigin) origin = NULL;

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

  g_clear_pointer (&self->country_code_binding, g_binding_unbind);

  g_clear_object (&self->default_origin);

  if (origin) {
    self->default_origin = g_object_ref (origin);
    self->country_code_binding =
      g_object_bind_property (origin, "country-code",
                              self, "country-code",
                              G_BINDING_SYNC_CREATE);
  }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEFAULT_ORIGIN]);
}
