/*
 * Copyright (C) 2023 Purism SPC
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
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "CallsPluginManager"

#include "calls-config.h"

#include "calls-provider.h"
#include "calls-plugin.h"
#include "calls-plugin-manager.h"
#include "calls-util.h"

#include <libpeas.h>

/**
 * SECTION:plugin-manager
 * @short_description: Manages available plugins
 * @Title: CallsPluginManager
 *
 * #CallsPluginManager is a singleton that manages available plugins.
 */

struct _CallsPluginManager {
  GObject     parent_instance;

  PeasEngine *plugin_engine;

  GListStore *plugins;
  GListStore *providers;
};

G_DEFINE_TYPE (CallsPluginManager, calls_plugin_manager, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PROVIDERS,
  PROP_PLUGINS,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static CallsPlugin *
find_plugin_by_name (CallsPluginManager *self,
                     const char         *module_name,
                     guint              *position)
{
  guint n_plugins;
  GListModel *plugins;

  g_assert (CALLS_IS_PLUGIN_MANAGER (self));

  plugins = G_LIST_MODEL (self->plugins);
  n_plugins = g_list_model_get_n_items (plugins);

  for (guint i = 0; i < n_plugins; i++) {
    g_autoptr (CallsPlugin) plugin = g_list_model_get_item (plugins, i);

    if (g_strcmp0 (calls_plugin_get_module_name (plugin), module_name) == 0) {
      if (position)
        *position = i;

      return plugin;
    }
  }

  return NULL;
}


static void
on_plugin_loaded (CallsPlugin        *plugin,
                  GParamSpec         *pspec,
                  CallsPluginManager *self)
{
  CallsProvider *provider = calls_plugin_get_provider (plugin);
  gboolean loaded = calls_plugin_is_loaded (plugin);

  if (!provider) {
    g_debug ("Plugin '%s' was %sloaded, but no provider was found to %s",
             calls_plugin_get_module_name (plugin),
             loaded ? "" : "un",
             loaded ? "add" : "remove");
    return;
  }

  if (calls_plugin_is_loaded (plugin)) {
    g_list_store_append (self->providers, provider);
  } else {
    guint pos;

    g_list_store_find (self->providers, provider, &pos);
    g_list_store_remove (self->providers, pos);
  }
}


static void
calls_plugin_manager_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  CallsPluginManager *self = CALLS_PLUGIN_MANAGER (object);

  switch (property_id) {
  case PROP_PROVIDERS:
    g_value_set_object (value, calls_plugin_manager_get_providers (self));
    break;

  case PROP_PLUGINS:
    g_value_set_object (value, calls_plugin_manager_get_plugins (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
unload_and_free_plugins (GListStore *plugins)
{
  GListModel *model = G_LIST_MODEL (plugins);
  guint n = g_list_model_get_n_items (model);
  for (guint i = 0; i < n; i++) {
    g_autoptr (CallsPlugin) plugin = g_list_model_get_item (model, i);
    g_autoptr (GError) error = NULL;

    if (!calls_plugin_unload (plugin, &error))
      g_warning ("Could not unload plugin '%s': %s",
                 calls_plugin_get_module_name (plugin),
                 error->message);
  }

  g_object_unref (plugins);
}

static void
calls_plugin_manager_dispose (GObject *object)
{
  CallsPluginManager *self = CALLS_PLUGIN_MANAGER (object);

  guint n = g_list_model_get_n_items (G_LIST_MODEL (self->plugins));
  for (guint i = 0; i < n; i++) {
    // The manager will be disposed immediately; don't let unloaded plugins callback to it
    g_autoptr (CallsPlugin) plugin = g_list_model_get_item (G_LIST_MODEL (self->plugins), i);
    g_signal_handlers_disconnect_by_func (G_OBJECT (plugin), G_CALLBACK (on_plugin_loaded), self);
  }

  g_clear_pointer (&self->plugins, unload_and_free_plugins);
  g_clear_object (&self->plugin_engine);

  G_OBJECT_CLASS (calls_plugin_manager_parent_class)->dispose (object);
}


static void
calls_plugin_manager_class_init (CallsPluginManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = calls_plugin_manager_dispose;
  object_class->get_property = calls_plugin_manager_get_property;

  /**
   * CallsPluginManager:providers:
   *
   * A #GListModel of #CallsProvider that are currently loaded.
   */
  props[PROP_PROVIDERS] =
    g_param_spec_object ("providers",
                         "",
                         "",
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * CallsPluginManager:plugins:
   *
   * A #GListModel of #CallsProvider that are currently loaded.
   */
  props[PROP_PLUGINS] =
    g_param_spec_object ("plugins",
                         "",
                         "",
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_plugin_manager_init (CallsPluginManager *self)
{
  g_autofree char *default_plugin_dir_provider = NULL;
  const char *dir;
  uint n_plugins;

  self->plugin_engine = peas_engine_new ();

  dir = g_getenv ("CALLS_PLUGIN_DIR");
  if (!STR_IS_NULL_OR_EMPTY (dir)) {
    g_autofree char *plugin_dir_provider = NULL;

    plugin_dir_provider = g_build_filename (dir, "provider", NULL);

    if (g_file_test (plugin_dir_provider, G_FILE_TEST_EXISTS)) {
      g_debug ("Adding '%s' to plugin search path", plugin_dir_provider);
      peas_engine_add_search_path (self->plugin_engine, plugin_dir_provider, NULL);
    } else {
      g_warning ("Not adding '%s' to plugin search path, because the directory doesn't exist.\n"
                 "Check if env CALLS_PLUGIN_DIR is set correctly", plugin_dir_provider);
    }
  }

  default_plugin_dir_provider = g_build_filename (PLUGIN_LIBDIR, "provider", NULL);
  peas_engine_add_search_path (self->plugin_engine, default_plugin_dir_provider, NULL);
  g_debug ("Scanning for plugins in '%s'", default_plugin_dir_provider);

  peas_engine_rescan_plugins (self->plugin_engine);

  self->plugins = g_list_store_new (CALLS_TYPE_PLUGIN);

  self->providers = g_list_store_new (CALLS_TYPE_PROVIDER);

  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (self->plugin_engine));

  for (uint i = 0; i < n_plugins; i++) {
    g_autoptr (PeasPluginInfo) info =
      g_list_model_get_item (G_LIST_MODEL (self->plugin_engine), i);
    CallsPlugin *plugin = calls_plugin_new (info);

    g_debug ("Created plugin '%s', found in '%s'",
             peas_plugin_info_get_module_name (info),
      peas_plugin_info_get_module_dir (info));

    g_signal_connect_object (plugin,
                             "notify::loaded",
                             G_CALLBACK (on_plugin_loaded),
                             self,
                             G_CONNECT_AFTER);

    g_list_store_append (self->plugins, plugin);
  }

  g_debug ("Created %d plugins", g_list_model_get_n_items (G_LIST_MODEL (self->plugins)));
}


CallsPluginManager *
calls_plugin_manager_get_default (void)
{
  static CallsPluginManager *instance;

  if (instance == NULL) {
    instance = g_object_new (CALLS_TYPE_PLUGIN_MANAGER, NULL);
    g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *) &instance);
  }
  return instance;
}


gboolean
calls_plugin_manager_load_plugin (CallsPluginManager *self,
                                  const char         *name,
                                  GError            **error)
{
  CallsPlugin *plugin;

  g_return_val_if_fail (CALLS_IS_PLUGIN_MANAGER (self), FALSE);
  g_return_val_if_fail (!STR_IS_NULL_OR_EMPTY (name), FALSE);

  plugin = find_plugin_by_name (self, name, NULL);

  if (!plugin) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                 "No plugin '%s' found", name);
    return FALSE;
  }

  return calls_plugin_load (plugin, error);
}


gboolean
calls_plugin_manager_unload_plugin (CallsPluginManager *self,
                                    const char         *name,
                                    GError            **error)
{
  CallsPlugin *plugin;

  g_return_val_if_fail (CALLS_IS_PLUGIN_MANAGER (self), FALSE);
  g_return_val_if_fail (!STR_IS_NULL_OR_EMPTY (name), FALSE);

  plugin = find_plugin_by_name (self, name, NULL);

  if (!plugin) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                 "No plugin '%s' found", name);
    return FALSE;
  }

  return calls_plugin_unload (plugin, error);
}

/**
 * calls_plugin_manager_get_plugins:
 * @self: The #CallsPluginManager
 *
 * Returns: (transfer none): A #GListModel of #[CallsPlugin]s
 */
GListModel *
calls_plugin_manager_get_plugins (CallsPluginManager *self)
{
  g_return_val_if_fail (CALLS_IS_PLUGIN_MANAGER (self), NULL);

  return G_LIST_MODEL (self->plugins);
}
/**
 * calls_plugin_manager_get_providers:
 * @self: The #CallsPluginManager
 *
 * Returns: (transfer none): A #GListModel of #[CallsProvider]s
 */
GListModel *
calls_plugin_manager_get_providers (CallsPluginManager *self)
{
  g_return_val_if_fail (CALLS_IS_PLUGIN_MANAGER (self), NULL);

  return G_LIST_MODEL (self->providers);
}


/**
 * calls_plugin_manager_has_any_plugins:
 * @self: The #CallsPluginManager
 *
 * Returns: %TRUE if any plugin is loaded, %FALSE otherwise.
 */
gboolean
calls_plugin_manager_has_any_plugins (CallsPluginManager *self)
{
  guint n_plugins;

  g_return_val_if_fail (CALLS_IS_PLUGIN_MANAGER (self), FALSE);

  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (self->plugins));

  for (guint i = 0; i < n_plugins; i++) {
    g_autoptr (CallsPlugin) plugin =
      g_list_model_get_item (G_LIST_MODEL (self->plugins), i);

    if (calls_plugin_is_loaded (plugin))
      return TRUE;
  }
  return FALSE;
}

/**
 * calls_plugin_manager_has_plugins:
 * @self: The #CallsPluginManager
 * @name: The name of the plugin to check
 *
 * Returns: %TRUE if a plugin with @name is loaded, %FALSE otherwise.
 */
gboolean
calls_plugin_manager_has_plugin (CallsPluginManager *self,
                                 const char         *name)
{
  CallsPlugin *plugin;

  g_return_val_if_fail (CALLS_IS_PLUGIN_MANAGER (self), FALSE);
  g_return_val_if_fail (!STR_IS_NULL_OR_EMPTY (name), FALSE);

  plugin = find_plugin_by_name (self, name, NULL);
  if (!plugin)
    return FALSE;

  return calls_plugin_is_loaded (plugin);
}


/**
 * calls_plugin_manager_get_plugin_names:
 * @self: The #CallsPluginManager
 * @length: (optional) (out): the length of the returned array
 *
 * Retrieves the names of all plugins loaded by @self, as an array.
 *
 * You should free the return value with g_free().
 *
 * Returns: (array length=length) (transfer container): a
 * %NULL-terminated array containing the plugin names.
 */
const char **
calls_plugin_manager_get_plugin_names (CallsPluginManager *self,
                                       guint              *length)
{
  GPtrArray *array;
  guint n_items;

  g_return_val_if_fail (CALLS_IS_PLUGIN_MANAGER (self), NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->plugins));
  if (length)
    *length = n_items;
  array = g_ptr_array_sized_new (n_items + 1);
  array->pdata[n_items] = NULL;

  for (guint i = 0; i < n_items; i++) {
    g_autoptr (CallsPlugin) plugin =
      g_list_model_get_item (G_LIST_MODEL (self->plugins), i);

    array->pdata[i] = (gpointer) calls_plugin_get_module_name (plugin);
  }

  return (const char **) g_ptr_array_free (array, FALSE);
}

gboolean
calls_plugin_manager_unload_all_plugins (CallsPluginManager *self, GError **error)
{
  GListModel *plugins;
  uint n_plugins;
  gboolean ok = TRUE;

  g_return_val_if_fail (CALLS_IS_PLUGIN_MANAGER (self), FALSE);

  plugins = G_LIST_MODEL (self->plugins);
  n_plugins = g_list_model_get_n_items (plugins);

  for (uint i = 0; i < n_plugins; i++) {
    g_autoptr (CallsPlugin) plugin = g_list_model_get_item (plugins, i);

    if (calls_plugin_is_loaded (plugin))
      ok = ok && calls_plugin_unload (plugin, error);
  }

  return ok;
}
