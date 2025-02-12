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

#define G_LOG_DOMAIN "CallsOfonoOrigin"

#include "calls-ofono-origin.h"
#include "calls-origin.h"
#include "calls-ussd.h"
#include "calls-ofono-call.h"
#include "calls-message-source.h"
#include "calls-util.h"

#include <glib/gi18n.h>


struct _CallsOfonoOrigin {
  GObject               parent_instance;
  GDBusConnection      *connection;
  GDBOModem            *modem;
  gchar                *name;
  GDBOVoiceCallManager *voice;
  GDBOSupplementaryServices *ussd;
  gboolean              sending_tones;
  GString              *tone_queue;
  GHashTable           *calls;

  CallsUssdState        ussd_state;
};

static void calls_ofono_origin_message_source_interface_init (CallsOriginInterface *iface);
static void calls_ofono_origin_origin_interface_init (CallsOriginInterface *iface);
static void calls_ofono_origin_ussd_interface_init (CallsUssdInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsOfonoOrigin, calls_ofono_origin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_ofono_origin_message_source_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_USSD,
                                                calls_ofono_origin_ussd_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_ORIGIN,
                                                calls_ofono_origin_origin_interface_init))


enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_CALLS,
  PROP_MODEM,
  PROP_COUNTRY_CODE,
  PROP_EMERGENCY_NUMBERS,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
ussd_initiate_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  g_autoptr (GTask) task = user_data;
  GDBOSupplementaryServices *ussd = GDBO_SUPPLEMENTARY_SERVICES (object);
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (user_data);
  g_autoptr (GVariant) variant = NULL;
  char *response = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);

  g_assert (GDBO_IS_SUPPLEMENTARY_SERVICES (ussd));
  g_assert (CALLS_IS_OFONO_ORIGIN (self));

  gdbo_supplementary_services_call_initiate_finish (ussd, NULL,
                                                    &variant, result, &error);

  if (error) {
    g_task_return_error (task, error);
  } else {
    g_autoptr (GVariant) child = g_variant_get_child_value (variant, 0);
    response = g_variant_dup_string (child, NULL);
    self->ussd_state = CALLS_USSD_STATE_USER_RESPONSE;
    g_task_return_pointer (task, response, g_free);
  }
}

static void
ussd_reinitiate_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr (GTask) task = user_data;
  CallsUssd *ussd = CALLS_USSD (object);
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (user_data);
  GCancellable *cancellable;
  GError *error = NULL;
  const char *command;

  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);

  g_assert (CALLS_IS_USSD (ussd));
  g_assert (CALLS_IS_OFONO_ORIGIN (self));

  calls_ussd_cancel_finish (ussd, result, &error);
  cancellable = g_task_get_cancellable (task);
  command = g_task_get_task_data (task);

  if (error)
    g_task_return_error (task, error);
  else {
    self->ussd_state = CALLS_USSD_STATE_ACTIVE;
    gdbo_supplementary_services_call_initiate (self->ussd, command, cancellable,
                                               ussd_initiate_cb, g_steal_pointer (&task));
  }
}

static void
ussd_respond_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  g_autoptr (GTask) task = user_data;
  GDBOSupplementaryServices *ussd = GDBO_SUPPLEMENTARY_SERVICES (object);
  CallsOfonoOrigin *self;
  char *response = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);

  g_assert (CALLS_IS_OFONO_ORIGIN (self));
  g_assert (GDBO_IS_SUPPLEMENTARY_SERVICES (ussd));

  gdbo_supplementary_services_call_respond_finish (ussd, &response, result, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, response, g_free);
}

static void
ussd_cancel_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr (GTask) task = user_data;
  GDBOSupplementaryServices *ussd = GDBO_SUPPLEMENTARY_SERVICES (object);
  CallsOfonoOrigin *self;
  GError *error = NULL;
  gboolean response;

  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);

  g_assert (CALLS_IS_OFONO_ORIGIN (self));
  g_assert (GDBO_IS_SUPPLEMENTARY_SERVICES (ussd));

  response = gdbo_supplementary_services_call_cancel_finish (ussd, result, &error);
  self->ussd_state = CALLS_USSD_STATE_IDLE;

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, response);
}

static CallsUssdState
calls_ofono_ussd_get_state (CallsUssd *ussd)
{
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (ussd);

  if (!self->ussd)
    self->ussd_state = CALLS_USSD_STATE_UNKNOWN;

  return self->ussd_state;
}

static void
calls_ofono_ussd_initiate_async (CallsUssd          *ussd,
                                 const char         *command,
                                 GCancellable       *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer            user_data)
{
  g_autoptr (GTask) task = NULL;
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (ussd);
  CallsUssdState state;

  g_return_if_fail (CALLS_IS_USSD (ussd));

  task = g_task_new (self, cancellable, callback, user_data);

  if (!self->ussd) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                             "No USSD interface found");
    return;
  }

  if (STR_IS_NULL_OR_EMPTY (command)) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "USSD command empty");
    return;
  }

  state = calls_ussd_get_state (CALLS_USSD (self));
  g_task_set_task_data (task, g_strdup (command), g_free);

  if (state == CALLS_USSD_STATE_ACTIVE ||
      state == CALLS_USSD_STATE_USER_RESPONSE)
    calls_ussd_cancel_async (CALLS_USSD (self), cancellable,
                             ussd_reinitiate_cb, g_steal_pointer (&task));
  else {
    self->ussd_state = CALLS_USSD_STATE_ACTIVE;
    gdbo_supplementary_services_call_initiate (self->ussd, command, cancellable,
                                               ussd_initiate_cb, g_steal_pointer (&task));
  }
}

static char *
calls_ofono_ussd_initiate_finish (CallsUssd    *ussd,
                                  GAsyncResult *result,
                                  GError      **error)
{
  g_return_val_if_fail (CALLS_IS_USSD (ussd), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
calls_ofono_ussd_respond_async (CallsUssd          *ussd,
                                const char         *response,
                                GCancellable       *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer            user_data)
{
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (ussd);
  GTask *task;

  g_return_if_fail (CALLS_IS_USSD (ussd));
  task = g_task_new (self, cancellable, callback, user_data);
  gdbo_supplementary_services_call_respond (self->ussd, response, cancellable,
                                            ussd_respond_cb, task);
}

static char *
calls_ofono_ussd_respond_finish (CallsUssd    *ussd,
                                 GAsyncResult *result,
                                 GError      **error)
{
  g_return_val_if_fail (CALLS_IS_USSD (ussd), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);


  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
calls_ofono_ussd_cancel_async (CallsUssd          *ussd,
                               GCancellable       *cancellable,
                               GAsyncReadyCallback callback,
                               gpointer            user_data)
{
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (ussd);
  GTask *task;

  g_return_if_fail (CALLS_IS_USSD (ussd));

  task = g_task_new (self, cancellable, callback, user_data);
  gdbo_supplementary_services_call_cancel (self->ussd, cancellable,
                                           ussd_cancel_cb, task);
}

static gboolean
calls_ofono_ussd_cancel_finish (CallsUssd    *ussd,
                                GAsyncResult *result,
                                GError      **error)
{
  g_return_val_if_fail (CALLS_IS_USSD (ussd), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}


static void
dial_cb (GDBOVoiceCallManager *voice,
         GAsyncResult         *res,
         CallsOfonoOrigin     *self)
{
  gboolean ok;

  g_autoptr (GError) error = NULL;

  ok = gdbo_voice_call_manager_call_dial_finish (voice, NULL, res, &error);
  if (!ok) {
    g_warning ("Error dialing number on modem `%s': %s",
               self->name, error->message);
    CALLS_ERROR (self, error);
    return;
  }

  /* We will add the call through the call-added signal */
}


static void
dial (CallsOrigin *origin, const gchar *number)
{
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (origin);

  g_return_if_fail (self->voice != NULL);

  gdbo_voice_call_manager_call_dial (self->voice,
                                     number,
                                     "default" /* default caller id settings */,
                                     NULL,
                                     (GAsyncReadyCallback) dial_cb,
                                     self);
}


static gboolean
supports_protocol (CallsOrigin *origin,
                   const char  *protocol)
{
  g_assert (protocol);
  g_assert (CALLS_IS_OFONO_ORIGIN (origin));

  return g_strcmp0 (protocol, "tel") == 0;
}

CallsOfonoOrigin *
calls_ofono_origin_new (GDBOModem *modem)
{
  g_return_val_if_fail (GDBO_IS_MODEM (modem), NULL);

  return g_object_new (CALLS_TYPE_OFONO_ORIGIN,
                       "modem", modem,
                       NULL);
}

gboolean
calls_ofono_origin_matches (CallsOfonoOrigin *self,
                            const char       *path)
{
  g_return_val_if_fail (CALLS_IS_OFONO_ORIGIN (self), FALSE);
  g_return_val_if_fail (path, FALSE);
  g_return_val_if_fail (self->modem, FALSE);

  return g_strcmp0 (g_dbus_proxy_get_object_path (G_DBUS_PROXY (self->modem)), path) == 0;
}

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (object);

  switch (property_id) {
  case PROP_ID:
    /* we're using a hardcoded value, so let's ignore it */
    break;

  case PROP_MODEM:
    g_set_object (&self->modem, GDBO_MODEM (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
get_property (GObject    *object,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (object);

  switch (property_id) {
  case PROP_ID:
    g_value_set_string (value, "ofono");
    break;

  case PROP_NAME:
    g_value_set_string (value, self->name);
    break;

  case PROP_CALLS:
    g_value_set_pointer (value, g_hash_table_get_values (self->calls));
    break;

  case PROP_COUNTRY_CODE:
    g_value_set_string (value, NULL);
    break;

  case PROP_EMERGENCY_NUMBERS:
    g_value_set_boxed (value, NULL);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
remove_call (CallsOfonoOrigin *self,
             CallsOfonoCall   *call,
             const gchar      *reason)
{
  const gchar *path = calls_ofono_call_get_object_path (call);

  g_signal_emit_by_name (CALLS_ORIGIN (self), "call-removed",
                         CALLS_CALL (call), reason);
  g_hash_table_remove (self->calls, path);
}


struct CallsRemoveCallsData {
  CallsOrigin *origin;
  const gchar *reason;
};

static gboolean
remove_calls_cb (const gchar                 *path,
                 CallsOfonoCall              *call,
                 struct CallsRemoveCallsData *data)
{
  g_signal_emit_by_name (data->origin, "call-removed",
                         CALLS_CALL (call), data->reason);
  return TRUE;
}

static void
remove_calls (CallsOfonoOrigin *self, const gchar *reason)
{
  struct CallsRemoveCallsData data = { CALLS_ORIGIN (self), reason };

  g_hash_table_foreach_remove (self->calls,
                               (GHRFunc) remove_calls_cb,
                               &data);
}


struct CallsVoiceCallProxyNewData {
  CallsOfonoOrigin *self;
  GVariant         *properties;
};


static void
send_tones_cb (GDBOVoiceCallManager *voice,
               GAsyncResult         *res,
               CallsOfonoOrigin     *self)
{
  gboolean ok;
  GError *error = NULL;

  /* Deal with old tones */
  ok = gdbo_voice_call_manager_call_send_tones_finish
         (voice, res, &error);
  if (!ok) {
    g_warning ("Error sending DTMF tones to network on modem `%s': %s",
               self->name, error->message);
    CALLS_EMIT_MESSAGE (self, error->message, GTK_MESSAGE_WARNING);
  }

  /* Possibly send new tones */
  if (self->tone_queue) {
    g_debug ("Sending queued DTMF tones `%s'", self->tone_queue->str);

    gdbo_voice_call_manager_call_send_tones (voice,
                                             self->tone_queue->str,
                                             NULL,
                                             (GAsyncReadyCallback) send_tones_cb,
                                             self);

    g_string_free (self->tone_queue, TRUE);
    self->tone_queue = NULL;
  } else {
    self->sending_tones = FALSE;
  }
}


static void
tone_cb (CallsOfonoOrigin *self,
         gchar             key)
{
  const gchar key_str[2] = { key, '\0' };

  if (self->sending_tones) {
    if (self->tone_queue) {
      g_string_append_c (self->tone_queue, key);
    } else {
      self->tone_queue = g_string_new (key_str);
    }
  } else {
    g_debug ("Sending immediate DTMF tone `%c'", key);

    gdbo_voice_call_manager_call_send_tones (self->voice,
                                             key_str,
                                             NULL,
                                             (GAsyncReadyCallback) send_tones_cb,
                                             self);

    self->sending_tones = TRUE;
  }
}

static void
voice_call_proxy_new_cb (GDBusConnection                   *connection,
                         GAsyncResult                      *res,
                         struct CallsVoiceCallProxyNewData *data)
{
  CallsOfonoOrigin *self = data->self;
  GDBOVoiceCall *voice_call;

  g_autoptr (GError) error = NULL;
  const gchar *path;
  CallsOfonoCall *call;

  voice_call = gdbo_voice_call_proxy_new_finish (res, &error);
  if (!voice_call) {
    g_variant_unref (data->properties);
    g_free (data);
    g_warning ("Error creating oFono VoiceCall proxy: %s",
               error->message);
    CALLS_ERROR (self, error);
    return;
  }

  call = calls_ofono_call_new (voice_call, data->properties);
  g_signal_connect_swapped (call, "tone",
                            G_CALLBACK (tone_cb), self);

  path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (voice_call));
  g_hash_table_insert (self->calls, g_strdup (path), call);

  g_signal_emit_by_name (CALLS_ORIGIN (self), "call-added",
                         CALLS_CALL (call));

  g_debug ("Call `%s' added", path);
}


static void
call_added_cb (GDBOVoiceCallManager *voice,
               const gchar          *path,
               GVariant             *properties,
               CallsOfonoOrigin     *self)
{
  struct CallsVoiceCallProxyNewData *data;

  g_debug ("Adding call `%s'", path);

  if (g_hash_table_lookup (self->calls, path)) {
    g_warning ("Call `%s' already exists", path);
    return;
  }

  data = g_new0 (struct CallsVoiceCallProxyNewData, 1);
  data->self = self;
  data->properties = properties;
  g_variant_ref (properties);

  gdbo_voice_call_proxy_new (self->connection,
                             G_DBUS_PROXY_FLAGS_NONE,
                             g_dbus_proxy_get_name (G_DBUS_PROXY (voice)),
                             path,
                             NULL,
                             (GAsyncReadyCallback) voice_call_proxy_new_cb,
                             data);

  g_debug ("Call `%s' addition in progress", path);
}


static void
call_removed_cb (GDBOVoiceCallManager *voice,
                 const gchar          *path,
                 CallsOfonoOrigin     *self)
{
  CallsOfonoCall *ofono_call;
  GString *reason;
  const gchar *ofono_reason;

  g_debug ("Removing call `%s'", path);

  ofono_call = g_hash_table_lookup (self->calls, path);
  if (!ofono_call) {
    g_warning ("Could not find removed call `%s'", path);
    return;
  }

  reason = g_string_new ("Call removed");

  ofono_reason = calls_ofono_call_get_disconnect_reason (ofono_call);
  if (ofono_reason) {
    /* The oFono reason is either "local", "remote" or "network".
     * We just capitalise that to create a nice reason string.
     */
    g_string_assign (reason, ofono_reason);
    reason->str[0] = g_ascii_toupper (reason->str[0]);
    g_string_append (reason, " disconnection");
  }

  remove_call (self, ofono_call, reason->str);

  g_string_free (reason, TRUE);

  g_debug ("Removed call `%s'", path);
}

static void
get_calls_cb (GDBOVoiceCallManager *voice,
              GAsyncResult         *res,
              CallsOfonoOrigin     *self)
{
  gboolean ok;
  GVariant *calls_with_properties = NULL;

  g_autoptr (GError) error = NULL;
  GVariantIter *iter = NULL;
  const gchar *path;
  GVariant *properties;

  ok = gdbo_voice_call_manager_call_get_calls_finish (voice,
                                                      &calls_with_properties,
                                                      res,
                                                      &error);
  if (!ok) {
    g_warning ("Error getting calls from oFono"
               " VoiceCallManager `%s': %s",
               self->name, error->message);
    CALLS_ERROR (self, error);
    return;
  }

  {
    char *text = g_variant_print (calls_with_properties, TRUE);
    g_debug ("Received calls from oFono"
             " VoiceCallManager `%s': %s",
             self->name, text);
    g_free (text);
  }

  g_variant_get (calls_with_properties, "a(oa{sv})", &iter);
  while (g_variant_iter_loop (iter, "(&o@a{sv})",
                              &path, &properties))
  {
    g_debug ("Got call object path `%s'", path);
    call_added_cb (voice, path, properties, self);
  }
  g_variant_iter_free (iter);

  g_variant_unref (calls_with_properties);
}

static void
voice_new_cb (GDBusConnection  *connection,
              GAsyncResult     *res,
              CallsOfonoOrigin *self)
{
  g_autoptr (GError) error = NULL;

  self->voice = gdbo_voice_call_manager_proxy_new_finish (res, &error);
  if (!self->voice) {
    g_warning ("Error creating oFono"
               " VoiceCallManager `%s' proxy: %s",
               self->name, error->message);
    CALLS_ERROR (self, error);
    return;
  }

  g_signal_connect (self->voice, "call-added",
                    G_CALLBACK (call_added_cb), self);
  g_signal_connect (self->voice, "call-removed",
                    G_CALLBACK (call_removed_cb), self);

  gdbo_voice_call_manager_call_get_calls
    (self->voice,
    NULL,
    (GAsyncReadyCallback) get_calls_cb,
    self);
}


static void
ussd_new_cb (GDBusConnection  *connection,
             GAsyncResult     *res,
             CallsOfonoOrigin *self)
{
  g_autoptr (GError) error = NULL;

  self->ussd = gdbo_supplementary_services_proxy_new_finish (res, &error);
  if (!self->ussd) {
    g_warning ("Error creating oFono"
               " SupplementaryServices `%s' proxy: %s",
               self->name, error->message);
    CALLS_ERROR (self, error);
  }
}


static void
constructed (GObject *object)
{
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (object);
  GDBusProxy *modem_proxy;
  gchar *name;

  g_return_if_fail (self->modem != NULL);

  modem_proxy = G_DBUS_PROXY (self->modem);

  self->connection = g_dbus_proxy_get_connection (modem_proxy);
  g_object_ref (self->connection);

  name = g_object_get_data (G_OBJECT (self->modem),
                            "calls-modem-name");
  if (name)
    self->name = g_strdup (name);

  gdbo_voice_call_manager_proxy_new (self->connection,
                                     G_DBUS_PROXY_FLAGS_NONE,
                                     g_dbus_proxy_get_name (modem_proxy),
                                     g_dbus_proxy_get_object_path (modem_proxy),
                                     NULL,
                                     (GAsyncReadyCallback) voice_new_cb,
                                     self);

  G_OBJECT_CLASS (calls_ofono_origin_parent_class)->constructed (object);

  gdbo_supplementary_services_proxy_new (self->connection,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         g_dbus_proxy_get_name (modem_proxy),
                                         g_dbus_proxy_get_object_path (modem_proxy),
                                         NULL,
                                         (GAsyncReadyCallback) ussd_new_cb,
                                         self);
}


static void
dispose (GObject *object)
{
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (object);

  remove_calls (self, NULL);
  g_clear_object (&self->modem);
  g_clear_object (&self->connection);

  G_OBJECT_CLASS (calls_ofono_origin_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (object);

  if (self->tone_queue) {
    g_string_free (self->tone_queue, TRUE);
  }
  g_free (self->name);

  G_OBJECT_CLASS (calls_ofono_origin_parent_class)->finalize (object);
}


static void
calls_ofono_origin_class_init (CallsOfonoOriginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  props[PROP_MODEM] =
    g_param_spec_object ("modem",
                         "Modem",
                         "A GDBO proxy object for the underlying modem object",
                         GDBO_TYPE_MODEM,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_MODEM, props[PROP_MODEM]);

#define IMPLEMENTS(ID, NAME) \
  g_object_class_override_property (object_class, ID, NAME);    \
  props[ID] = g_object_class_find_property(object_class, NAME);

  IMPLEMENTS (PROP_NAME, "id");
  IMPLEMENTS (PROP_NAME, "name");
  IMPLEMENTS (PROP_CALLS, "calls");
  IMPLEMENTS (PROP_COUNTRY_CODE, "country-code");
  IMPLEMENTS (PROP_EMERGENCY_NUMBERS, "emergency-numbers");

#undef IMPLEMENTS

}


static void
calls_ofono_origin_message_source_interface_init (CallsOriginInterface *iface)
{
}


static void
calls_ofono_origin_ussd_interface_init (CallsUssdInterface *iface)
{
  iface->get_state = calls_ofono_ussd_get_state;
  iface->initiate_async = calls_ofono_ussd_initiate_async;
  iface->initiate_finish = calls_ofono_ussd_initiate_finish;
  iface->respond_async = calls_ofono_ussd_respond_async;
  iface->respond_finish = calls_ofono_ussd_respond_finish;
  iface->cancel_async = calls_ofono_ussd_cancel_async;
  iface->cancel_finish = calls_ofono_ussd_cancel_finish;
}


static void
calls_ofono_origin_origin_interface_init (CallsOriginInterface *iface)
{
  iface->dial = dial;
  iface->supports_protocol = supports_protocol;
}


static void
calls_ofono_origin_init (CallsOfonoOrigin *self)
{
  self->calls = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, g_object_unref);
}
