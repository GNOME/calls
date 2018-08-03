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

#include "calls-mm-provider.h"
#include "calls-provider.h"
#include "calls-mm-origin.h"
#include "calls-message-source.h"
#include "calls-origin.h"

#include <libmm-glib.h>
#include <glib/gi18n.h>

struct _CallsMMProvider
{
  GObject parent_instance;

  /** D-Bus connection */
  GDBusConnection *connection;
  /** ModemManager object proxy */
  MMManager *mm;
  /** Map of D-Bus object paths to origins */
  GHashTable *origins;
};

static void calls_mm_provider_message_source_interface_init (CallsProviderInterface *iface);
static void calls_mm_provider_provider_interface_init (CallsProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsMMProvider, calls_mm_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_mm_provider_message_source_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_PROVIDER,
                                                calls_mm_provider_provider_interface_init))


enum {
  PROP_0,
  PROP_CONNECTION,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static const gchar *
get_name (CallsProvider *iface)
{
  return "ModemManager";
}


static GList *
get_origins (CallsProvider *iface)
{
  CallsMMProvider *self = CALLS_MM_PROVIDER (iface);
  return g_hash_table_get_values (self->origins);
}


static void
add_origin (CallsMMProvider *self,
            GDBusObject     *object)
{
  MMObject *mm_obj;
  CallsMMOrigin *origin;

  g_debug ("Adding new voice-cable modem `%s'",
           g_dbus_object_get_object_path (object));

  g_assert (MM_IS_OBJECT (object));
  mm_obj = MM_OBJECT (object);

  origin = calls_mm_origin_new (mm_obj);

  g_hash_table_insert (self->origins,
                       mm_object_dup_path (mm_obj),
                       origin);

  g_signal_emit_by_name (CALLS_PROVIDER (self),
                         "origin-added", origin);
}


static void
interface_added_cb (CallsMMProvider *self,
                    GDBusObject     *object,
                    GDBusInterface  *interface)
{
  GDBusInterfaceInfo *info;

  info = g_dbus_interface_get_info (interface);

  if (g_strcmp0 (info->name,
                 "org.freedesktop.ModemManager1.Modem.Voice") == 0)
    {
      add_origin (self, object);
    }
}


static void
remove_origin (CallsMMProvider *self,
               GDBusObject     *object)
{
  const gchar *path;
  gpointer *origin;

  path = g_dbus_object_get_object_path (object);

  origin = g_hash_table_lookup (self->origins, path);
  g_assert (origin != NULL && CALLS_IS_ORIGIN (origin));

  g_signal_emit_by_name (CALLS_PROVIDER (self),
                         "origin-removed", CALLS_ORIGIN (origin));

  g_hash_table_remove (self->origins, path);
}


static void
interface_removed_cb (CallsMMProvider *self,
                      GDBusObject     *object,
                      GDBusInterface  *interface)
{
  GDBusInterfaceInfo *info;

  info = g_dbus_interface_get_info (interface);

  if (g_strcmp0 (info->name,
                 "org.freedesktop.ModemManager1.Modem.Voice") != 0)
    {
      remove_origin (self, object);
    }
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsMMProvider *self = CALLS_MM_PROVIDER (object);

  switch (property_id) {
  case PROP_CONNECTION:
    g_set_object (&self->connection,
                  g_value_get_object (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
add_mm_object (CallsMMProvider *self, GDBusObject *object)
{
  GList *ifaces, *node;

  ifaces = g_dbus_object_get_interfaces (object);
  for (node = ifaces; node; node = node->next)
    {
      interface_added_cb (self, object,
                          G_DBUS_INTERFACE (node->data));
    }

  g_list_free_full (ifaces, g_object_unref);
}


static void
add_mm_objects (CallsMMProvider *self)
{
  GList *objects, *node;

  objects = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (self->mm));
  for (node = objects; node; node = node->next)
    {
      add_mm_object (self, G_DBUS_OBJECT (node->data));
    }

  g_list_free_full (objects, g_object_unref);
}


static void
mm_manager_new_cb (GDBusConnection *connection,
                   GAsyncResult *res,
                   CallsMMProvider *self)
{
  GError *error = NULL;

  self->mm = mm_manager_new_finish (res, &error);
  if (!self->mm)
    {
      g_error ("Error creating ModemManager Manager: %s",
               error->message);
      g_assert_not_reached();
    }


  g_signal_connect_swapped (self->mm, "interface-added",
                            G_CALLBACK (interface_added_cb), self);
  g_signal_connect_swapped (self->mm, "interface-removed",
                            G_CALLBACK (interface_removed_cb), self);

  add_mm_objects (self);
  if (g_hash_table_size (self->origins) == 0)
    {
      g_warning ("No modems available");
      CALLS_EMIT_MESSAGE (self, "No modems available",
                          GTK_MESSAGE_WARNING);
    }
}


static void
constructed (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsMMProvider *self = CALLS_MM_PROVIDER (object);

  mm_manager_new (self->connection,
                  G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                  NULL,
                  (GAsyncReadyCallback) mm_manager_new_cb,
                  self);

  parent_class->constructed (object);
}


static void
dispose (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsMMProvider *self = CALLS_MM_PROVIDER (object);

  g_hash_table_remove_all (self->origins);
  g_clear_object (&self->mm);
  g_clear_object (&self->connection);

  parent_class->dispose (object);
}


static void
finalize (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsMMProvider *self = CALLS_MM_PROVIDER (object);

  g_hash_table_unref (self->origins);

  parent_class->finalize (object);
}


static void
calls_mm_provider_class_init (CallsMMProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  props[PROP_CONNECTION] =
    g_param_spec_object ("connection",
                         _("Connection"),
                         _("The D-Bus connection to use for communication with ModemManager"),
                         G_TYPE_DBUS_CONNECTION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_mm_provider_message_source_interface_init (CallsProviderInterface *iface)
{
}


static void
calls_mm_provider_provider_interface_init (CallsProviderInterface *iface)
{
  iface->get_name = get_name;
  iface->get_origins = get_origins;
}


static void
calls_mm_provider_init (CallsMMProvider *self)
{
  self->origins = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
}


CallsMMProvider *
calls_mm_provider_new (GDBusConnection *connection)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);

  return g_object_new (CALLS_TYPE_MM_PROVIDER,
                       "connection", connection,
                       NULL);
}
