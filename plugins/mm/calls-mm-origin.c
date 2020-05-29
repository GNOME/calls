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

#include "calls-mm-origin.h"
#include "calls-origin.h"
#include "calls-ussd.h"
#include "calls-mm-call.h"
#include "calls-message-source.h"

#include <glib/gi18n.h>


struct _CallsMMOrigin
{
  GObject parent_instance;
  MMObject *mm_obj;
  MMModemVoice *voice;
  MMModem3gppUssd *ussd;

  /* XXX: These should be used only for pointer comparison,
   * The content should never be used as it might be
   * pointing to a freed location */
  char *last_ussd_request;
  char *last_ussd_response;

  gulong           ussd_handle_id;
  gchar *name;
  GHashTable *calls;
};

static void calls_mm_origin_message_source_interface_init (CallsOriginInterface *iface);
static void calls_mm_origin_origin_interface_init (CallsOriginInterface *iface);
static void calls_mm_origin_ussd_interface_init (CallsUssdInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsMMOrigin, calls_mm_origin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_mm_origin_message_source_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_USSD,
                                                calls_mm_origin_ussd_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_ORIGIN,
                                                calls_mm_origin_origin_interface_init))

enum {
  PROP_0,
  PROP_NAME,
  PROP_CALLS,
  PROP_MODEM,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
ussd_initiate_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  MMModem3gppUssd *ussd = (MMModem3gppUssd *)object;
  g_autoptr(GTask) task = user_data;
  CallsMMOrigin *self = user_data;
  char *response = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);

  g_assert (MM_IS_MODEM_3GPP_USSD (ussd));
  g_assert (CALLS_IS_MM_ORIGIN (self));

  response = mm_modem_3gpp_ussd_initiate_finish (ussd, result, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, response, g_free);
}

static void
ussd_reinitiate_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  CallsUssd *ussd = (CallsUssd *)object;
  g_autoptr(GTask) task = user_data;
  CallsMMOrigin *self = user_data;
  GCancellable *cancellable;
  GError *error = NULL;
  const char *command;

  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);

  g_assert (CALLS_IS_USSD (ussd));
  g_assert (CALLS_IS_MM_ORIGIN (self));

  calls_ussd_cancel_finish (ussd, result, &error);
  cancellable = g_task_get_cancellable (task);
  command = g_task_get_task_data (task);

  if (error)
    g_task_return_error (task, error);
  else
    mm_modem_3gpp_ussd_initiate (self->ussd, command, cancellable,
                                 ussd_initiate_cb, g_steal_pointer (&task));
}

static void
ussd_respond_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  MMModem3gppUssd *ussd = (MMModem3gppUssd *)object;
  CallsMMOrigin *self;
  g_autoptr(GTask) task = user_data;
  char *response = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);

  g_assert (CALLS_IS_MM_ORIGIN (self));
  g_assert (MM_IS_MODEM_3GPP_USSD (ussd));

  response = mm_modem_3gpp_ussd_respond_finish (ussd, result, &error);

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
  MMModem3gppUssd *ussd = (MMModem3gppUssd *)object;
  CallsMMOrigin *self;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  gboolean response;

  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);

  g_assert (CALLS_IS_MM_ORIGIN (self));
  g_assert (MM_IS_MODEM_3GPP_USSD (ussd));

  response = mm_modem_3gpp_ussd_cancel_finish (ussd, result, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, response);
}

static CallsUssdState
calls_mm_ussd_get_state (CallsUssd *ussd)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (ussd);

  if (!self->ussd)
    return CALLS_USSD_STATE_UNKNOWN;

  return mm_modem_3gpp_ussd_get_state (self->ussd);
}

static void
calls_mm_ussd_initiate_async (CallsUssd           *ussd,
                              const char          *command,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (ussd);
  g_autoptr(GTask) task = NULL;
  CallsUssdState state;

  g_return_if_fail (CALLS_IS_USSD (ussd));

  task = g_task_new (self, cancellable, callback, user_data);

  if (!self->ussd)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                               "No USSD interface found");
      return;
    }

  if (!command || !*command)
    {
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
  else
    mm_modem_3gpp_ussd_initiate (self->ussd, command, cancellable,
                                 ussd_initiate_cb, g_steal_pointer (&task));
}

static char *
calls_mm_ussd_initiate_finish (CallsUssd     *ussd,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (CALLS_IS_USSD (ussd), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);


  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
calls_mm_ussd_respond_async (CallsUssd           *ussd,
                             const char          *response,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (ussd);
  GTask *task;

  g_return_if_fail (CALLS_IS_USSD (ussd));

  task = g_task_new (self, cancellable, callback, user_data);
  mm_modem_3gpp_ussd_respond (self->ussd, response, cancellable,
                              ussd_respond_cb, task);
}

static char *
calls_mm_ussd_respond_finish (CallsUssd     *ussd,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (CALLS_IS_USSD (ussd), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);


  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
calls_mm_ussd_cancel_async (CallsUssd           *ussd,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (ussd);
  GTask *task;

  g_return_if_fail (CALLS_IS_USSD (ussd));

  task = g_task_new (self, cancellable, callback, user_data);
  mm_modem_3gpp_ussd_cancel (self->ussd, cancellable,
                             ussd_cancel_cb, task);
}

static gboolean
calls_mm_ussd_cancel_finish (CallsUssd     *ussd,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (CALLS_IS_USSD (ussd), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);


  return g_task_propagate_boolean (G_TASK (result), error);
}


static void
dial_cb (MMModemVoice  *voice,
         GAsyncResult  *res,
         CallsMMOrigin *self)
{
  MMCall *call;
  GError *error = NULL;

  call = mm_modem_voice_create_call_finish (voice, res, &error);
  if (!call)
    {
      g_warning ("Error dialing number on ModemManager modem `%s': %s",
                 self->name, error->message);
      CALLS_ERROR (self, error);
    }
}


static void
dial (CallsOrigin *origin, const gchar *number)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (origin);
  MMCallProperties *props;

  g_assert (self->voice != NULL);

  props = mm_call_properties_new();
  mm_call_properties_set_number (props, number);

  mm_modem_voice_create_call
    (self->voice,
     props,
     NULL,
     (GAsyncReadyCallback) dial_cb,
     self);

  g_object_unref (props);
}


static void
remove_calls (CallsMMOrigin *self, const gchar *reason)
{
  GList *paths, *node;
  gpointer call;

  paths = g_hash_table_get_keys (self->calls);

  for (node = paths; node != NULL; node = node->next)
    {
      g_hash_table_steal_extended (self->calls, node->data, NULL, &call);
      g_signal_emit_by_name (self, "call-removed",
                             CALLS_CALL(call), reason);
      g_object_unref (call);
    }

  g_list_free_full (paths, g_free);
}


struct CallsMMOriginDeleteCallData
{
  CallsMMOrigin *self;
  gchar *path;
};


static void
delete_call_cb (MMModemVoice                       *voice,
                GAsyncResult                       *res,
                struct CallsMMOriginDeleteCallData *data)
{
  gboolean ok;
  GError *error = NULL;

  ok = mm_modem_voice_delete_call_finish (voice, res, &error);
  if (!ok)
    {
      g_warning ("Error deleting call `%s' on MMModemVoice `%s': %s",
                 data->path, data->self->name, error->message);
      CALLS_ERROR (data->self, error);
    }

  g_free (data->path);
  g_free (data);
}


static void
delete_call (CallsMMOrigin  *self,
             CallsMMCall    *call)
{
  const gchar *path;
  struct CallsMMOriginDeleteCallData *data;

  path = calls_mm_call_get_object_path (call);

  data = g_new0 (struct CallsMMOriginDeleteCallData, 1);
  data->self = self;
  data->path = g_strdup (path);

  mm_modem_voice_delete_call
    (self->voice,
     path,
     NULL,
     (GAsyncReadyCallback)delete_call_cb,
     data);
}

static void
call_state_changed_cb (CallsMMOrigin  *self,
                       CallsCallState  new_state,
                       CallsCallState  old_state,
                       CallsCall      *call)
{
  if (new_state != CALLS_CALL_STATE_DISCONNECTED)
    {
      return;
    }

  delete_call (self, CALLS_MM_CALL (call));
}


static void
add_call (CallsMMOrigin *self,
          MMCall        *mm_call)
{
  CallsMMCall *call;
  gchar *path;

  call = calls_mm_call_new (mm_call);

  g_signal_connect_swapped (call, "state-changed",
                            G_CALLBACK (call_state_changed_cb),
                            self);

  path = mm_call_dup_path (mm_call);
  g_hash_table_insert (self->calls, path, call);

  g_signal_emit_by_name (CALLS_ORIGIN(self), "call-added",
                         CALLS_CALL(call));

  if (mm_call_get_state (mm_call) == MM_CALL_STATE_TERMINATED)
    {
      // Delete any remnant disconnected call
      delete_call (self, call);
    }

  g_debug ("Call `%s' added", path);
}


struct CallsMMOriginCallAddedData
{
  CallsMMOrigin *self;
  gchar *path;
};


static void
call_added_list_calls_cb (MMModemVoice                      *voice,
                          GAsyncResult                      *res,
                          struct CallsMMOriginCallAddedData *data)
{
  GList *calls;
  GError *error = NULL;

  calls = mm_modem_voice_list_calls_finish (voice, res, &error);
  if (!calls)
    {
      if (error)
        {
          g_warning ("Error listing calls on MMModemVoice `%s'"
                     " after call-added signal: %s",
                     data->self->name, error->message);
          CALLS_ERROR (data->self, error);
        }
      else
        {
          g_warning ("No calls on MMModemVoice `%s'"
                     " after call-added signal",
                     data->self->name);
        }
    }
  else
    {
      GList *node;
      MMCall *call;
      gboolean found = FALSE;

      for (node = calls; node; node = node->next)
        {
          call = MM_CALL (node->data);

          if (g_strcmp0 (mm_call_get_path (call), data->path) == 0)
            {
              add_call (data->self, call);
              found = TRUE;
            }
        }

      if (!found)
        {
          g_warning ("Could not find new call `%s' in call list"
                     " on MMModemVoice `%s' after call-added signal",
                     data->path, data->self->name);
        }

      g_list_free_full (calls, g_object_unref);
    }

  g_free (data->path);
  g_free (data);
}


static void
call_added_cb (MMModemVoice  *voice,
               gchar         *path,
               CallsMMOrigin *self)
{
  struct CallsMMOriginCallAddedData *data;

  if (g_hash_table_contains (self->calls, path))
    {
      g_warning ("Received call-added signal for"
                 " existing call object path `%s'", path);
      return;
    }

  data = g_new0 (struct CallsMMOriginCallAddedData, 1);
  data->self = self;
  data->path = g_strdup (path);

  mm_modem_voice_list_calls
    (voice,
     NULL,
     (GAsyncReadyCallback) call_added_list_calls_cb,
     data);
}


static void
call_deleted_cb (MMModemVoice  *voice,
                 const gchar   *path,
                 CallsMMOrigin *self)
{
  gpointer call;
  gpointer key;
  GString *reason;
  const gchar *mm_reason;

  g_debug ("Removing call `%s'", path);

  g_hash_table_steal_extended (self->calls, path, &key, &call);

  g_free (key);

  if (!call)
    {
      g_warning ("Could not find removed call `%s'", path);
      return;
    }

  reason = g_string_new ("Call removed");

  mm_reason = calls_mm_call_get_disconnect_reason (CALLS_MM_CALL (call));
  if (mm_reason)
    {
      g_string_assign (reason, mm_reason);
    }

  g_signal_emit_by_name (self, "call-removed", call, reason);

  g_object_unref (call);
  g_string_free (reason, TRUE);

  g_debug ("Removed call `%s'", path);
}


static void
list_calls_cb (MMModemVoice  *voice,
               GAsyncResult  *res,
               CallsMMOrigin *self)
{
  GList *calls, *node;
  GError *error = NULL;

  calls = mm_modem_voice_list_calls_finish (voice, res, &error);
  if (!calls)
    {
      if (error)
        {
          g_warning ("Error listing calls on MMModemVoice `%s': %s",
                     self->name, error->message);
          CALLS_ERROR (self, error);
        }
      return;
    }

  for (node = calls; node; node = node->next)
    {
      add_call (self, MM_CALL (node->data));
    }

  g_list_free_full (calls, g_object_unref);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);

  switch (property_id) {
  case PROP_MODEM:
    g_set_object (&self->mm_obj, g_value_get_object(value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
get_property (GObject      *object,
              guint         property_id,
              GValue       *value,
              GParamSpec   *pspec)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);

  switch (property_id) {
  case PROP_NAME:
    g_value_set_string (value, self->name);
    break;

  case PROP_CALLS:
    g_value_set_pointer(value, g_hash_table_get_values (self->calls));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static gchar *
modem_get_name (MMModem *modem)
{
  gchar *name = NULL;

#define try(prop)                               \
  name = mm_modem_dup_##prop (modem);           \
  if (name) {                                   \
    return name;                                \
  }

  try (model);
  try (manufacturer);
  try (device);
  try (primary_port);
  try (device_identifier);
  try (plugin);

#undef try

  return NULL;
}


static void
ussd_properties_changed_cb (CallsMMOrigin *self,
                            GVariant      *properties)
{
  const char *response;
  GVariant *value;
  CallsUssdState state;

  g_assert (CALLS_IS_MM_ORIGIN (self));

  state = calls_ussd_get_state (CALLS_USSD (self));

  value = g_variant_lookup_value (properties, "State", NULL);
  if (value)
    g_signal_emit_by_name (self, "ussd-state-changed");
  g_clear_pointer (&value, g_variant_unref);

  /* XXX: We check for user state only because the NetworkRequest
   * dbus property change isn't regularly emitted */
  if (state == CALLS_USSD_STATE_USER_RESPONSE ||
      (value = g_variant_lookup_value (properties, "NetworkRequest", NULL)))
    {
      response = mm_modem_3gpp_ussd_get_network_request (self->ussd);

      if (response && *response && response != self->last_ussd_request)
        g_signal_emit_by_name (self, "ussd-added", response);

      if (response && *response)
        self->last_ussd_request = (char *)response;
      g_clear_pointer (&value, g_variant_unref);
    }

  if (state != CALLS_USSD_STATE_USER_RESPONSE &&
      (value = g_variant_lookup_value (properties, "NetworkNotification", NULL)))
    {
      response = mm_modem_3gpp_ussd_get_network_notification (self->ussd);

      if (response && *response && response != self->last_ussd_response)
        g_signal_emit_by_name (self, "ussd-added", response);

      if (response && *response)
        self->last_ussd_response = (char *)response;
      g_clear_pointer (&value, g_variant_unref);
    }
}

static void
call_mm_ussd_changed_cb (CallsMMOrigin *self)
{
  g_assert (CALLS_IS_MM_ORIGIN (self));

  if (self->ussd_handle_id)
    g_signal_handler_disconnect (self, self->ussd_handle_id);

  self->ussd_handle_id = 0;

  g_clear_object (&self->ussd);
  self->ussd = mm_object_get_modem_3gpp_ussd (self->mm_obj);

  /* XXX: We hook to dbus properties changed because the regular signal emission is inconsistent  */
  if (self->ussd)
    self->ussd_handle_id = g_signal_connect_object (self->ussd, "g-properties-changed",
                                                    G_CALLBACK (ussd_properties_changed_cb), self,
                                                    G_CONNECT_SWAPPED);
}

static void
constructed (GObject *object)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);
  MmGdbusModemVoice *gdbus_voice;

  self->name = modem_get_name (mm_object_get_modem (self->mm_obj));

  g_signal_connect_object (self->mm_obj, "notify::modem3gpp-ussd",
                           G_CALLBACK (call_mm_ussd_changed_cb), self,
                           G_CONNECT_SWAPPED);
  call_mm_ussd_changed_cb (self);

  self->voice = mm_object_get_modem_voice (self->mm_obj);
  g_assert (self->voice != NULL);

  gdbus_voice = MM_GDBUS_MODEM_VOICE (self->voice);
  g_signal_connect (gdbus_voice, "call-added",
                    G_CALLBACK (call_added_cb), self);
  g_signal_connect (gdbus_voice, "call-deleted",
                    G_CALLBACK (call_deleted_cb), self);

  mm_modem_voice_list_calls
    (self->voice,
     NULL,
     (GAsyncReadyCallback) list_calls_cb,
     self);
  G_OBJECT_CLASS (calls_mm_origin_parent_class)->constructed (object);
}


static void
dispose (GObject *object)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);

  remove_calls (self, NULL);
  g_clear_object (&self->mm_obj);
  g_clear_object (&self->ussd);

  G_OBJECT_CLASS (calls_mm_origin_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);

  g_hash_table_unref (self->calls);
  g_free (self->name);

  G_OBJECT_CLASS (calls_mm_origin_parent_class)->finalize (object);
}


static void
calls_mm_origin_class_init (CallsMMOriginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  props[PROP_MODEM] =
    g_param_spec_object ("mm-object",
                         "Modem Object",
                         "A libmm-glib proxy object for the modem",
                         MM_TYPE_OBJECT,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_MODEM, props[PROP_MODEM]);

#define IMPLEMENTS(ID, NAME) \
  g_object_class_override_property (object_class, ID, NAME);    \
  props[ID] = g_object_class_find_property(object_class, NAME);

  IMPLEMENTS (PROP_NAME, "name");
  IMPLEMENTS (PROP_CALLS, "calls");

#undef IMPLEMENTS

}


static void
calls_mm_origin_message_source_interface_init (CallsOriginInterface *iface)
{
}


static void
calls_mm_origin_ussd_interface_init (CallsUssdInterface *iface)
{
  iface->get_state = calls_mm_ussd_get_state;
  iface->initiate_async  = calls_mm_ussd_initiate_async;
  iface->initiate_finish = calls_mm_ussd_initiate_finish;
  iface->respond_async   = calls_mm_ussd_respond_async;
  iface->respond_finish  = calls_mm_ussd_respond_finish;
  iface->cancel_async    = calls_mm_ussd_cancel_async;
  iface->cancel_finish   = calls_mm_ussd_cancel_finish;
}


static void
calls_mm_origin_origin_interface_init (CallsOriginInterface *iface)
{
  iface->dial = dial;
}


static void
calls_mm_origin_init (CallsMMOrigin *self)
{
  self->calls = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, g_object_unref);
}

CallsMMOrigin *
calls_mm_origin_new (MMObject *mm_obj)
{
  return g_object_new (CALLS_TYPE_MM_ORIGIN,
                       "mm-object", mm_obj,
                       NULL);
}
