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

#define G_LOG_DOMAIN "CallsSipProvider"

#define SIP_ACCOUNT_FILE "sip-account.cfg"
#define CALLS_PROTOCOL_SIP_STR "sip"

#include "calls-config.h"

#include "calls-account-provider.h"
#include "calls-message-source.h"
#include "calls-provider.h"
#include "calls-secret-store.h"
#include "calls-sip-account-widget.h"
#include "calls-sip-enums.h"
#include "calls-sip-origin.h"
#include "calls-sip-provider.h"
#include "calls-sip-util.h"
#include "calls-util.h"

#include <libpeas.h>
#include <sofia-sip/nua.h>
#include <sofia-sip/su_glib.h>

static const char * const supported_protocols[] = {
  "tel",
  "sip",
  "sips",
  NULL
};

/**
 * SECTION:sip-provider
 * @short_description: A #CallsProvider for the SIP protocol
 * @Title: CallsSipProvider
 *
 * #CallsSipProvider is derived from #CallsProvider and is responsible
 * for setting up the sofia-sip stack.
 */

enum {
  PROP_0,
  PROP_SIP_STATE,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

struct _CallsSipProvider {
  CallsProvider          parent_instance;

  GListStore            *origins;
  /* SIP */
  CallsSipContext       *ctx;
  SipEngineState         sip_state;

  gboolean               use_memory_backend;
  gchar                 *filename;

  CallsSipAccountWidget *account_widget;
};

static void calls_sip_provider_message_source_interface_init (CallsMessageSourceInterface *iface);
static void calls_sip_provider_account_provider_interface_init (CallsAccountProviderInterface *iface);

#ifdef FOR_TESTING

G_DEFINE_TYPE_WITH_CODE
  (CallsSipProvider, calls_sip_provider, CALLS_TYPE_PROVIDER,
  G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                         calls_sip_provider_message_source_interface_init)
  G_IMPLEMENT_INTERFACE (CALLS_TYPE_ACCOUNT_PROVIDER,
                         calls_sip_provider_account_provider_interface_init))

#else

G_DEFINE_DYNAMIC_TYPE_EXTENDED
  (CallsSipProvider, calls_sip_provider, CALLS_TYPE_PROVIDER, 0,
  G_IMPLEMENT_INTERFACE_DYNAMIC (CALLS_TYPE_MESSAGE_SOURCE,
                                 calls_sip_provider_message_source_interface_init)
  G_IMPLEMENT_INTERFACE_DYNAMIC (CALLS_TYPE_ACCOUNT_PROVIDER,
                                 calls_sip_provider_account_provider_interface_init))

#endif /* FOR_TESTING */

typedef struct {
  CallsSipProvider *provider;
  GKeyFile         *key_file;
  char             *name;
} SipOriginLoadData;

static void
on_origin_pw_looked_up (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  SipOriginLoadData *data;

  g_autoptr (GError) error = NULL;
  g_autofree char *id = NULL;
  g_autofree char *host = NULL;
  g_autofree char *user = NULL;
  g_autofree char *password = NULL;
  g_autofree char *display_name = NULL;
  g_autofree char *protocol = NULL;
  gint port = 0;
  gint local_port = 0;
  gboolean auto_connect = TRUE;
  gboolean direct_mode = FALSE;
  gboolean can_tel = FALSE;
  SipMediaEncryption media_encryption = SIP_MEDIA_ENCRYPTION_NONE;

  g_assert (user_data);

  data = user_data;

  if (g_key_file_has_key (data->key_file, data->name, "Id", NULL))
    id = g_key_file_get_string (data->key_file, data->name, "Id", NULL);
  else
    id = g_strdup (data->name);

  g_debug ("Password looked up for %s", id);

  host = g_key_file_get_string (data->key_file, data->name, "Host", NULL);
  user = g_key_file_get_string (data->key_file, data->name, "User", NULL);
  display_name = g_key_file_get_string (data->key_file, data->name, "DisplayName", NULL);
  protocol = g_key_file_get_string (data->key_file, data->name, "Protocol", NULL);
  port = g_key_file_get_integer (data->key_file, data->name, "Port", NULL);
  local_port = g_key_file_get_integer (data->key_file, data->name, "LocalPort", NULL);

  if (g_key_file_has_key (data->key_file, data->name, "AutoConnect", NULL))
    auto_connect = g_key_file_get_boolean (data->key_file, data->name, "AutoConnect", NULL);

  if (protocol == NULL)
    protocol = g_strdup ("UDP");

  if (g_key_file_has_key (data->key_file, data->name, "DirectMode", NULL))
    direct_mode = g_key_file_get_boolean (data->key_file, data->name, "DirectMode", NULL);

  if (g_key_file_has_key (data->key_file, data->name, "CanTel", NULL))
    can_tel =
      g_key_file_get_boolean (data->key_file, data->name, "CanTel", NULL);

  if (g_key_file_has_key (data->key_file, data->name, "MediaEncryption", NULL))
    media_encryption =
      (SipMediaEncryption) g_key_file_get_integer (data->key_file, data->name, "MediaEncryption", NULL);

  g_key_file_unref (data->key_file);

  /* PW */
  password = secret_password_lookup_finish (result, &error);
  if (!direct_mode && error) {
    g_warning ("Could not lookup password: %s", error->message);
    return;
  }

  if (!direct_mode &&
      (STR_IS_NULL_OR_EMPTY (host) ||
       STR_IS_NULL_OR_EMPTY (user) ||
       STR_IS_NULL_OR_EMPTY (password))) {
    g_warning ("Host, user and password must not be empty");

    return;
  }

  calls_sip_provider_add_origin_full (data->provider,
                                      id,
                                      host,
                                      user,
                                      password,
                                      display_name,
                                      protocol,
                                      port,
                                      media_encryption,
                                      auto_connect,
                                      direct_mode,
                                      local_port,
                                      can_tel,
                                      FALSE);
}


static void
new_origin_from_keyfile_secret (CallsSipProvider *self,
                                GKeyFile         *key_file,
                                const char       *name)
{
  g_autofree char *host = NULL;
  g_autofree char *user = NULL;
  SipOriginLoadData *data;

  g_assert (CALLS_IS_SIP_PROVIDER (self));
  g_assert (key_file);
  g_assert (name);

  if (!g_key_file_has_group (key_file, name)) {
    g_warning ("Keyfile has no group %s", name);
    return;
  }

  host = g_key_file_get_string (key_file, name, "Host", NULL);
  user = g_key_file_get_string (key_file, name, "User", NULL);

  data = g_new0 (SipOriginLoadData, 1);
  data->provider = self;
  data->key_file = g_key_file_ref (key_file);
  data->name = g_strdup (name);

  g_debug ("Looking up password for account '%s'", name);
  secret_password_lookup (calls_secret_get_schema (), NULL,
                          on_origin_pw_looked_up, data,
                          CALLS_SERVER_ATTRIBUTE, host,
                          CALLS_USERNAME_ATTRIBUTE, user,
                          CALLS_PROTOCOL_ATTRIBUTE, CALLS_PROTOCOL_SIP_STR,
                          NULL);
}


static void
on_origin_pw_cleared (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!secret_password_clear_finish (result, &error))
    g_warning ("Could not delete the password in the keyring: %s",
               error ? error->message : "No reason given");
}


static void
origin_pw_delete_secret (CallsSipOrigin *origin)
{
  g_autofree char *host = NULL;
  g_autofree char *user = NULL;

  g_assert (CALLS_IS_SIP_ORIGIN (origin));

  g_object_get (origin,
                "host", &host,
                "user", &user,
                NULL);

  secret_password_clear (calls_secret_get_schema (), NULL, on_origin_pw_cleared, NULL,
                         CALLS_SERVER_ATTRIBUTE, host,
                         CALLS_USERNAME_ATTRIBUTE, user,
                         CALLS_PROTOCOL_ATTRIBUTE, CALLS_PROTOCOL_SIP_STR,
                         NULL);
}


static void
on_origin_pw_saved (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!secret_password_store_finish (result, &error)) {
    g_warning ("Could not store the password in the keyring: %s",
               error ? error->message : "No reason given");
  }
}


static void
origin_to_keyfile (CallsSipOrigin *origin,
                   GKeyFile       *key_file,
                   const char     *name)
{
  g_autofree char *id = NULL;
  g_autofree char *host = NULL;
  g_autofree char *user = NULL;
  g_autofree char *password = NULL;
  g_autofree char *display_name = NULL;
  g_autofree char *protocol = NULL;
  g_autofree char *label_secret = NULL;
  gint port;
  gint local_port;
  gboolean auto_connect;
  gboolean direct_mode;
  gboolean can_tel;
  SipMediaEncryption media_encryption;

  g_assert (CALLS_IS_SIP_ORIGIN (origin));
  g_assert (key_file);

  g_object_get (origin,
                "id", &id,
                "host", &host,
                "user", &user,
                "password", &password,
                "display-name", &display_name,
                "transport-protocol", &protocol,
                "port", &port,
                "auto-connect", &auto_connect,
                "direct-mode", &direct_mode,
                "local-port", &local_port,
                "can-tel", &can_tel,
                "media-encryption", &media_encryption,
                NULL);

  g_key_file_set_string (key_file, name, "Id", id);
  g_key_file_set_string (key_file, name, "Host", host);
  g_key_file_set_string (key_file, name, "User", user);
  g_key_file_set_string (key_file, name, "DisplayName", display_name ?: "");
  g_key_file_set_string (key_file, name, "Protocol", protocol);
  g_key_file_set_integer (key_file, name, "Port", port);
  g_key_file_set_boolean (key_file, name, "AutoConnect", auto_connect);
  g_key_file_set_boolean (key_file, name, "DirectMode", direct_mode);
  g_key_file_set_integer (key_file, name, "LocalPort", local_port);
  g_key_file_set_boolean (key_file, name, "CanTel", can_tel);
  g_key_file_set_integer (key_file, name, "MediaEncryption", media_encryption);

  label_secret = g_strdup_printf ("Calls Password for %s", id);

  /* save to keyring */
  secret_password_store (calls_secret_get_schema (), NULL, label_secret, password,
                         NULL, on_origin_pw_saved, NULL,
                         CALLS_SERVER_ATTRIBUTE, host,
                         CALLS_USERNAME_ATTRIBUTE, user,
                         CALLS_PROTOCOL_ATTRIBUTE, CALLS_PROTOCOL_SIP_STR,
                         NULL);
}


static const char *
calls_sip_provider_get_name (CallsProvider *provider)
{
  return "SIP provider";
}


static const char *
calls_sip_provider_get_status (CallsProvider *provider)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (provider);

  switch (self->sip_state) {
  case SIP_ENGINE_ERROR:
    return "Error";

  case SIP_ENGINE_READY:
    return "Normal";

  default:
    break;
  }
  return "Unknown";
}


static GListModel *
calls_sip_provider_get_origins (CallsProvider *provider)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (provider);

  return G_LIST_MODEL (self->origins);
}

static const char *const *
calls_sip_provider_get_protocols (CallsProvider *provider)
{
  return supported_protocols;
}


static void
calls_sip_provider_deinit_sip (CallsSipProvider *self)
{
  GSource *gsource;

  if (self->sip_state == SIP_ENGINE_NULL)
    return;

  /* clean up sofia */
  if (!self->ctx)
    goto bail;


  if (self->ctx->root) {
    gsource = su_glib_root_gsource (self->ctx->root);
    g_source_destroy (gsource);
    su_root_destroy (self->ctx->root);

    if (su_home_unref (self->ctx->home) != 1)
      g_warning ("Error in su_home_unref ()");
  }
  g_clear_pointer (&self->ctx, g_free);

bail:
  self->sip_state = SIP_ENGINE_NULL;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SIP_STATE]);
}


static gboolean
calls_sip_provider_init_sofia (CallsSipProvider *self,
                               GError          **error)
{
  GSource *gsource;

  g_assert (CALLS_SIP_PROVIDER (self));

  self->ctx = g_new0 (CallsSipContext, 1);
  if (self->ctx == NULL) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Could not allocate memory for the SIP context");
    goto err;
  }

  if (su_init () != su_success) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "su_init () failed");
    goto err;
  }

  if (su_home_init (self->ctx->home) != su_success) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "su_home_init () failed");
    goto err;
  }

  self->ctx->root = su_glib_root_create (self);
  if (self->ctx->root == NULL) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "su_glib_root_create () failed");
    goto err;
  }
  gsource = su_glib_root_gsource (self->ctx->root);
  if (gsource == NULL) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "su_glib_root_gsource () failed");
    goto err;
  }

  g_source_attach (gsource, NULL);
  self->sip_state = SIP_ENGINE_READY;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SIP_STATE]);
  return TRUE;

err:
  self->sip_state = SIP_ENGINE_ERROR;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SIP_STATE]);
  return FALSE;
}


static void
calls_sip_provider_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (object);

  switch (property_id) {
  case PROP_SIP_STATE:
    g_value_set_enum (value, self->sip_state);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_sip_provider_constructed (GObject *object)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (object);
  g_autoptr (GError) error = NULL;

  G_OBJECT_CLASS (calls_sip_provider_parent_class)->constructed (object);

  if (calls_sip_provider_init_sofia (self, &error)) {
    if (!self->use_memory_backend) {
      g_autoptr (GKeyFile) key_file = g_key_file_new ();

      if (!g_key_file_load_from_file (key_file, self->filename, G_KEY_FILE_NONE, &error)) {
        if (error->domain == G_FILE_ERROR &&
            error->code == G_FILE_ERROR_NOENT)
          g_debug ("Not loading SIP accounts: No such file '%s'", self->filename);
        else
          g_warning ("Error loading keyfile '%s': %s", self->filename, error->message);

        return;
      }

      calls_sip_provider_load_accounts (self, key_file);
    }
  } else {
    g_warning ("Could not initialize sofia stack: %s", error->message);
  }

}


static void
calls_sip_provider_dispose (GObject *object)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (object);

  g_list_store_remove_all (self->origins);
  g_clear_object (&self->origins);
  g_clear_object (&self->account_widget);

  g_clear_pointer (&self->filename, g_free);

  calls_sip_provider_deinit_sip (self);

  G_OBJECT_CLASS (calls_sip_provider_parent_class)->dispose (object);
}


#ifndef FOR_TESTING

static void
calls_sip_provider_class_finalize (CallsSipProviderClass *klass)
{
}

#endif /* FOR_TESTING */

static void
calls_sip_provider_class_init (CallsSipProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsProviderClass *provider_class = CALLS_PROVIDER_CLASS (klass);

  object_class->constructed = calls_sip_provider_constructed;
  object_class->dispose = calls_sip_provider_dispose;
  object_class->get_property = calls_sip_provider_get_property;

  provider_class->get_name = calls_sip_provider_get_name;
  provider_class->get_status = calls_sip_provider_get_status;
  provider_class->get_origins = calls_sip_provider_get_origins;
  provider_class->get_protocols = calls_sip_provider_get_protocols;

  props[PROP_SIP_STATE] =
    g_param_spec_enum ("sip-state",
                       "SIP state",
                       "The state of the SIP engine",
                       SIP_TYPE_ENGINE_STATE,
                       SIP_ENGINE_NULL,
                       G_PARAM_READABLE);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_sip_provider_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}

static void
ensure_account_widget (CallsSipProvider *self)
{
  if (!self->account_widget) {
    self->account_widget = calls_sip_account_widget_new (self);
    g_object_ref_sink (self->account_widget);
  }
}

static GtkWidget *
get_account_widget (CallsAccountProvider *provider)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (provider);

  ensure_account_widget (self);
  return GTK_WIDGET (self->account_widget);
}

static void
add_new_account (CallsAccountProvider *provider)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (provider);

  ensure_account_widget (self);
  calls_sip_account_widget_set_origin (self->account_widget, NULL);
}

static void
edit_account (CallsAccountProvider *provider,
              CallsAccount         *account)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (provider);

  ensure_account_widget (self);
  calls_sip_account_widget_set_origin (self->account_widget, CALLS_SIP_ORIGIN (account));
}

static void
calls_sip_provider_account_provider_interface_init (CallsAccountProviderInterface *iface)
{
  iface->get_account_widget = get_account_widget;
  iface->add_new_account = add_new_account;
  iface->edit_account = edit_account;
}

static void
calls_sip_provider_init (CallsSipProvider *self)
{
  g_autofree char *directory = NULL;
  const char *filename_env = g_getenv ("CALLS_SIP_ACCOUNT_FILE");
  const char *sip_test_env = g_getenv ("CALLS_SIP_TEST");

  self->origins = g_list_store_new (CALLS_TYPE_ORIGIN);

  if (!STR_IS_NULL_OR_EMPTY (filename_env))
    self->filename = g_strdup (filename_env);
  else
    self->filename = g_build_filename (g_get_user_config_dir (),
                                       APP_DATA_NAME,
                                       SIP_ACCOUNT_FILE,
                                       NULL);

  if (STR_IS_NULL_OR_EMPTY (sip_test_env)) {
    directory = g_path_get_dirname (self->filename);
    if (g_mkdir_with_parents (directory, 0750) == -1) {
      int err_save = errno;
      g_warning ("Failed to create directory '%s': %d\n"
                 "Can not store credentials persistently!",
                 directory, err_save);
    }
  } else {
    self->use_memory_backend = TRUE;
  }

}

/**
 * calls_sip_provider_add_origin:
 * @self: A #CallsSipProvider
 * @id: The id of the new origin (should be unique)
 * @host: The host to connect to
 * @user: The username to use
 * @password: The password to use
 * @display_name: The display name
 * @transport_protocol: The transport protocol to use, can be one of "UDP", "TCP" or "TLS"
 * @port: The port of the host
 * @media_encryption: A #SipMediaEncryption specifying if media should be encrypted
 * @store_credentials: Whether to store credentials for this origin to disk
 *
 * Adds a new origin (SIP account)
 *
 * Return: (transfer none): A #CallsSipOrigin
 */
CallsSipOrigin *
calls_sip_provider_add_origin (CallsSipProvider  *self,
                               const char        *id,
                               const char        *host,
                               const char        *user,
                               const char        *password,
                               const char        *display_name,
                               const char        *transport_protocol,
                               gint               port,
                               SipMediaEncryption media_encryption,
                               gboolean           store_credentials)
{
  return calls_sip_provider_add_origin_full (self,
                                             id,
                                             host,
                                             user,
                                             password,
                                             display_name,
                                             transport_protocol,
                                             port,
                                             media_encryption,
                                             TRUE,
                                             FALSE,
                                             0,
                                             FALSE,
                                             store_credentials);
}

/**
 * calls_sip_provider_add_origin_full:
 * @self: A #CallsSipProvider
 * @id: The id of the new origin (should be unique)
 * @host: The host to connect to
 * @user: The username to use
 * @password: The password to use
 * @display_name: The display name
 * @transport_protocol: The transport protocol to use, can be one of "UDP", "TCP" or "TLS"
 * @port: The port of the host
 * @media_encryption: A #SipMediaEncryption specifying if media should be encrypted
 * @auto_connect: Whether to automatically try going online
 * @direct_mode: Whether to use direct connection mode. Useful when you don't want to
 * connect to a SIP server. Mostly useful for testing and debugging.
 * @can_tel: Whether this origin can be used for PSTN telephony
 * @store_credentials: Whether to store credentials for this origin to disk
 *
 * Adds a new origin (SIP account). If @direct_mode is %TRUE then @host, @user and
 * @password do not have to be set.
 *
 * Return: (transfer none): A #CallsSipOrigin
 */
CallsSipOrigin *
calls_sip_provider_add_origin_full (CallsSipProvider  *self,
                                    const char        *id,
                                    const char        *host,
                                    const char        *user,
                                    const char        *password,
                                    const char        *display_name,
                                    const char        *transport_protocol,
                                    gint               port,
                                    SipMediaEncryption media_encryption,
                                    gboolean           auto_connect,
                                    gboolean           direct_mode,
                                    gint               local_port,
                                    gboolean           can_tel,
                                    gboolean           store_credentials)
{
  g_autoptr (CallsSipOrigin) origin = NULL;
  g_autofree char *protocol = NULL;

  g_return_val_if_fail (CALLS_IS_SIP_PROVIDER (self), NULL);
  g_return_val_if_fail (id || *id, NULL);

  /* direct-mode is mostly useful for testing without a SIP server */
  if (!direct_mode) {
    g_return_val_if_fail (host, NULL);
    g_return_val_if_fail (user, NULL);
    g_return_val_if_fail (password, NULL);
  }

  if (transport_protocol) {
    g_return_val_if_fail (protocol_is_valid (transport_protocol), NULL);

    protocol = g_ascii_strup (transport_protocol, -1);
  }

  origin = g_object_new (CALLS_TYPE_SIP_ORIGIN,
                         "id", id,
                         "sip-context", self->ctx,
                         "host", host,
                         "user", user,
                         "password", password,
                         "display-name", display_name,
                         "transport-protocol", protocol ?: "UDP",
                         "port", port,
                         "media-encryption", media_encryption,
                         "auto-connect", auto_connect,
                         "direct-mode", direct_mode,
                         "local-port", local_port,
                         "can-tel", can_tel,
                         NULL);

  g_list_store_append (self->origins, origin);

  if (store_credentials && !self->use_memory_backend)
    calls_sip_provider_save_accounts_to_disk (self);

  return origin;
}


gboolean
calls_sip_provider_remove_origin (CallsSipProvider *self,
                                  CallsSipOrigin   *origin)
{
  guint position;

  g_return_val_if_fail (CALLS_IS_SIP_PROVIDER (self), FALSE);
  g_return_val_if_fail (CALLS_IS_SIP_ORIGIN (origin), FALSE);

  if (g_list_store_find (self->origins, origin, &position)) {
    g_object_ref (origin);
    g_list_store_remove (self->origins, position);

    if (!self->use_memory_backend) {
      origin_pw_delete_secret (origin);
      calls_sip_provider_save_accounts_to_disk (self);
    }
    g_object_unref (origin);
    return TRUE;
  }
  return FALSE;
}


CallsSipProvider *
calls_sip_provider_new (void)
{
  return g_object_new (CALLS_TYPE_SIP_PROVIDER, NULL);
}


void
calls_sip_provider_load_accounts (CallsSipProvider *self,
                                  GKeyFile         *key_file)
{
  g_auto (GStrv) groups = NULL;

  g_return_if_fail (CALLS_IS_SIP_PROVIDER (self));
  g_return_if_fail (key_file);

  groups = g_key_file_get_groups (key_file, NULL);

  g_debug ("Found %u accounts in keyfile '%s'",
           g_strv_length (groups),
           self->filename);

  for (gsize i = 0; groups[i] != NULL; i++) {
    new_origin_from_keyfile_secret (self, key_file, groups[i]);
  }
}


void
calls_sip_provider_save_accounts (CallsSipProvider *self,
                                  GKeyFile         *key_file)
{
  guint n_origins;

  g_return_if_fail (CALLS_IS_SIP_PROVIDER (self));
  g_return_if_fail (key_file);

  n_origins = g_list_model_get_n_items (G_LIST_MODEL (self->origins));
  for (guint i = 0; i < n_origins; i++) {
    g_autoptr (CallsSipOrigin) origin =
      g_list_model_get_item (G_LIST_MODEL (self->origins), i);
    g_autofree char *group_name = g_strdup_printf ("sip-%02d", i);

    origin_to_keyfile (origin, key_file, group_name);
  }
}


gboolean
calls_sip_provider_save_accounts_to_disk (CallsSipProvider *self)
{
  g_autoptr (GKeyFile) key_file = g_key_file_new ();
  g_autoptr (GError) error = NULL;
  gboolean saved = FALSE;

  g_assert (CALLS_IS_SIP_PROVIDER (self));

  calls_sip_provider_save_accounts (self, key_file);

  if (!(saved = g_key_file_save_to_file (key_file, self->filename, &error)))
    g_warning ("Error saving keyfile to file %s: %s", self->filename, error->message);

  return saved;
}


#ifndef FOR_TESTING

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
  calls_sip_provider_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              CALLS_TYPE_PROVIDER,
                                              CALLS_TYPE_SIP_PROVIDER);
}

#endif /* FOR_TESTING */
