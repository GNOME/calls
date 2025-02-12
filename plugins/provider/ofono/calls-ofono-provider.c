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

#define G_LOG_DOMAIN "CallsOfonoProvider"

#include "calls-ofono-provider.h"
#include "calls-provider.h"
#include "calls-ofono-origin.h"
#include "calls-message-source.h"
#include "calls-util.h"

#include <libgdbofono/gdbo-manager.h>
#include <libgdbofono/gdbo-modem.h>

#include <glib/gi18n.h>
#include <libpeas.h>

static const char * const supported_protocols[] = {
  "tel",
  NULL
};

struct _CallsOfonoProvider {
  CallsProvider    parent_instance;

  /* The status property */
  gchar           *status;
  /** ID for the D-Bus watch */
  guint            watch_id;
  /** D-Bus connection */
  GDBusConnection *connection;
  /** D-Bus proxy for the oFono Manager object */
  GDBOManager     *manager;
  /** Map of D-Bus object paths to a struct CallsModemData */
  GHashTable      *modems;
  /* A list of CallsOrigins */
  GListStore      *origins;
};


static void calls_ofono_provider_message_source_interface_init (CallsMessageSourceInterface *iface);


G_DEFINE_DYNAMIC_TYPE_EXTENDED
  (CallsOfonoProvider, calls_ofono_provider, CALLS_TYPE_PROVIDER, 0,
  G_IMPLEMENT_INTERFACE_DYNAMIC (CALLS_TYPE_MESSAGE_SOURCE,
                                 calls_ofono_provider_message_source_interface_init))


static void
set_status (CallsOfonoProvider *self,
            const gchar        *new_status)
{
  if (strcmp (self->status, new_status) == 0) {
    return;
  }

  g_free (self->status);
  self->status = g_strdup (new_status);
  g_object_notify (G_OBJECT (self), "status");
}


static void
update_status (CallsOfonoProvider *self)
{
  const gchar *s;
  GListModel *model;

  model = G_LIST_MODEL (self->origins);

  if (!self->connection) {
    s = _("DBus unavailable");
  } else if (g_list_model_get_n_items (model) == 0) {
    s = _("No voice-capable modem available");
  } else {
    s = _("Normal");
  }

  set_status (self, s);
}


static gboolean
ofono_find_origin_index (CallsOfonoProvider *self,
                         const char         *path,
                         guint              *index)
{
  GListModel *model;
  guint n_items;

  g_assert (CALLS_IS_OFONO_PROVIDER (self));

  model = G_LIST_MODEL (self->origins);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr (CallsOfonoOrigin) origin = NULL;

    origin = g_list_model_get_item (model, i);

    if (calls_ofono_origin_matches (origin, path)) {
      if (index)
        *index = i;

      update_status (self);

      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
object_array_includes (GVariantIter *iter,
                       const gchar  *needle)
{
  const gchar *str;
  gboolean found = FALSE;

  while (g_variant_iter_loop (iter, "&s", &str))
  {
    if (g_strcmp0 (str, needle) == 0) {
      found = TRUE;
      break;
    }
  }
  g_variant_iter_free (iter);

  return found;
}


static void
modem_check_ifaces (CallsOfonoProvider *self,
                    GDBOModem          *modem,
                    const gchar        *modem_name,
                    GVariant           *ifaces)
{
  gboolean voice;
  GVariantIter *iter = NULL;
  const gchar *path;
  guint index;
  gboolean has_origin;

  g_variant_get (ifaces, "as", &iter);

  voice = object_array_includes (iter, "org.ofono.VoiceCallManager");

  path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (modem));

  has_origin = ofono_find_origin_index (self, path, &index);
  if (voice && !has_origin) {
    g_autoptr (CallsOfonoOrigin) origin = NULL;

    g_debug ("Adding oFono Origin with path `%s'", path);

    origin = calls_ofono_origin_new (modem);
    g_list_store_append (self->origins, origin);
  } else if (!voice && has_origin) {
    g_list_store_remove (self->origins, index);
  }
}


static void
modem_property_changed_cb (GDBOModem          *modem,
                           const gchar        *name,
                           GVariant           *value,
                           CallsOfonoProvider *self)
{
  gchar *modem_name;

  g_debug ("Modem property `%s' changed", name);

  if (g_strcmp0 (name, "Interfaces") != 0)
    return;

  modem_name = g_object_get_data (G_OBJECT (modem),
                                  "calls-modem-name");

  /* PropertyChanged gives us a variant gvariant containing a string array,
     but modem_check_ifaces expects the inner string array gvariant */
  value = g_variant_get_variant (value);
  modem_check_ifaces (self, modem, modem_name, value);
}


struct CallsModemProxyNewData {
  CallsOfonoProvider *self;
  gchar              *name;
  GVariant           *ifaces;
};


static void
modem_proxy_new_cb (GDBusConnection               *connection,
                    GAsyncResult                  *res,
                    struct CallsModemProxyNewData *data)
{
  GDBOModem *modem;
  GError *error = NULL;
  const gchar *path;

  modem = gdbo_modem_proxy_new_finish (res, &error);
  if (!modem) {
    g_variant_unref (data->ifaces);
    g_free (data->name);
    g_free (data);
    g_warning ("Error creating oFono Modem proxy: %s",
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

  g_hash_table_insert (data->self->modems, g_strdup (path), modem);


  if (data->ifaces) {
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

static const char *const *
calls_ofono_provider_get_protocols (CallsProvider *provider)
{
  return supported_protocols;
}

static gboolean
calls_ofono_provider_is_modem (CallsProvider *provider)
{
  return TRUE;
}

static void
modem_added_cb (GDBOManager        *manager,
                const gchar        *path,
                GVariant           *properties,
                CallsOfonoProvider *self)
{
  struct CallsModemProxyNewData *data;

  g_debug ("Adding modem `%s'", path);

  if (g_hash_table_lookup (self->modems, path)) {
    g_warning ("Modem `%s' already exists", path);
    return;
  }

  data = g_new0 (struct CallsModemProxyNewData, 1);
  data->self = self;
  data->name = modem_properties_get_name (properties);

  data->ifaces = g_variant_lookup_value
                   (properties, "Interfaces", G_VARIANT_TYPE_ARRAY);
  if (data->ifaces) {
    g_variant_ref (data->ifaces);
  }

  gdbo_modem_proxy_new (self->connection,
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
  guint index;

  g_debug ("Removing modem `%s'", path);

  if (ofono_find_origin_index (self, path, &index))
    g_list_store_remove (self->origins, index);

  g_hash_table_remove (self->modems, path);

  g_debug ("Modem `%s' removed", path);
}


static void
get_modems_cb (GDBOManager        *manager,
               GAsyncResult       *res,
               CallsOfonoProvider *self)
{
  g_autoptr (GError) error = NULL;

  gboolean ok;
  GVariant *modems;
  GVariantIter *modems_iter = NULL;
  const gchar *path;
  GVariant *properties;

  ok = gdbo_manager_call_get_modems_finish (manager, &modems,
                                            res, &error);
  if (!ok) {
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
  CallsOfonoProvider *self = CALLS_OFONO_PROVIDER (provider);

  return self->status;
}

static GListModel *
calls_ofono_provider_get_origins (CallsProvider *provider)
{
  CallsOfonoProvider *self = CALLS_OFONO_PROVIDER (provider);

  return G_LIST_MODEL (self->origins);
}

static void
ofono_appeared_cb (GDBusConnection    *connection,
                   const gchar        *name,
                   const gchar        *name_owner,
                   CallsOfonoProvider *self)
{
  g_autoptr (GError) error = NULL;
  self->connection = connection;
  if (!self->connection) {
    g_warning ("Error creating D-Bus connection: %s",
               error->message);
    return;
  }

  /* TODO this should be async */
  self->manager = gdbo_manager_proxy_new_sync
                    (self->connection,
                    G_DBUS_PROXY_FLAGS_NONE,
                    "org.ofono",
                    "/",
                    NULL,
                    &error);
  if (!self->manager) {
    g_warning ("Error creating ModemManager object manager proxy: %s",
               error->message);
    return;
  }

  g_signal_connect (self->manager, "modem-added",
                    G_CALLBACK (modem_added_cb), self);
  g_signal_connect (self->manager, "modem-removed",
                    G_CALLBACK (modem_removed_cb), self);

  gdbo_manager_call_get_modems (self->manager,
                                NULL,
                                (GAsyncReadyCallback) get_modems_cb,
                                self);
}


static void
ofono_vanished_cb (GDBusConnection    *connection,
                   const gchar        *name,
                   CallsOfonoProvider *self)
{
  g_debug ("Ofono vanished from D-Bus");
  g_list_store_remove_all (self->origins);
  update_status (self);
}

static void
constructed (GObject *object)
{
  CallsOfonoProvider *self = CALLS_OFONO_PROVIDER (object);

  self->watch_id =
    g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                      "org.ofono",
                      G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                      (GBusNameAppearedCallback) ofono_appeared_cb,
                      (GBusNameVanishedCallback) ofono_vanished_cb,
                      self, NULL);

  g_debug ("Watching for Ofono");


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

  g_object_unref (self->origins);
  g_free (self->status);
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
  provider_class->get_protocols = calls_ofono_provider_get_protocols;
  provider_class->is_modem = calls_ofono_provider_is_modem;
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
  self->status = g_strdup (_("Initialized"));
  self->modems = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        g_free, g_object_unref);
  self->origins = g_list_store_new (CALLS_TYPE_ORIGIN);
}


G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
  calls_ofono_provider_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              CALLS_TYPE_PROVIDER,
                                              CALLS_TYPE_OFONO_PROVIDER);
}
