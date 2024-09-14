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
  PROP_PREFERRED_AUDIO_CODECS,
  PROP_ALWAYS_ALLOW_SDES,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

struct _CallsSettings {
  GObject    parent_instance;

  GSettings *settings;

  GStrv      autoload_plugins;
  GStrv      preferred_audio_codecs;
  gboolean   always_allow_sdes;
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

  case PROP_PREFERRED_AUDIO_CODECS:
    calls_settings_set_preferred_audio_codecs (self, g_value_get_boxed (value));
    break;

  case PROP_ALWAYS_ALLOW_SDES:
    calls_settings_set_always_allow_sdes (self, g_value_get_boolean (value));
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
    g_value_take_string (value, calls_settings_get_country_code (self));
    break;

  case PROP_AUTOLOAD_PLUGINS:
    g_value_take_boxed (value, calls_settings_get_autoload_plugins (self));
    break;

  case PROP_PREFERRED_AUDIO_CODECS:
    g_value_take_boxed (value, calls_settings_get_preferred_audio_codecs (self));
    break;

  case PROP_ALWAYS_ALLOW_SDES:
    g_value_set_boolean (value, calls_settings_get_always_allow_sdes (self));
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

  G_OBJECT_CLASS (calls_settings_parent_class)->constructed (object);

  /**
   * The country code is the only persistent setting which should be written
   * from within Calls (by looking at the MCC of the network the modem is connected to)
   */
  g_settings_bind (self->settings, "country-code",
                   self, "country-code", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "auto-use-default-origins",
                   self, "auto-use-default-origins", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "autoload-plugins",
                   self, "autoload-plugins", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "preferred-audio-codecs",
                   self, "preferred-audio-codecs", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "always-allow-sdes",
                   self, "always-allow-sdes", G_SETTINGS_BIND_DEFAULT);
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
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_COUNTRY_CODE] =
    g_param_spec_string ("country-code",
                         "country code",
                         "The country code (usually from the modem)",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_AUTOLOAD_PLUGINS] =
    g_param_spec_boxed ("autoload-plugins",
                        "autoload plugins",
                        "The plugins to automatically load on startup",
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PREFERRED_AUDIO_CODECS] =
    g_param_spec_boxed ("preferred-audio-codecs",
                        "Preferred audio codecs",
                        "The audio codecs to prefer for VoIP calls",
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ALWAYS_ALLOW_SDES] =
    g_param_spec_boolean ("always-allow-sdes",
                          "Always allow SDES",
                          "Whether to always allow using key exchange (without TLS)",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}

static void
calls_settings_init (CallsSettings *self)
{
  self->settings = g_settings_new ("org.gnome.Calls");
}

/**
 * calls_settings_get_default:
 *
 * Returns: (transfer none): A #CallsSettings.
 */
CallsSettings *
calls_settings_get_default (void)
{
  static CallsSettings *instance = NULL;

  if (!instance) {
    instance = g_object_new (CALLS_TYPE_SETTINGS, NULL);
    g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *) &instance);
  }

  return instance;
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

/**
 * calls_settings_get_autoload_plugins:
 * @self: A #CallsSettings
 *
 * Returns: (array zero-terminated=1) (transfer full): List of plugins that are automatically loaded on startup.
 * Free with g_strfreev() when done.
 */
char **
calls_settings_get_autoload_plugins (CallsSettings *self)
{
  g_return_val_if_fail (CALLS_IS_SETTINGS (self), NULL);

  return g_settings_get_strv (G_SETTINGS (self->settings), "autoload-plugins");
}

/**
 * calls_settings_set_autoload_plugins:
 * @self: A #CallsSettings
 * @plugins: (array zero-terminated=1): The plugins to autoload
 *
 * Sets the plugins that should be loaded on startup.
 */
void
calls_settings_set_autoload_plugins (CallsSettings      *self,
                                     const char * const *plugins)
{
  gboolean initial = TRUE;

  g_return_if_fail (CALLS_IS_SETTINGS (self));
  g_return_if_fail (plugins);

  if (self->autoload_plugins) {
    initial = FALSE;
    if (g_strv_equal (plugins, (const char * const *) self->autoload_plugins))
      return;
  }

  g_strfreev (self->autoload_plugins);
  self->autoload_plugins = g_strdupv ((char **) plugins);

  if (!initial)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_AUTOLOAD_PLUGINS]);
}


/**
 * calls_settings_get_preferred_audio_codecs:
 * @self: A #CallsSettings
 *
 * Returns: (transfer full): List of preferred audio codecs for VoIP calls.
 * Free with g_strfreev() when done.
 */
char **
calls_settings_get_preferred_audio_codecs (CallsSettings *self)
{
  g_return_val_if_fail (CALLS_IS_SETTINGS (self), NULL);

  return g_strdupv (self->preferred_audio_codecs);
}

/**
 * calls_settings_set_preferred_audio_codecs:
 * @self: A #CallsSettings
 * @codecs: (array zero-terminated=1): The preferred codecs
 *
 * Set the preferred audio codecs for VoIP calls.
 */
void
calls_settings_set_preferred_audio_codecs (CallsSettings      *self,
                                           const char * const *codecs)
{
  gboolean initial = TRUE;

  g_return_if_fail (CALLS_IS_SETTINGS (self));
  g_return_if_fail (codecs);

  if (self->preferred_audio_codecs) {
    initial = FALSE;

    if (g_strv_equal (codecs, (const char * const *) self->preferred_audio_codecs))
      return;
  }

  g_strfreev (self->preferred_audio_codecs);
  self->preferred_audio_codecs = g_strdupv ((char **) codecs);

  if (!initial)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PREFERRED_AUDIO_CODECS]);
}


gboolean
calls_settings_get_always_allow_sdes (CallsSettings *self)
{
  g_return_val_if_fail (CALLS_IS_SETTINGS (self), FALSE);

  return self->always_allow_sdes;
}


void
calls_settings_set_always_allow_sdes (CallsSettings *self,
                                      gboolean       allowed)
{
  g_return_if_fail (CALLS_IS_SETTINGS (self));

  if (self->always_allow_sdes == allowed)
    return;

  self->always_allow_sdes = allowed;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALWAYS_ALLOW_SDES]);
}
