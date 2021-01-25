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
#include <libpeas/peas.h>
#include <glib/gi18n.h>

struct _CallsMMProvider
{
  CallsProvider parent_instance;

  /* The status property */
  gchar *status;
  /** ID for the D-Bus watch */
  guint watch_id;
  /** ModemManager object proxy */
  MMManager *mm;
  /** Map of D-Bus object paths to origins */
  GHashTable *origins;
};

static void calls_mm_provider_message_source_interface_init (CallsMessageSourceInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED
(CallsMMProvider, calls_mm_provider, CALLS_TYPE_PROVIDER, 0,
 G_IMPLEMENT_INTERFACE_DYNAMIC (CALLS_TYPE_MESSAGE_SOURCE,
                                calls_mm_provider_message_source_interface_init))


static void
set_status (CallsMMProvider *self,
            const gchar     *new_status)
{
  if (strcmp (self->status, new_status) == 0)
    {
      return;
    }

  g_free (self->status);
  self->status = g_strdup (new_status);
  g_object_notify (G_OBJECT (self), "status");
}


static void
update_status (CallsMMProvider *self)
{
  const gchar *s;

  if (!self->mm)
    {
      s = _("ModemManager unavailable");
    }
  else if (g_hash_table_size (self->origins) == 0)
    {
      s = _("No voice-capable modem available");
    }
  else
    {
      s = _("Normal");
    }

  set_status (self, s);
}


static void
add_origin (CallsMMProvider *self,
            GDBusObject     *object)
{
  MMObject *mm_obj;
  CallsMMOrigin *origin;
  const gchar *path;

  path = g_dbus_object_get_object_path (object);
  if (g_hash_table_contains (self->origins, path))
    {
      g_warning ("New voice interface on existing"
                 " origin with path `%s'", path);
      return;
    }

  g_debug ("Adding new voice-capable modem `%s'",
           path);

  g_assert (MM_IS_OBJECT (object));
  mm_obj = MM_OBJECT (object);

  origin = calls_mm_origin_new (mm_obj);

  g_hash_table_insert (self->origins,
                       mm_object_dup_path (mm_obj),
                       origin);

  g_signal_emit_by_name (CALLS_PROVIDER (self),
                         "origin-added", origin);
  update_status (self);
}


static void
interface_added_cb (CallsMMProvider *self,
                    GDBusObject     *object,
                    GDBusInterface  *interface)
{
  GDBusInterfaceInfo *info;

  info = g_dbus_interface_get_info (interface);

  g_debug ("ModemManager interface `%s' found on object `%s'",
           info->name,
           g_dbus_object_get_object_path (object));

  if (g_strcmp0 (info->name,
                 "org.freedesktop.ModemManager1.Modem.Voice") == 0)
    {
      add_origin (self, object);
    }
}


static void
remove_modem_object (CallsMMProvider *self,
                     const gchar     *path,
                     GDBusObject     *object)
{
  gpointer *origin;

  origin = g_hash_table_lookup (self->origins, path);
  if (!origin)
    {
      return;
    }

  g_assert (CALLS_IS_ORIGIN (origin));

  g_object_ref (origin);
  g_hash_table_remove (self->origins, path);

  g_signal_emit_by_name (CALLS_PROVIDER (self),
                         "origin-removed", CALLS_ORIGIN (origin));
  g_object_unref (origin);

  update_status (self);
}


static void
interface_removed_cb (CallsMMProvider *self,
                      GDBusObject     *object,
                      GDBusInterface  *interface)
{
  const gchar *path;
  GDBusInterfaceInfo *info;

  path = g_dbus_object_get_object_path (object);
  info = g_dbus_interface_get_info (interface);

  g_debug ("ModemManager interface `%s' removed on object `%s'",
           info->name, path);

  if (g_strcmp0 (info->name,
                 "org.freedesktop.ModemManager1.Modem.Voice") == 0)
    {
      remove_modem_object (self, path, object);
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


void
object_added_cb (CallsMMProvider *self,
                 GDBusObject     *object)
{
  g_debug ("ModemManager object `%s' added",
           g_dbus_object_get_object_path (object));

  add_mm_object (self, object);
}


void
object_removed_cb (CallsMMProvider *self,
                   GDBusObject     *object)
{
  const gchar *path;

  path = g_dbus_object_get_object_path (object);
  g_debug ("ModemManager object `%s' removed", path);

  remove_modem_object (self, path, object);
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


  g_signal_connect_swapped (G_DBUS_OBJECT_MANAGER (self->mm),
                            "interface-added",
                            G_CALLBACK (interface_added_cb), self);
  g_signal_connect_swapped (G_DBUS_OBJECT_MANAGER (self->mm),
                            "interface-removed",
                            G_CALLBACK (interface_removed_cb), self);
  g_signal_connect_swapped (G_DBUS_OBJECT_MANAGER (self->mm),
                            "object-added",
                            G_CALLBACK (object_added_cb), self);
  g_signal_connect_swapped (G_DBUS_OBJECT_MANAGER (self->mm),
                            "object-removed",
                            G_CALLBACK (object_removed_cb), self);

  update_status (self);
  add_mm_objects (self);
}


static void
mm_appeared_cb (GDBusConnection *connection,
                const gchar *name,
                const gchar *name_owner,
                CallsMMProvider *self)
{
  g_debug ("ModemManager appeared on D-Bus");

  mm_manager_new (connection,
                  G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                  NULL,
                  (GAsyncReadyCallback) mm_manager_new_cb,
                  self);
}


static void
clear_dbus (CallsMMProvider *self)
{
  GList *paths, *node;
  gpointer origin;

  paths = g_hash_table_get_keys (self->origins);

  for (node = paths; node != NULL; node = node->next)
    {
      g_hash_table_steal_extended (self->origins, node->data, NULL, &origin);
      g_signal_emit_by_name (CALLS_PROVIDER (self),
                             "origin-removed", CALLS_ORIGIN (origin));
      g_object_unref (origin);
    }

  g_list_free_full (paths, g_free);

  g_clear_object (&self->mm);
}


void
mm_vanished_cb (GDBusConnection *connection,
                const gchar *name,
                CallsMMProvider *self)
{
  g_debug ("ModemManager vanished from D-Bus");
  clear_dbus (self);
  update_status (self);
}


static const char *
calls_mm_provider_get_name (CallsProvider *provider)
{
  return "ModemManager";
}

static const char *
calls_mm_provider_get_status (CallsProvider *provider)
{
  CallsMMProvider *self = CALLS_MM_PROVIDER (provider);

  return self->status;
}

static GList *
calls_mm_provider_get_origins (CallsProvider *provider)
{
  CallsMMProvider *self = CALLS_MM_PROVIDER (provider);

  return g_hash_table_get_values (self->origins);
}

static void
constructed (GObject *object)
{
  CallsMMProvider *self = CALLS_MM_PROVIDER (object);

  self->watch_id =
    g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                      MM_DBUS_SERVICE,
                      G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                      (GBusNameAppearedCallback)mm_appeared_cb,
                      (GBusNameVanishedCallback)mm_vanished_cb,
                      self, NULL);

  g_debug ("Watching for ModemManager");

  G_OBJECT_CLASS (calls_mm_provider_parent_class)->constructed (object);
}


static void
dispose (GObject *object)
{
  CallsMMProvider *self = CALLS_MM_PROVIDER (object);

  if (self->watch_id)
    {
      g_bus_unwatch_name (self->watch_id);
      self->watch_id = 0;
    }

  clear_dbus (self);

  G_OBJECT_CLASS (calls_mm_provider_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsMMProvider *self = CALLS_MM_PROVIDER (object);

  g_hash_table_unref (self->origins);
  g_free (self->status);

  G_OBJECT_CLASS (calls_mm_provider_parent_class)->finalize (object);
}


static void
calls_mm_provider_class_init (CallsMMProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsProviderClass *provider_class = CALLS_PROVIDER_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  provider_class->get_name = calls_mm_provider_get_name;
  provider_class->get_status = calls_mm_provider_get_status;
  provider_class->get_origins = calls_mm_provider_get_origins;
}


static void
calls_mm_provider_class_finalize (CallsMMProviderClass *klass)
{
}

static void
calls_mm_provider_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}


static void
calls_mm_provider_init (CallsMMProvider *self)
{
  self->status = g_strdup (_("Initialised"));
  self->origins = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
}


G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
  calls_mm_provider_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              CALLS_TYPE_PROVIDER,
                                              CALLS_TYPE_MM_PROVIDER);
}
