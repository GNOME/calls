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

#define G_LOG_DOMAIN "CallsCredentials"

#include "calls-credentials.h"

/**
 * SECTION:Credentials
 * @short_description: Credentials for online accounts
 * @Title: CallsCredentials
 *
 * #CallsCredentials represents account credentials f.e. for SIP.
 */

enum {
  PROP_0,
  PROP_NAME,
  PROP_ACC_HOST,
  PROP_ACC_DISPLAY_NAME,
  PROP_ACC_USER,
  PROP_ACC_PASSWORD,
  PROP_ACC_PORT,
  PROP_ACC_PROTOCOL,
  PROP_ACC_AUTO_CONNECT,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

enum {
  SIGNAL_ACCOUNT_UPDATED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];

struct _CallsCredentials
{
  GObject parent_instance;

  char *name;

  /* Account information */
  char *host;
  char *display_name;
  char *user;
  char *password;
  gint port;
  char *transport_protocol;

  gboolean auto_connect;
};


G_DEFINE_TYPE (CallsCredentials, calls_credentials, G_TYPE_OBJECT)

static gboolean
check_required_keys (GKeyFile    *key_file,
                     const gchar *group_name)
{
  gchar *keys[] = {
    "User",
    "Password",
    "Host",
  };

  g_assert (group_name);
  g_assert (g_key_file_has_group (key_file, group_name));

  for (gsize i = 0; i < G_N_ELEMENTS (keys); i++) {
    if (!g_key_file_has_key (key_file, group_name, keys[i], NULL))
      return FALSE;
  }

  return TRUE;
}


static void
calls_credentials_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  CallsCredentials *self = CALLS_CREDENTIALS (object);

  switch (property_id) {
  case PROP_NAME:
    g_free (self->name);
    self->name = g_value_dup_string (value);
    break;

  case PROP_ACC_HOST:
    g_free (self->host);
    self->host = g_value_dup_string (value);
    break;

  case PROP_ACC_DISPLAY_NAME:
    g_free (self->display_name);
    self->display_name = g_value_dup_string (value);
    break;

  case PROP_ACC_USER:
    g_free (self->user);
    self->user = g_value_dup_string (value);
    break;

  case PROP_ACC_PASSWORD:
    g_free (self->password);
    self->password = g_value_dup_string (value);
    break;

  case PROP_ACC_PORT:
    self->port = g_value_get_int (value);
    break;

  case PROP_ACC_PROTOCOL:
    g_free (self->transport_protocol);
    self->transport_protocol = g_value_dup_string (value);
    break;

  case PROP_ACC_AUTO_CONNECT:
    self->auto_connect = g_value_get_boolean (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_credentials_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  CallsCredentials *self = CALLS_CREDENTIALS (object);

  switch (property_id) {
  case PROP_NAME:
    g_value_set_string (value, self->name);
    break;

  case PROP_ACC_HOST:
    g_value_set_string (value, self->host);
    break;

  case PROP_ACC_DISPLAY_NAME:
    g_value_set_string (value, self->display_name);
    break;

  case PROP_ACC_USER:
    g_value_set_string (value, self->user);
    break;

  case PROP_ACC_PASSWORD:
    g_value_set_string (value, self->password);
    break;

  case PROP_ACC_PORT:
    g_value_set_int (value, self->port);
    break;

  case PROP_ACC_PROTOCOL:
    g_value_set_string (value, self->transport_protocol);
    break;

  case PROP_ACC_AUTO_CONNECT:
    g_value_set_boolean (value, self->auto_connect);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_credentials_finalize (GObject *object)
{
  CallsCredentials *self = CALLS_CREDENTIALS (object);

  g_free (self->name);
  g_free (self->host);
  g_free (self->display_name);
  g_free (self->user);
  g_free (self->password);
  g_free (self->transport_protocol);

  G_OBJECT_CLASS (calls_credentials_parent_class)->finalize (object);
}


static void
calls_credentials_class_init (CallsCredentialsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = calls_credentials_set_property;
  object_class->get_property = calls_credentials_get_property;
  object_class->finalize = calls_credentials_finalize;

  props[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name for this set of credentials",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_ACC_HOST] =
    g_param_spec_string ("host",
                         "Host",
                         "The host to connect to",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_ACC_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display name",
                         "The display name",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_ACC_USER] =
    g_param_spec_string ("user",
                         "User",
                         "The username",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_ACC_PASSWORD] =
    g_param_spec_string ("password",
                         "Password",
                         "The password",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_ACC_PORT] =
    g_param_spec_int ("port",
                      "Port",
                      "The port to connect to",
                      0, 65535, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_ACC_PROTOCOL] =
    g_param_spec_string ("protocol",
                         "Protocol",
                         "The transport protocol to use for the connection",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_ACC_AUTO_CONNECT] =
    g_param_spec_boolean ("auto-connect",
                          "Auto connect",
                          "Whether to connect automatically",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  signals[SIGNAL_ACCOUNT_UPDATED] =
    g_signal_new ("account-updated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
}


static void
calls_credentials_init (CallsCredentials *self)
{
}


CallsCredentials *
calls_credentials_new (void)
{
  return g_object_new (CALLS_TYPE_CREDENTIALS, NULL);
}

/**
 * calls_credentials_update_from_keyfile:
 * @self: A #CallsCredentials
 * @key_file: A #GKeyFile
 * @name: The name of the credentials which doubles as the option group
 *
 * Updates the credentials from a given keyfile.
 *
 * Returns: %TRUE if credentials were updated, %FALSE otherwise
 */
gboolean
calls_credentials_update_from_keyfile (CallsCredentials *self,
                                       GKeyFile         *key_file,
                                       const char       *name)
{
  char *user = NULL;
  char *password = NULL;
  char *host = NULL;
  char *protocol = NULL;
  char *display_name = NULL;
  gint port = 0;
  gboolean auto_connect = TRUE;

  g_return_val_if_fail (CALLS_IS_CREDENTIALS (self), FALSE);
  g_return_val_if_fail (name, FALSE);
  g_return_val_if_fail (g_key_file_has_group (key_file, name), FALSE);

  if (!check_required_keys (key_file, name)) {
    g_warning ("Not all required keys found in section %s", name);
    return FALSE;
  }

  user = g_key_file_get_string (key_file, name, "User", NULL);
  password = g_key_file_get_string (key_file, name, "Password", NULL);
  host = g_key_file_get_string (key_file, name, "Host", NULL);
  protocol = g_key_file_get_string (key_file, name, "Protocol", NULL);
  port = g_key_file_get_integer (key_file, name, "Port", NULL);
  display_name = g_key_file_get_string (key_file, name, "DisplayName", NULL);

  if (g_key_file_has_key (key_file, name, "AutoConnect", NULL))
    auto_connect = g_key_file_get_boolean (key_file, name, "AutoConnect", NULL);

  if (protocol == NULL)
    protocol = g_strdup ("UDP");

  if (g_strcmp0 (host, "") == 0 ||
      g_strcmp0 (user, "") == 0 ||
      g_strcmp0 (password, "") == 0) {
    g_warning ("Host, user and password must not be empty");

    g_free (user);
    g_free (password);
    g_free (host);
    g_free (protocol);
    g_free (display_name);

    return FALSE;
  }

  g_free (self->name);
  self->name = g_strdup (name);

  g_free (self->host);
  self->host = host;

  g_free (self->user);
  self->user = user;

  g_free (self->password);
  self->password = password;

  g_free (self->transport_protocol);
  self->transport_protocol = protocol;

  g_free (self->display_name);
  self->display_name = display_name;

  self->port = port;
  self->auto_connect = auto_connect;

  g_debug ("Updated credentials with name %s", name);

  g_signal_emit (self, signals[SIGNAL_ACCOUNT_UPDATED], 0);

  return TRUE;
}

const char *
calls_credentials_get_name (CallsCredentials *self)
{
  g_return_val_if_fail (CALLS_IS_CREDENTIALS (self), NULL);

  return self->name;
}

void
calls_credentials_set_name (CallsCredentials *self,
                            const char       *name)
{
  g_return_if_fail (CALLS_IS_CREDENTIALS (self));

  if (!name)
    return;

  if (g_strcmp0 (name, self->name) == 0)
    return;

  g_free (self->name);
  self->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}
