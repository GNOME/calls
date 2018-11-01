/*
 * Copyright (C) 2018 Purism SPC
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

#include "calls-enumerate-params.h"
#include "calls-provider.h"
#include "calls-origin.h"
#include "calls-call.h"

#include <glib/gi18n.h>
#include <glib-object.h>


typedef enum
{
  CALLS_ENUMERATE_PROVIDER,
  CALLS_ENUMERATE_ORIGIN,
  CALLS_ENUMERATE_CALL,
  CALLS_ENUMERATE_LAST
} CallsEnumerateObjectType;


struct _CallsEnumerateParams
{
  GObject parent_instance;

  gboolean    enumerating;
  gpointer    user_data;
  GHashTable *callbacks[3];
};

G_DEFINE_TYPE (CallsEnumerateParams, calls_enumerate_params, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_USER_DATA,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
calls_enumerate_params_init (CallsEnumerateParams *self)
{
  unsigned i;

  self->enumerating = TRUE;

  for (i = 0; i < CALLS_ENUMERATE_LAST; ++i)
    {
      self->callbacks[i] =
        g_hash_table_new_full (g_str_hash, g_str_equal,
                               g_free, NULL);
    }
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsEnumerateParams *self = CALLS_ENUMERATE_PARAMS (object);

  switch (property_id) {
  case PROP_USER_DATA:
    self->user_data = g_value_get_pointer (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
finalize (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsEnumerateParams *self = CALLS_ENUMERATE_PARAMS (object);

  unsigned i;
  for (i = 0; i < CALLS_ENUMERATE_LAST; ++i)
    {
      g_hash_table_unref (self->callbacks[i]);
    }

  parent_class->finalize (object);
}


static void
calls_enumerate_params_class_init (CallsEnumerateParamsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->finalize = finalize;

  props[PROP_USER_DATA] =
    g_param_spec_pointer ("user-data",
                          _("User data"),
                          _("The pointer to be provided as user data for signal connections"),
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


CallsEnumerateParams *
calls_enumerate_params_new (gpointer user_data)
{
  return g_object_new (CALLS_TYPE_ENUMERATE_PARAMS,
                       "user-data", user_data,
                       NULL);
}


gpointer
calls_enumerate_params_get_user_data (CallsEnumerateParams *self)
{
  return self->user_data;
}


gboolean
calls_enumerate_params_get_enumerating (CallsEnumerateParams *self)
{
  return self->enumerating;
}


void
calls_enumerate_params_set_enumerating (CallsEnumerateParams *self,
                                        gboolean              enumerating)
{
  self->enumerating = enumerating;
}


static GHashTable *
lookup_callbacks (CallsEnumerateParams *self,
                  GType                 obj_type)
{
  const GType obj_gtypes[3] =
    {
      CALLS_TYPE_PROVIDER,
      CALLS_TYPE_ORIGIN,
      CALLS_TYPE_CALL
    };
  unsigned i;

  for (i = 0; i < CALLS_ENUMERATE_LAST; ++i)
    {
      if (g_type_is_a (obj_type, obj_gtypes[i]))
        {
          return self->callbacks[i];
        }
    }

  g_error ("Unknown GType `%s' converting to Provider enumeration object type",
           g_type_name (obj_type));
}


gboolean
calls_enumerate_params_have_callbacks (CallsEnumerateParams *self,
                                       GType                 obj_type)
{
  GHashTable * const callbacks = lookup_callbacks (self, obj_type);
  return g_hash_table_size (callbacks) > 0;
}


gboolean
calls_enumerate_params_add (CallsEnumerateParams *self,
                            GType                 obj_type,
                            const gchar          *detail,
                            GCallback             callback)
{
  GHashTable * const callbacks = lookup_callbacks (self, obj_type);
  return g_hash_table_insert (callbacks, g_strdup (detail), callback);
}


struct _CallsEnumerateConnectData
{
  gpointer instance;
  gpointer user_data;
};
typedef struct _CallsEnumerateConnectData CallsEnumerateConnectData;


static void
callbacks_connect (const gchar               *detail,
                   GCallback                  callback,
                   CallsEnumerateConnectData *data)
{
  g_signal_connect_swapped (data->instance,
                            detail,
                            callback,
                            data->user_data);
}


void
calls_enumerate_params_connect (CallsEnumerateParams *self,
                                GObject              *object)
{
  GHashTable * const callbacks =
    lookup_callbacks (self, G_TYPE_FROM_INSTANCE (object));
  CallsEnumerateConnectData data;

  data.instance  = object;
  data.user_data = self->user_data;

  g_hash_table_foreach (callbacks,
                        (GHFunc)callbacks_connect,
                        &data);
}


GCallback
calls_enumerate_params_lookup  (CallsEnumerateParams *self,
                                GType                 obj_type,
                                const gchar          *detail)
{
  GHashTable * const callbacks = lookup_callbacks (self, obj_type);
  return g_hash_table_lookup (callbacks, detail);
}
