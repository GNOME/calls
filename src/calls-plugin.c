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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Calls.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "calls-plugin.h"

/**
 * SECTION:calls-plugin
 * @short_description: A plugin for calls
 * @Title: CallsPlugin
 *
 * Holds information about plugins and allows loading/unloading.
 */

struct _CallsPlugin {
  GObject         parent_instance;

  PeasPluginInfo *info;
  CallsProvider  *provider;

  gboolean        is_loaded;
};

G_DEFINE_TYPE (CallsPlugin, calls_plugin, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  PROP_NAME,
  PROP_DESCRIPTION,
  PROP_AUTHORS,
  PROP_COPYRIGHT,
  PROP_VERSION,
  PROP_LOADED,
  PROP_PROVIDER,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];


static void
calls_plugin_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  CallsPlugin *self = CALLS_PLUGIN (object);

  switch (prop_id) {
  case PROP_PLUGIN_INFO:
    self->info = g_value_get_object (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}


static void
calls_plugin_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  CallsPlugin *self = CALLS_PLUGIN (object);

  switch (prop_id) {
  case PROP_NAME:
    g_value_set_string (value, calls_plugin_get_name (self));
    break;

  case PROP_DESCRIPTION:
    g_value_set_string (value, calls_plugin_get_description (self));
    break;

  case PROP_AUTHORS:
    g_value_set_boxed (value, calls_plugin_get_authors (self));
    break;

  case PROP_COPYRIGHT:
    g_value_set_string (value, calls_plugin_get_copyright (self));
    break;

  case PROP_VERSION:
    g_value_set_string (value, calls_plugin_get_version (self));
    break;

  case PROP_LOADED:
    g_value_set_boolean (value, calls_plugin_is_loaded (self));
    break;

  case PROP_PROVIDER:
    g_value_set_object (value, calls_plugin_get_provider (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}


static void
calls_plugin_dispose (GObject *object)
{
  CallsPlugin *self = CALLS_PLUGIN (object);

  g_clear_object (&self->info);
  g_clear_object (&self->provider);

  G_OBJECT_CLASS (calls_plugin_parent_class)->dispose (object);
}


static void
calls_plugin_class_init (CallsPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = calls_plugin_dispose;
  object_class->get_property = calls_plugin_get_property;
  object_class->set_property = calls_plugin_set_property;

  /**
   * CallsPlugin:plugin-info:
   *
   * The #PeasPluginInfo containing information about the plugin
   */
  props[PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info",
                        "",
                        "",
                        PEAS_TYPE_PLUGIN_INFO,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);
  /**
   * CallsPlugin:name:
   *
   * The name of the plugin
   */
  props[PROP_NAME] =
    g_param_spec_string ("name",
                         "",
                         "",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * CallsPlugin:description:
   *
   * A description of the plugin
   */
  props[PROP_DESCRIPTION] =
    g_param_spec_string ("description",
                         "",
                         "",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * CallsPlugin:authors:
   *
   * The name of the plugin
   */
  props[PROP_AUTHORS] =
    g_param_spec_boxed ("authors",
                        "",
                        "",
                        G_TYPE_STRV,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * CallsPlugin:description:
   *
   * The copyright holder of the plugin
   */
  props[PROP_COPYRIGHT] =
    g_param_spec_string ("copyright",
                         "",
                         "",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * CallsPlugin:version:
   *
   * The version of the plugin
   */
  props[PROP_VERSION] =
    g_param_spec_string ("version",
                         "",
                         "",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * CallsPlugin:loaded:
   *
   * Whether the plugin is loaded. This property is set after the plugin was loaded
   * and unset before the plugin was unloaded. This means at notification time
   * the e.g. provider property still points to a valid object.
   */
  props[PROP_LOADED] =
    g_param_spec_boolean ("loaded",
                          "",
                          "",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * CallsPlugin:provider:
   *
   * The #CallsProvider provided by this plugin, if any
   */
  props[PROP_PROVIDER] =
    g_param_spec_object ("provider",
                         "",
                         "",
                         CALLS_TYPE_PROVIDER,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_plugin_init (CallsPlugin *self)
{
}


CallsPlugin *
calls_plugin_new (PeasPluginInfo *info)
{
  g_return_val_if_fail (info, NULL);

  return g_object_new (CALLS_TYPE_PLUGIN,
                       "plugin-info", info,
                       NULL);
}


gboolean
calls_plugin_load (CallsPlugin *self,
                   GError     **error)
{
  PeasEngine *peas = peas_engine_get_default ();
  GObject *extension;

  g_return_val_if_fail (CALLS_IS_PLUGIN (self), FALSE);

  if (calls_plugin_is_loaded (self))
    return TRUE;

  if (!peas_engine_load_plugin (peas, self->info)) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Plugin '%s' could not be loaded",
                 peas_plugin_info_get_module_name (self->info));
    return FALSE;
  }

  g_assert (peas_plugin_info_is_loaded (self->info));

  if (!peas_engine_provides_extension (peas, self->info, CALLS_TYPE_PROVIDER)) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Plugin '%s' does not provide a CallsProvider",
                 peas_plugin_info_get_module_name (self->info));
    peas_engine_unload_plugin (peas, self->info);
    return FALSE;
  }

  g_debug ("Successfully loaded plugin '%s'",
           peas_plugin_info_get_module_name (self->info));

  extension = peas_engine_create_extension (peas, self->info, CALLS_TYPE_PROVIDER, NULL);
  if (!extension) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not create CallsProvider from plugin '%s'",
                 peas_plugin_info_get_module_name (self->info));
    peas_engine_unload_plugin (peas, self->info);
    return FALSE;
  }

  self->provider = CALLS_PROVIDER (extension);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROVIDER]);

  self->is_loaded = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LOADED]);

  return TRUE;
}


gboolean
calls_plugin_unload (CallsPlugin *self,
                     GError     **error)
{
  PeasEngine *peas = peas_engine_get_default ();

  g_return_val_if_fail (CALLS_IS_PLUGIN (self), FALSE);

  if (!calls_plugin_is_loaded (self))
    return TRUE;

  if (!peas_engine_unload_plugin (peas, self->info)) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not unload plugin '%s'",
                 peas_plugin_info_get_module_name (self->info));
    return FALSE;
  }

  self->is_loaded = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LOADED]);
  g_clear_object (&self->provider);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PROVIDER]);

  return TRUE;
}


gboolean
calls_plugin_is_loaded (CallsPlugin *self)
{
  g_return_val_if_fail (CALLS_IS_PLUGIN (self), FALSE);

  return self->is_loaded;
}


CallsProvider *
calls_plugin_get_provider (CallsPlugin *self)
{
  g_return_val_if_fail (CALLS_IS_PLUGIN (self), NULL);

  return self->provider;
}


const char *
calls_plugin_get_module_name (CallsPlugin *self)
{
  g_return_val_if_fail (CALLS_IS_PLUGIN (self), NULL);
  g_return_val_if_fail (self->info, NULL);

  return peas_plugin_info_get_module_name (self->info);
}

const char *
calls_plugin_get_name (CallsPlugin *self)
{
  g_return_val_if_fail (CALLS_IS_PLUGIN (self), NULL);
  g_return_val_if_fail (self->info, NULL);

  return peas_plugin_info_get_name (self->info);
}


const char *
calls_plugin_get_description (CallsPlugin *self)
{
  g_return_val_if_fail (CALLS_IS_PLUGIN (self), NULL);
  g_return_val_if_fail (self->info, NULL);

  return peas_plugin_info_get_name (self->info);
}


const char * const *
calls_plugin_get_authors (CallsPlugin *self)
{
  g_return_val_if_fail (CALLS_IS_PLUGIN (self), NULL);
  g_return_val_if_fail (self->info, NULL);

  return peas_plugin_info_get_authors (self->info);
}


const char *
calls_plugin_get_copyright (CallsPlugin *self)
{
  g_return_val_if_fail (CALLS_IS_PLUGIN (self), NULL);
  g_return_val_if_fail (self->info, NULL);

  return peas_plugin_info_get_copyright (self->info);
}

const char *
calls_plugin_get_version (CallsPlugin *self)
{
  g_return_val_if_fail (CALLS_IS_PLUGIN (self), NULL);
  g_return_val_if_fail (self->info, NULL);

  return peas_plugin_info_get_version (self->info);
}
