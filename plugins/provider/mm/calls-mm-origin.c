/*
 * Copyright (C) 2018-2023 Purism SPC
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

#define G_LOG_DOMAIN "CallsMMOrigin"

#include "calls-mm-origin.h"
#include "calls-origin.h"
#include "calls-ussd.h"
#include "calls-mm-call.h"
#include "calls-message-source.h"
#include "calls-util.h"
#include "itu-e212-iso.h"

#include <glib/gi18n.h>


struct _CallsMMOrigin {
  GObject          parent_instance;
  MMObject        *mm_obj;
  MMModemVoice    *voice;
  MMModem3gppUssd *ussd;
  MMModemLocation *location;
  MMLocation3gpp  *location_3gpp;
  MMSim           *sim;
  GCancellable    *cancel;

  /* XXX: These should be used only for pointer comparison,
   * The content should never be used as it might be
   * pointing to a freed location */
  char            *last_ussd_request;
  char            *last_ussd_response;

  gulong           ussd_handle_id;

  char            *id;
  char            *name;
  GHashTable      *calls;
  char            *country_code;
  const char      *network_country_code;
  GStrv            emergency_numbers;
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
  MMModem3gppUssd *ussd = MM_MODEM_3GPP_USSD (object);
  CallsMMOrigin *self = CALLS_MM_ORIGIN (user_data);
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
  g_autoptr (GTask) task = user_data;
  CallsUssd *ussd = CALLS_USSD (object);
  CallsMMOrigin *self = CALLS_MM_ORIGIN (user_data);
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
  g_autoptr (GTask) task = user_data;
  MMModem3gppUssd *ussd = MM_MODEM_3GPP_USSD (object);
  CallsMMOrigin *self;
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
  g_autoptr (GTask) task = user_data;
  MMModem3gppUssd *ussd = MM_MODEM_3GPP_USSD (object);
  CallsMMOrigin *self;
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

  return (CallsUssdState) mm_modem_3gpp_ussd_get_state (self->ussd);
}

static void
calls_mm_ussd_initiate_async (CallsUssd          *ussd,
                              const char         *command,
                              GCancellable       *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer            user_data)
{
  g_autoptr (GTask) task = NULL;
  CallsMMOrigin *self = CALLS_MM_ORIGIN (ussd);
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
  else
    mm_modem_3gpp_ussd_initiate (self->ussd, command, cancellable,
                                 ussd_initiate_cb, g_steal_pointer (&task));
}

static char *
calls_mm_ussd_initiate_finish (CallsUssd    *ussd,
                               GAsyncResult *result,
                               GError      **error)
{
  g_return_val_if_fail (CALLS_IS_USSD (ussd), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);


  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
calls_mm_ussd_respond_async (CallsUssd          *ussd,
                             const char         *response,
                             GCancellable       *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer            user_data)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (ussd);
  GTask *task;

  g_return_if_fail (CALLS_IS_USSD (ussd));

  task = g_task_new (self, cancellable, callback, user_data);
  mm_modem_3gpp_ussd_respond (self->ussd, response, cancellable,
                              ussd_respond_cb, task);
}

static char *
calls_mm_ussd_respond_finish (CallsUssd    *ussd,
                              GAsyncResult *result,
                              GError      **error)
{
  g_return_val_if_fail (CALLS_IS_USSD (ussd), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);


  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
calls_mm_ussd_cancel_async (CallsUssd          *ussd,
                            GCancellable       *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer            user_data)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (ussd);
  GTask *task;

  g_return_if_fail (CALLS_IS_USSD (ussd));

  task = g_task_new (self, cancellable, callback, user_data);
  mm_modem_3gpp_ussd_cancel (self->ussd, cancellable,
                             ussd_cancel_cb, task);
}

static gboolean
calls_mm_ussd_cancel_finish (CallsUssd    *ussd,
                             GAsyncResult *result,
                             GError      **error)
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
  g_autoptr (GError) error = NULL;
  MMCall *call;

  call = mm_modem_voice_create_call_finish (voice, res, &error);
  if (!call) {
    g_warning ("Error dialing number on ModemManager modem `%s': %s",
               self->name, error->message);
    CALLS_ERROR (self, error);
  }
}


static void
dial (CallsOrigin *origin,
      const char  *number)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (origin);
  g_autoptr (MMCallProperties) call_props = NULL;

  g_assert (self->voice != NULL);

  call_props = mm_call_properties_new ();
  mm_call_properties_set_number (call_props, number);

  mm_modem_voice_create_call (self->voice,
                              call_props,
                              NULL,
                              (GAsyncReadyCallback) dial_cb,
                              self);
}


static gboolean
supports_protocol (CallsOrigin *origin,
                   const char  *protocol)
{
  g_assert (protocol);
  g_assert (CALLS_IS_MM_ORIGIN (origin));

  return g_strcmp0 (protocol, "tel") == 0;
}


static const char *
get_country_code (CallsOrigin *origin)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (origin);

  g_assert (CALLS_IS_MM_ORIGIN (origin));

  return self->country_code;
}


static const char *
get_network_country_code (CallsOrigin *origin)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (origin);

  g_assert (CALLS_IS_MM_ORIGIN (origin));

  return self->network_country_code;
}


static void
remove_calls (CallsMMOrigin *self, const char *reason)
{
  GList *paths;
  gpointer call;

  paths = g_hash_table_get_keys (self->calls);

  for (GList *node = paths; node != NULL; node = node->next) {
    g_hash_table_steal_extended (self->calls, node->data, NULL, &call);
    g_signal_emit_by_name (self, "call-removed",
                           CALLS_CALL (call), reason);
    g_object_unref (call);
  }

  g_list_free_full (paths, g_free);
}


struct CallsMMOriginDeleteCallData {
  CallsMMOrigin *self;
  char          *path;
};


static void
delete_call_cb (MMModemVoice                       *voice,
                GAsyncResult                       *res,
                struct CallsMMOriginDeleteCallData *data)
{
  g_autoptr (GError) error = NULL;
  gboolean ok;

  ok = mm_modem_voice_delete_call_finish (voice, res, &error);
  if (!ok) {
    g_warning ("Error deleting call `%s' on MMModemVoice `%s': %s",
               data->path, data->self->name, error->message);
    CALLS_ERROR (data->self, error);
  }

  g_free (data->path);
  g_object_unref (data->self);
  g_free (data);
}


static void
delete_call (CallsMMOrigin *self,
             CallsMMCall   *call)
{
  const char *path;
  struct CallsMMOriginDeleteCallData *data;

  path = calls_mm_call_get_object_path (call);

  data = g_new0 (struct CallsMMOriginDeleteCallData, 1);
  data->self = g_object_ref (self);
  data->path = g_strdup (path);

  mm_modem_voice_delete_call (self->voice,
                              path,
                              NULL,
                              (GAsyncReadyCallback) delete_call_cb,
                              data);
}

static void
call_state_changed_cb (CallsCall     *call,
                       GParamSpec    *pspec,
                       CallsMMOrigin *self)
{
  g_assert (CALLS_IS_MM_ORIGIN (self));
  g_assert (CALLS_IS_MM_CALL (call));

  if (calls_call_get_state (call) != CALLS_CALL_STATE_DISCONNECTED)
    return;

  delete_call (self, CALLS_MM_CALL (call));
}


static void
add_call (CallsMMOrigin *self,
          MMCall        *mm_call)
{
  CallsMMCall *call;
  char *path;

  call = calls_mm_call_new (mm_call);

  g_signal_connect (call, "notify::state",
                    G_CALLBACK (call_state_changed_cb),
                    self);

  path = mm_call_dup_path (mm_call);
  g_hash_table_insert (self->calls, path, call);

  g_signal_emit_by_name (CALLS_ORIGIN (self), "call-added",
                         CALLS_CALL (call));

  if (mm_call_get_state (mm_call) == MM_CALL_STATE_TERMINATED) {
    /* Delete any remnant disconnected call */
    delete_call (self, call);
  }

  g_debug ("Call `%s' added", path);

  /* FIXME: Hang up the call, since accepting a secondary call does not currently work.
   * CallsMMCall[28822]: WARNING: Error accepting ModemManager call to `+4916XXXXXXXX': GDBus.Error:org.freedesktop.ModemManager1.Error.Core.Failed: This call was not ringing, cannot accept
   */
  if (g_hash_table_size (self->calls) > 1)
    calls_call_hang_up (CALLS_CALL (call));
}


struct CallsMMOriginCallAddedData {
  CallsMMOrigin *self;
  char          *path;
};


static void
call_added_list_calls_cb (MMModemVoice                      *voice,
                          GAsyncResult                      *res,
                          struct CallsMMOriginCallAddedData *data)
{
  g_autoptr (GError) error = NULL;
  GList *calls;

  calls = mm_modem_voice_list_calls_finish (voice, res, &error);
  if (!calls) {
    if (error) {
      g_warning ("Error listing calls on MMModemVoice `%s'"
                 " after call-added signal: %s",
                 data->self->name, error->message);
      CALLS_ERROR (data->self, error);
    } else {
      g_warning ("No calls on MMModemVoice `%s'"
                 " after call-added signal",
                 data->self->name);
    }
  } else {
    GList *node;
    MMCall *call;
    gboolean found = FALSE;

    for (node = calls; node; node = node->next) {
      call = MM_CALL (node->data);

      if (g_strcmp0 (mm_call_get_path (call), data->path) == 0) {
        add_call (data->self, call);
        found = TRUE;
      }
    }

    if (!found) {
      g_warning ("Could not find new call `%s' in call list"
                 " on MMModemVoice `%s' after call-added signal",
                 data->path, data->self->name);
    }

    g_list_free_full (calls, g_object_unref);
  }

  g_free (data->path);
  g_object_unref (data->self);
  g_free (data);
}


static void
call_added_cb (MMModemVoice  *voice,
               char          *path,
               CallsMMOrigin *self)
{
  struct CallsMMOriginCallAddedData *data;

  if (g_hash_table_contains (self->calls, path)) {
    g_warning ("Received call-added signal for"
               " existing call object path `%s'", path);
    return;
  }

  data = g_new0 (struct CallsMMOriginCallAddedData, 1);
  data->self = g_object_ref (self);
  data->path = g_strdup (path);

  mm_modem_voice_list_calls (voice,
                             NULL,
                             (GAsyncReadyCallback) call_added_list_calls_cb,
                             data);
}


static void
call_deleted_cb (MMModemVoice *voice,
                 const char   *path,
                 gpointer      user_data)
{
  CallsMMOrigin *self;
  gpointer call;
  gpointer key;
  const char *mm_reason;

  g_assert (CALLS_IS_MM_ORIGIN (user_data));
  self = CALLS_MM_ORIGIN (user_data);

  g_debug ("Removing call `%s'", path);

  g_hash_table_steal_extended (self->calls, path, &key, &call);

  g_free (key);

  if (!call) {
    g_warning ("Could not find removed call `%s'", path);
    return;
  }

  mm_reason = calls_mm_call_get_disconnect_reason (CALLS_MM_CALL (call));
  g_signal_emit_by_name (self,
                         "call-removed",
                         call,
                         mm_reason ?: "Call removed");

  g_object_unref (call);

  g_debug ("Removed call `%s'", path);
}


static void
list_calls_cb (MMModemVoice *voice,
               GAsyncResult *res,
               gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (CallsMMOrigin) self = NULL;
  GList *calls;

  g_assert (CALLS_IS_MM_ORIGIN (user_data));
  self = CALLS_MM_ORIGIN (user_data);

  calls = mm_modem_voice_list_calls_finish (voice, res, &error);
  if (!calls) {
    if (error) {
      g_warning ("Error listing calls on MMModemVoice `%s': %s",
                 self->name, error->message);
      CALLS_ERROR (self, error);
    }
    return;
  }

  for (GList *node = calls; node; node = node->next) {
    add_call (self, MM_CALL (node->data));
  }

  g_list_free_full (calls, g_object_unref);
}


static void
on_modem_location_get_3gpp_finish (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  MMModemLocation *location = MM_MODEM_LOCATION (source_object);
  MMLocation3gpp *location_3gpp;
  CallsMMOrigin *self;
  g_autoptr (GError) err = NULL;
  g_autofree char *str = NULL;
  guint mcc;

  location_3gpp = mm_modem_location_get_3gpp_finish (location, res, &err);
  if (!location_3gpp) {
    if (err)
      g_warning ("Failed to get 3gpp location service: %s", err->message);
    return;
  }

  self = CALLS_MM_ORIGIN (user_data);
  g_assert (CALLS_IS_MM_ORIGIN (self));
  self->location_3gpp = location_3gpp;
  mcc = mm_location_3gpp_get_mobile_country_code (self->location_3gpp);
  if (!mcc) {
    g_warning ("Failed to get country code for %s", mm_object_get_path (self->mm_obj));
    return;
  }
  str = g_strdup_printf ("%u", mcc);
  self->network_country_code = get_country_iso_for_mcc (str);
  g_debug ("Got network country code %u (%s) for %s",
           mcc,
           self->network_country_code,
           mm_object_get_path (self->mm_obj));
  /* Trigger update of known emergency numbers */
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_EMERGENCY_NUMBERS]);
}


static void
on_modem_location_enabled (CallsMMOrigin   *self,
                           GParamSpec      *pspec,
                           MMModemLocation *location)
{
  if (!(mm_modem_location_get_enabled (location) & MM_MODEM_LOCATION_SOURCE_3GPP_LAC_CI)) {
    g_debug ("Modem '%s' has 3gpp location disabled", mm_object_get_path (self->mm_obj));
    /* We keep the current network country code cached */
    return;
  }

  mm_modem_location_get_3gpp (self->location,
                              self->cancel,
                              on_modem_location_get_3gpp_finish,
                              self);
}


static void
set_mm_object (CallsMMOrigin *self, MMObject *mm_obj)
{
  g_autoptr (MMModemLocation) location = NULL;

  g_set_object (&self->mm_obj, mm_obj);

  location = mm_object_get_modem_location (mm_obj);
  if (location == NULL) {
    g_debug ("Modem '%s' has no location capabilities", mm_object_get_path (mm_obj));
    return;
  }

  g_debug ("Modem '%s' has location capabilities", mm_object_get_path (mm_obj));
  self->location = g_steal_pointer (&location);

  /* Wait for location gathering to get enabled */
  g_signal_connect_object (self->location,
                           "notify::enabled",
                           G_CALLBACK (on_modem_location_enabled),
                           self,
                           G_CONNECT_SWAPPED);
  on_modem_location_enabled (self, NULL, self->location);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);

  switch (property_id) {
  case PROP_ID:
    self->id = g_value_dup_string (value);
    break;

  case PROP_MODEM:
    set_mm_object (self, g_value_get_object (value));
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
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);

  switch (property_id) {
  case PROP_ID:
    g_value_set_string (value, self->id);
    break;

  case PROP_NAME:
    g_value_set_string (value, self->name);
    break;

  case PROP_CALLS:
    g_value_set_pointer (value, g_hash_table_get_values (self->calls));
    break;

  case PROP_COUNTRY_CODE:
    g_value_set_string (value, self->country_code);
    break;

  case PROP_EMERGENCY_NUMBERS:
    g_value_set_boxed (value, self->emergency_numbers);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static char *
modem_get_name (MMModem *modem)
{
  char *name = NULL;
  const char * const *numbers = NULL;

  numbers = mm_modem_get_own_numbers (modem);
  if (numbers && g_strv_length ((char **) numbers) > 0) {
    name = g_strdup (numbers[0]);
    return name;
  }

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
      (value = g_variant_lookup_value (properties, "NetworkRequest", NULL))) {
    response = mm_modem_3gpp_ussd_get_network_request (self->ussd);

    if (!STR_IS_NULL_OR_EMPTY (response) && response != self->last_ussd_request)
      g_signal_emit_by_name (self, "ussd-added", response);

    if (!STR_IS_NULL_OR_EMPTY (response))
      self->last_ussd_request = (char *) response;
    g_clear_pointer (&value, g_variant_unref);
  }

  if (state != CALLS_USSD_STATE_USER_RESPONSE &&
      (value = g_variant_lookup_value (properties, "NetworkNotification", NULL))) {
    response = mm_modem_3gpp_ussd_get_network_notification (self->ussd);

    if (!STR_IS_NULL_OR_EMPTY (response) && response != self->last_ussd_response)
      g_signal_emit_by_name (self, "ussd-added", response);

    if (!STR_IS_NULL_OR_EMPTY (response))
      self->last_ussd_response = (char *) response;
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
get_sim_ready_cb (MMModem      *modem,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  g_autoptr (GError) err = NULL;
  g_autoptr (CallsMMOrigin) self = NULL;
  const char *code;

  g_assert (CALLS_IS_MM_ORIGIN (user_data));
  self = CALLS_MM_ORIGIN (user_data);

  self->sim = mm_modem_get_sim_finish (modem, res, &err);
  if (!self->sim) {
    g_warning ("Couldn't get sim: %s", err->message);
    return;
  }

  code = get_country_iso_for_mcc (mm_sim_get_imsi (self->sim));
  if (code && g_strcmp0 (self->country_code, code)) {
    g_debug ("Setting the country code to `%s'", code);

    self->country_code = g_strdup (code);
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_COUNTRY_CODE]);
  }

  g_strfreev (self->emergency_numbers);
  self->emergency_numbers = mm_sim_dup_emergency_numbers (self->sim);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_EMERGENCY_NUMBERS]);
}


static void
call_waiting_setup_cb (GObject      *obj,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (CallsMMOrigin) self = NULL;
  MMModemVoice *voice = MM_MODEM_VOICE (obj);

  g_assert (CALLS_IS_MM_ORIGIN (user_data));
  self = CALLS_MM_ORIGIN (user_data);
  g_assert (voice == self->voice);

  if (!mm_modem_voice_call_waiting_setup_finish (voice, res, &error)) {
    g_warning ("Could not disable call waiting: %s", error->message);
    return;
  }

  g_info ("Disabled call waiting on modem '%s'", self->name);
}


static void
call_waiting_query_cb (GObject      *obj,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (CallsMMOrigin) self = NULL;
  MMModemVoice *voice = MM_MODEM_VOICE (obj);
  gboolean waiting;

  g_assert (CALLS_IS_MM_ORIGIN (user_data));
  self = CALLS_MM_ORIGIN (user_data);
  g_assert (voice == self->voice);

  if (!mm_modem_voice_call_waiting_query_finish (voice, res, &waiting, &error)) {
    g_warning ("Could not query call waiting status: %s", error->message);
    return;
  }

  g_debug ("Call waiting is %sabled", waiting ? "en" : "dis");
  if (waiting) {
    g_info ("Disabling call waiting: Not implemented");

    mm_modem_voice_call_waiting_setup (voice,
                                       FALSE,
                                       NULL,
                                       call_waiting_setup_cb,
                                       g_steal_pointer (&self));
  }
}


static void
constructed (GObject *object)
{
  g_autoptr (MMModem) modem = NULL;
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);

  G_OBJECT_CLASS (calls_mm_origin_parent_class)->constructed (object);

  modem = mm_object_get_modem (self->mm_obj);
  self->name = modem_get_name (modem);

  mm_modem_get_sim (modem,
                    NULL,
                    (GAsyncReadyCallback) get_sim_ready_cb,
                    g_object_ref (self));

  g_signal_connect_object (self->mm_obj, "notify::modem3gpp-ussd",
                           G_CALLBACK (call_mm_ussd_changed_cb), self,
                           G_CONNECT_SWAPPED);
  call_mm_ussd_changed_cb (self);

  self->voice = mm_object_get_modem_voice (self->mm_obj);
  g_assert (self->voice != NULL);

  mm_modem_voice_call_waiting_query (self->voice,
                                     NULL,
                                     call_waiting_query_cb,
                                     g_object_ref (self));

  g_signal_connect (self->voice, "call-added",
                    G_CALLBACK (call_added_cb), self);
  g_signal_connect (self->voice, "call-deleted",
                    G_CALLBACK (call_deleted_cb), self);

  mm_modem_voice_list_calls (self->voice,
                             NULL,
                             (GAsyncReadyCallback) list_calls_cb,
                             g_object_ref (self));
}


static void
dispose (GObject *object)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);

  g_cancellable_cancel (self->cancel);
  g_clear_object (&self->cancel);

  remove_calls (self, NULL);
  g_clear_object (&self->voice);
  g_clear_object (&self->mm_obj);
  g_clear_object (&self->ussd);
  g_clear_object (&self->sim);
  g_clear_object (&self->location);
  g_clear_object (&self->location_3gpp);
  g_clear_pointer (&self->country_code, g_free);
  g_clear_pointer (&self->id, g_free);

  G_OBJECT_CLASS (calls_mm_origin_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);

  g_hash_table_unref (self->calls);
  g_free (self->name);
  g_strfreev (self->emergency_numbers);

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

  IMPLEMENTS (PROP_ID, "id");
  IMPLEMENTS (PROP_NAME, "name");
  IMPLEMENTS (PROP_CALLS, "calls");
  IMPLEMENTS (PROP_COUNTRY_CODE, "country-code");
  IMPLEMENTS (PROP_EMERGENCY_NUMBERS, "emergency-numbers");

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
  iface->initiate_async = calls_mm_ussd_initiate_async;
  iface->initiate_finish = calls_mm_ussd_initiate_finish;
  iface->respond_async = calls_mm_ussd_respond_async;
  iface->respond_finish = calls_mm_ussd_respond_finish;
  iface->cancel_async = calls_mm_ussd_cancel_async;
  iface->cancel_finish = calls_mm_ussd_cancel_finish;
}


static void
calls_mm_origin_origin_interface_init (CallsOriginInterface *iface)
{
  iface->dial = dial;
  iface->supports_protocol = supports_protocol;
  iface->get_country_code = get_country_code;
  iface->get_network_country_code = get_network_country_code;
}


static void
calls_mm_origin_init (CallsMMOrigin *self)
{
  self->cancel = g_cancellable_new ();
  self->calls = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, g_object_unref);
}

CallsMMOrigin *
calls_mm_origin_new (MMObject   *mm_obj,
                     const char *id)
{
  return g_object_new (CALLS_TYPE_MM_ORIGIN,
                       "mm-object", mm_obj,
                       "id", id,
                       NULL);
}

gboolean
calls_mm_origin_matches (CallsMMOrigin *self,
                         MMObject      *mm_obj)
{
  g_return_val_if_fail (CALLS_IS_MM_ORIGIN (self), FALSE);
  g_return_val_if_fail (MM_IS_OBJECT (mm_obj), FALSE);

  if (self->mm_obj)
    return g_strcmp0 (mm_object_get_path (mm_obj),
                      mm_object_get_path (self->mm_obj)) == 0;

  return FALSE;
}
