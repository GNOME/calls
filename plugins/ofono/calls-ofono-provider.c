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

#include "calls-ofono-provider.h"
#include "calls-provider.h"
#include "calls-ofono-origin.h"
#include "calls-message-source.h"
#include "util.h"

#include <libgdbofono/gdbo-manager.h>
#include <libgdbofono/gdbo-modem.h>

#include <glib/gi18n.h>
#include <libpeas/peas.h>


struct _CallsOfonoProvider
{
  CallsProvider parent_instance;

  /** D-Bus connection */
  GDBusConnection *connection;
  /** D-Bus proxy for the oFono Manager object */
  GDBOManager *manager;
  /** Map of D-Bus object paths to a struct CallsModemData */
  GHashTable *modems;
  /** Map of D-Bus object paths to Origins */
  GHashTable *origins;
};


static void calls_ofono_provider_message_source_interface_init (CallsMessageSourceInterface *iface);


G_DEFINE_DYNAMIC_TYPE_EXTENDED
(CallsOfonoProvider, calls_ofono_provider, G_TYPE_OBJECT, 0,
 G_IMPLEMENT_INTERFACE_DYNAMIC (CALLS_TYPE_MESSAGE_SOURCE,
                                calls_ofono_provider_message_source_interface_init))


static void
add_origin_to_list (const gchar *path,
                    CallsOfonoOrigin *origin,
                    GList **list)
{
  *list = g_list_prepend (*list, origin);
}


static void
add_origin (CallsOfonoProvider *self,
            const gchar        *path,
            GDBOModem          *modem)
{
  CallsOfonoOrigin *origin;

  g_debug ("Adding oFono Origin with path `%s'", path);

  origin = calls_ofono_origin_new (modem);
  g_hash_table_insert (self->origins, g_strdup(path), origin);

  g_signal_emit_by_name (CALLS_PROVIDER (self),
                         "origin-added", origin);
}


static void
remove_origin (CallsOfonoProvider *self,
               const gchar        *path,
               CallsOfonoOrigin   *origin)
{
  g_debug ("Removing oFono Origin with path `%s'", path);

  g_signal_emit_by_name (CALLS_PROVIDER (self),
                         "origin-removed", origin);

  g_hash_table_remove (self->origins, path);
  g_object_unref (origin);
}


static gboolean
object_array_includes (GVariantIter *iter,
                       const gchar  *needle)
{
  const gchar *str;
  gboolean found = FALSE;
  while (g_variant_iter_loop (iter, "&s", &str))
    {
      if (g_strcmp0 (str, needle) == 0)
        {
          found = TRUE;
          break;
        }
    }
  g_variant_iter_free (iter);

  return found;
}


static void
modem_check_ifaces (CallsOfonoProvider *self,
                    GDBOModem *modem,
                    const gchar *modem_name,
                    GVariant *ifaces)
{
  gboolean voice;
  GVariantIter *iter = NULL;
  const gchar *path;
  CallsOfonoOrigin *origin;

  g_variant_get (ifaces, "as", &iter);

  voice = object_array_includes
    (iter, "org.ofono.VoiceCallManager");

  path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (modem));
  origin = g_hash_table_lookup (self->origins, path);

  if (voice && !origin)
    {
      add_origin (self, path, modem);
    }
  else if (!voice && origin)
    {
      remove_origin (self, path, origin);
    }
}


static void
modem_property_changed_cb (GDBOModem *modem,
                           const gchar *name,
                           GVariant *value,
                           CallsOfonoProvider *self)
{
  gchar *modem_name;

  g_debug ("Modem property `%s' changed", name);

  if (g_strcmp0 (name, "Interfaces") != 0)
    {
      return;
    }

  modem_name = g_object_get_data (G_OBJECT (modem),
                                  "calls-modem-name");

  modem_check_ifaces (self, modem, modem_name, value);
}


struct CallsModemProxyNewData
{
  CallsOfonoProvider *self;
  gchar              *name;
  GVariant           *ifaces;
};


static void
modem_proxy_new_cb (GDBusConnection *connection,
                    GAsyncResult *res,
                    struct CallsModemProxyNewData *data)
{
  GDBOModem *modem;
  GError *error = NULL;
  const gchar *path;

  modem = gdbo_modem_proxy_new_finish (res, &error);
  if (!modem)
    {
      g_variant_unref (data->ifaces);
      g_free (data->name);
      g_free (data);
      g_error ("Error creating oFono Modem proxy: %s",
               error->message);
      return;
    }

  g_signal_connect (modem, "property-changed",
                    G_CALLBACK (modem_property_changed_cb),
                    data->self);


  /* We want to store the oFono modem's Name property so we can pass it
     to our Origin when we create it */
  g_object_set_data_full (G_OBJECT (modem), "calls-modem-name",
                          data->name, g_free);

  path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (modem));

  g_hash_table_insert (data->self->modems, g_strdup(path), modem);


  if (data->ifaces)
    {
      modem_check_ifaces (data->self, modem,
                          data->name, data->ifaces);
      g_variant_unref (data->ifaces);
    }

  g_free (data);

  g_debug ("Modem `%s' added", path);
}


static gchar *
modem_properties_get_name (GVariant *properties)
{
  gchar *name = NULL;
  gboolean ok;

#define try(prop)                                       \
  ok = g_variant_lookup (properties, prop, "s", &name); \
  if (ok) {                                             \
    return name;                                        \
  }

  try ("Name");
  try ("Model");
  try ("Manufacturer");
  try ("Serial");
  try ("SystemPath");

#undef try

  return NULL;
}


static void
modem_added_cb (GDBOManager        *manager,
                const gchar        *path,
                GVariant           *properties,
                CallsOfonoProvider *self)
{
  struct CallsModemProxyNewData *data;

  g_debug ("Adding modem `%s'", path);

  if (g_hash_table_lookup (self->modems, path))
    {
      g_warning ("Modem `%s' already exists", path);
      return;
    }

  data = g_new0 (struct CallsModemProxyNewData, 1);
  data->self = self;
  data->name = modem_properties_get_name (properties);

  data->ifaces = g_variant_lookup_value
    (properties, "Interfaces", G_VARIANT_TYPE_ARRAY);
  if (data->ifaces)
    {
      g_variant_ref (data->ifaces);
    }

  gdbo_modem_proxy_new
    (self->connection,
     G_DBUS_PROXY_FLAGS_NONE,
     g_dbus_proxy_get_name (G_DBUS_PROXY (manager)),
     path,
     NULL,
     (GAsyncReadyCallback) modem_proxy_new_cb,
     data);

  g_debug ("Modem `%s' addition in progress", path);
}


static void
modem_removed_cb (GDBOManager        *manager,
                  const gchar        *path,
                  CallsOfonoProvider *self)
{
  CallsOfonoOrigin *origin;

  g_debug ("Removing modem `%s'", path);

  origin = g_hash_table_lookup (self->origins, path);
  if (origin)
    {
      remove_origin (self, path, origin);
    }

  g_hash_table_remove (self->modems, path);

  g_debug ("Modem `%s' removed", path);
}


static void
get_modems_cb (GDBOManager *manager,
               GAsyncResult *res,
               CallsOfonoProvider *self)
{
  gboolean ok;
  GVariant *modems;
  GVariantIter *modems_iter = NULL;
  GError *error = NULL;
  const gchar *path;
  GVariant *properties;

  ok = gdbo_manager_call_get_modems_finish (manager, &modems,
                                            res, &error);
  if (!ok)
    {
      g_warning ("Error getting modems from oFono Manager: %s",
                 error->message);
      CALLS_ERROR (self, error);
      return;
    }

  {
    char *text = g_variant_print (modems, TRUE);
    g_debug ("Received modems from oFono Manager: %s", text);
    g_free (text);
  }

  g_variant_get (modems, "a(oa{sv})", &modems_iter);
  while (g_variant_iter_loop (modems_iter, "(&o@a{sv})",
                              &path, &properties))
    {
      g_debug ("Got modem object path `%s'", path);
      modem_added_cb (manager, path, properties, self);
    }
  g_variant_iter_free (modems_iter);

  g_variant_unref (modems);
}

static const char *
calls_ofono_provider_get_name (CallsProvider *provider)
{
  return "Ofono";
}

static const char *
calls_ofono_provider_get_status (CallsProvider *provider)
{
  return "";
}

static GList *
calls_ofono_provider_get_origins (CallsProvider *provider)
{
  CallsOfonoProvider *self = CALLS_OFONO_PROVIDER (provider);
  GList *list = NULL;

  g_hash_table_foreach (self->origins,
                        (GHFunc)add_origin_to_list, &list);

  return g_list_reverse (list);
}


static void
constructed (GObject *object)
{
  CallsOfonoProvider *self = CALLS_OFONO_PROVIDER (object);
  GError *error = NULL;

  self->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!self->connection)
    {
      g_error ("Error creating D-Bus connection: %s",
               error->message);
    }

  self->manager = gdbo_manager_proxy_new_sync
    (self->connection,
     G_DBUS_PROXY_FLAGS_NONE,
     "org.ofono",
     "/",
     NULL,
     &error);
  if (!self->manager)
    {
      g_error ("Error creating ModemManager object manager proxy: %s",
               error->message);
    }

  g_signal_connect (self->manager, "modem-added",
                    G_CALLBACK (modem_added_cb), self);
  g_signal_connect (self->manager, "modem-removed",
                    G_CALLBACK (modem_removed_cb), self);

  gdbo_manager_call_get_modems
    (self->manager,
     NULL,
     (GAsyncReadyCallback) get_modems_cb,
     self);
     
  G_OBJECT_CLASS (calls_ofono_provider_parent_class)->constructed (object);
}


static void
dispose (GObject *object)
{
  CallsOfonoProvider *self = CALLS_OFONO_PROVIDER (object);

  g_clear_object (&self->manager);
  g_clear_object (&self->connection);

  G_OBJECT_CLASS (calls_ofono_provider_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsOfonoProvider *self = CALLS_OFONO_PROVIDER (object);

  g_hash_table_unref (self->origins);
  g_hash_table_unref (self->modems);

  G_OBJECT_CLASS (calls_ofono_provider_parent_class)->finalize (object);
}


static void
calls_ofono_provider_class_init (CallsOfonoProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsProviderClass *provider_class = CALLS_PROVIDER_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  provider_class->get_name = calls_ofono_provider_get_name;
  provider_class->get_status = calls_ofono_provider_get_status;
  provider_class->get_origins = calls_ofono_provider_get_origins;
}


static void
calls_ofono_provider_class_finalize (CallsOfonoProviderClass *klass)
{
}


static void
calls_ofono_provider_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}


static void
calls_ofono_provider_init (CallsOfonoProvider *self)
{
  self->modems = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        g_free, g_object_unref);
  self->origins = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, NULL);
}


G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
  calls_ofono_provider_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              CALLS_TYPE_PROVIDER,
                                              CALLS_TYPE_OFONO_PROVIDER);
}
