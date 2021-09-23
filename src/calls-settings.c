/*
 * Copyright (C) 2021 Purism SPC
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
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define G_LOG_DOMAIN "CallsSettings"

#include "calls-settings.h"

#include <gio/gio.h>

/**
 * SECTION: calls-settings
 * @title: CallsSettings
 * @short_description: The application settings
 *
 * Manage application settings
 */

enum {
  PROP_0,
  PROP_AUTO_USE_DEFAULT_ORIGINS,
  PROP_COUNTRY_CODE,
  PROP_AUTOLOAD_PLUGINS,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

struct _CallsSettings {
  GObject parent_instance;

  GSettings *settings;
};

G_DEFINE_TYPE (CallsSettings, calls_settings, G_TYPE_OBJECT)


static void
calls_settings_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  CallsSettings *self = CALLS_SETTINGS (object);

  switch (prop_id) {
  case PROP_AUTO_USE_DEFAULT_ORIGINS:
    calls_settings_set_use_default_origins (self, g_value_get_boolean (value));
    break;

  case PROP_COUNTRY_CODE:
    calls_settings_set_country_code (self, g_value_get_string (value));
    break;

  case PROP_AUTOLOAD_PLUGINS:
    calls_settings_set_autoload_plugins (self, g_value_get_boxed (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}


static void
calls_settings_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  CallsSettings *self = CALLS_SETTINGS (object);

  switch (prop_id) {
  case PROP_AUTO_USE_DEFAULT_ORIGINS:
    g_value_set_boolean (value, calls_settings_get_use_default_origins (self));
    break;

  case PROP_COUNTRY_CODE:
    g_value_set_string (value, calls_settings_get_country_code (self));
    break;

  case PROP_AUTOLOAD_PLUGINS:
    g_value_set_boxed (value, calls_settings_get_autoload_plugins (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}


static void
calls_settings_constructed (GObject *object)
{
  CallsSettings *self = CALLS_SETTINGS (object);

  g_settings_bind (self->settings, "auto-use-default-origins",
                   self, "auto-use-default-origins", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "country-code",
                   self, "country-code", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "autoload-plugins",
                   self, "autoload-plugins", G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (calls_settings_parent_class)->constructed (object);
}


static void
calls_settings_finalize (GObject *object)
{
  CallsSettings *self = CALLS_SETTINGS (object);

  g_object_unref (self->settings);

  G_OBJECT_CLASS (calls_settings_parent_class)->finalize (object);
}


static void
calls_settings_class_init (CallsSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = calls_settings_set_property;
  object_class->get_property = calls_settings_get_property;
  object_class->constructed = calls_settings_constructed;
  object_class->finalize = calls_settings_finalize;

  props[PROP_AUTO_USE_DEFAULT_ORIGINS] =
    g_param_spec_boolean ("auto-use-default-origins",
                          "auto use default origins",
                          "Automatically use default origins",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  props[PROP_COUNTRY_CODE] =
    g_param_spec_string ("country-code",
                         "country code",
                         "The country code (usually from the modem)",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_AUTOLOAD_PLUGINS] =
    g_param_spec_boxed ("autoload-plugins",
                        "autoload plugins",
                        "The plugins to automatically load on startup",
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}

static void
calls_settings_init (CallsSettings *self)
{
  self->settings = g_settings_new ("org.gnome.Calls");
}

/**
 * calls_settings_new:
 *
 * Returns: (transfer full): A #CallsSettings.
 */
CallsSettings *
calls_settings_new (void)
{
  return g_object_new (CALLS_TYPE_SETTINGS, NULL);
}

/**
 * calls_settings_get_use_default_origins:
 * @self: A #CallsSettings
 *
 * Whether to prompt the user when there multiple origigins or fall back to defaults
 *
 * Returns: %TRUE if using defaults, %FALSE when the user should be prompted
 */
gboolean
calls_settings_get_use_default_origins (CallsSettings *self)
{
  g_return_val_if_fail (CALLS_IS_SETTINGS (self), FALSE);

  return g_settings_get_boolean (G_SETTINGS (self->settings), "auto-use-default-origins");
}

/**
 * calls_settings_set_use_default_origins:
 * @self: A #CallsSettings
 * @enable: %TRUE to use default origins, %FALSE otherwise
 *
 * Sets whether to prompt the user when there multiple origigins or fall back to defaults
 */
void
calls_settings_set_use_default_origins (CallsSettings *self,
                                        gboolean       enable)
{
  g_return_if_fail (CALLS_IS_SETTINGS (self));

  g_debug ("%sabling the use of default origins", enable ? "En" : "Dis");
  g_settings_set_boolean (G_SETTINGS (self->settings), "auto-use-default-origins", enable);
}

/**
 * calls_settings_get_country_code:
 * @self: A #CallsSettings
 *
 * Whether to prompt the user when there multiple origigins or fall back to defaults
 *
 * Returns: (transfer full): The used country code
 */
char *
calls_settings_get_country_code (CallsSettings *self)
{
  g_return_val_if_fail (CALLS_IS_SETTINGS (self), NULL);

  return g_settings_get_string (G_SETTINGS (self->settings), "country-code");
}

/**
 * calls_settings_set_country_code:
 * @self: A #CallsSettings
 * @country_code: The country code to set
 *
 * Sets the country code
 */
void
calls_settings_set_country_code (CallsSettings *self,
                                 const char    *country_code)
{
  g_return_if_fail (CALLS_IS_SETTINGS (self));

  g_debug ("Setting country code to %s", country_code);
  g_settings_set_string (G_SETTINGS (self->settings), "country-code", country_code);
}


char **
calls_settings_get_autoload_plugins (CallsSettings *self)
{
  g_return_val_if_fail (CALLS_IS_SETTINGS (self), NULL);

  return g_settings_get_strv (G_SETTINGS (self->settings), "autoload-plugins");
}


void
calls_settings_set_autoload_plugins (CallsSettings      *self,
                                     const char * const *plugins)
{
  g_return_if_fail (CALLS_IS_SETTINGS (self));

  g_settings_set_strv (G_SETTINGS (self->settings), "autoload-plugins", plugins);
}
