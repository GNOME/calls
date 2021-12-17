/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "CallsDBusManager"

#include "calls-config.h"
#include "calls-emergency-calls-manager.h"
#include "calls-call.h"
#include "calls-call-dbus.h"
#include "calls-dbus-manager.h"
#include "calls-manager.h"
#include "calls-ui-call-data.h"
#include "calls-util.h"

/**
 * SECTION:calls-dbus-manager
 * @short_description: Manages the Call DBus interface
 * @Title: CallsCallDBusManager
 *
 * The #CallsCallManager is responsible for exposing the
 * call objects on the session bus using #GDBusObjectManager.
 *
 * Call objects are exported like /org/gnome/Calls/ as
 * /org/gnome/Calls/Call/1, /org/gnome/Calls/Call/2, … using
 * org.freedesktop.DBus.ObjectManager.
 */

typedef struct _CallsDBusManager {
  GObject                   parent;

  GDBusObjectManagerServer *object_manager;
  CallsEmergencyCallsManager *emergency_calls_manager;

  guint                     iface_num;
  GListStore               *objs;
  char                     *object_path;
} CallsDBusManager;


G_DEFINE_TYPE (CallsDBusManager, calls_dbus_manager, G_TYPE_OBJECT);

static char *
get_obj_path (CallsDBusManager *self, guint num)
{
  return g_strdup_printf ("%s/Call/%u", self->object_path, num);
}


static CallsDBusObjectSkeleton *
find_call (CallsDBusManager *self, CallsUiCallData *call, int *pos)
{
  CallsDBusObjectSkeleton *found = NULL;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (call), NULL);

  for (int i = 0; i >= 0; i++) {
    g_autoptr (CallsDBusObjectSkeleton) item = g_list_model_get_item (G_LIST_MODEL (self->objs), i);

    if (!item)
      break;

    if (call == g_object_get_data (G_OBJECT (item), "call")) {
      *pos = i;
      found = item;
      break;
    }
  }

  return found;
}


static gboolean
on_handle_call_accept (CallsDBusCallsCall    *skeleton,
                       GDBusMethodInvocation *invocation,
                       CallsUiCallData       *call)
{
  g_return_val_if_fail (CALLS_DBUS_IS_CALLS_CALL (skeleton), FALSE);
  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (call), FALSE);

  cui_call_accept (CUI_CALL (call));

  calls_dbus_calls_call_complete_accept (skeleton, invocation);
  return TRUE;
}


static gboolean
on_handle_call_hangup (CallsDBusCallsCall    *skeleton,
                       GDBusMethodInvocation *invocation,
                       CallsUiCallData       *call)
{
  g_return_val_if_fail (CALLS_DBUS_IS_CALLS_CALL (skeleton), FALSE);
  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (call), FALSE);

  cui_call_hang_up (CUI_CALL (call));

  calls_dbus_calls_call_complete_hangup (skeleton, invocation);
  return TRUE;
}

static gboolean
  (avatar_loadable_icon_transform_to_image_path) (GBinding *binding,
                                                  const GValue *from_value,
                                                  GValue *to_value,
                                                  gpointer user_data)
{
  GLoadableIcon *icon = G_LOADABLE_ICON (g_value_get_object (from_value));

  if (icon == NULL) {
    g_value_set_string (to_value, NULL);
    return TRUE;
  }

  if (G_IS_FILE_ICON (icon)) {
    GFile *file = g_file_icon_get_file (G_FILE_ICON (icon));

    g_value_take_string (to_value, g_file_get_path (file));
    return TRUE;
  }

  return FALSE;
}


static gboolean
on_handle_call_send_dtmf (CallsDBusCallsCall    *skeleton,
                          GDBusMethodInvocation *invocation,
                          const char            *dtmf_tone,
                          CallsUiCallData       *call)
{
  CuiCall *cui_call;

  g_return_val_if_fail (CALLS_DBUS_IS_CALLS_CALL (skeleton), FALSE);
  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (call), FALSE);

  cui_call = CUI_CALL (call);
  if (!cui_call_get_can_dtmf (cui_call)) {
    g_dbus_method_invocation_return_error (invocation,
                                           G_IO_ERROR,
                                           G_IO_ERROR_FAILED,
                                           "This call is not DTMF capable");
    return TRUE;
  }

  if (!dtmf_tone || !*dtmf_tone) {
    g_dbus_method_invocation_return_error (invocation,
                                           G_IO_ERROR,
                                           G_IO_ERROR_FAILED,
                                           "Cannot send empty DTMF tone");
    return TRUE;
  }

  if (dtmf_tone[1] != '\0') {
    g_dbus_method_invocation_return_error (invocation,
                                           G_IO_ERROR,
                                           G_IO_ERROR_FAILED,
                                           "Key '%s' must be a single valid tone",
                                           dtmf_tone);
    return TRUE;
  }

  if (!dtmf_tone_key_is_valid (*dtmf_tone)) {
    g_dbus_method_invocation_return_error (invocation,
                                           G_IO_ERROR,
                                           G_IO_ERROR_FAILED,
                                           "The key %s is not a valid DTMF tone",
                                           dtmf_tone);
    return TRUE;
  }

  if (cui_call_get_state (cui_call) != CUI_CALL_STATE_ACTIVE) {
    g_dbus_method_invocation_return_error (invocation,
                                           G_IO_ERROR,
                                           G_IO_ERROR_FAILED,
                                           "Can't send DTMF tone because call is inactive");
    return TRUE;
  }

  cui_call_send_dtmf (cui_call, dtmf_tone);
  calls_dbus_calls_call_complete_send_dtmf (skeleton, invocation);

  return TRUE;
}


static gboolean
on_handle_call_silence (CallsDBusCallsCall    *skeleton,
                        GDBusMethodInvocation *invocation,
                        CallsUiCallData       *call)
{
  g_return_val_if_fail (CALLS_DBUS_IS_CALLS_CALL (skeleton), FALSE);
  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (call), FALSE);

  /* TODO switch to cui_call_silence_ring() once it's available in libcall-ui */
  calls_ui_call_data_silence_ring (call);

  calls_dbus_calls_call_complete_silence (skeleton, invocation);
  return TRUE;
}


static GVariant *
build_hints (CallsUiCallData *call)
{
  GVariantDict dict;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (call), NULL);

  g_variant_dict_init (&dict, NULL);

  g_variant_dict_insert (&dict, "ui-active", "b",
                         calls_ui_call_data_get_ui_active (call));

  return g_variant_dict_end (&dict);
}


static void
on_notify_update_hints (CallsUiCallData    *call,
                        GParamSpec         *unused,
                        CallsDBusCallsCall *iface)
{
  GVariant *hints;

  g_assert (CALLS_IS_UI_CALL_DATA (call));
  g_assert (CALLS_DBUS_IS_CALLS_CALL (iface));

  hints = build_hints (call);

  calls_dbus_calls_call_set_hints (iface, hints);
}


static void
call_added_cb (CallsDBusManager *self, CuiCall *call)
{
  g_autoptr (CallsDBusObjectSkeleton) object = NULL;
  g_autoptr (CallsDBusCallsCall) iface = NULL;
  g_autofree char *path = NULL;

  path = get_obj_path (self, self->iface_num++);
  object = calls_dbus_object_skeleton_new (path);
  iface = calls_dbus_calls_call_skeleton_new ();
  g_dbus_object_skeleton_add_interface (G_DBUS_OBJECT_SKELETON (object),
                                        G_DBUS_INTERFACE_SKELETON (iface));

  /* Keep in sync with call object */
  g_object_set_data_full (G_OBJECT (object), "call", g_object_ref (call), g_object_unref);
  g_object_connect (iface,
                    "object-signal::handle-accept", G_CALLBACK (on_handle_call_accept), call,
                    "object-signal::handle-hangup", G_CALLBACK (on_handle_call_hangup), call,
                    "object-signal::handle-send_dtmf", G_CALLBACK (on_handle_call_send_dtmf), call,
                    "object-signal::handle-silence", G_CALLBACK (on_handle_call_silence), call,
                    NULL);
  g_object_bind_property (call, "state", iface, "state", G_BINDING_SYNC_CREATE);
  g_object_bind_property (call, "inbound", iface, "inbound", G_BINDING_SYNC_CREATE);
  g_object_bind_property (call, "id", iface, "id", G_BINDING_SYNC_CREATE);
  g_object_bind_property (call, "display-name", iface, "display-name", G_BINDING_SYNC_CREATE);
  g_object_bind_property (call, "protocol", iface, "protocol", G_BINDING_SYNC_CREATE);
  g_object_bind_property (call, "encrypted", iface, "encrypted", G_BINDING_SYNC_CREATE);
  g_object_set (iface, "can-dtmf", cui_call_get_can_dtmf (call), NULL);

  g_object_bind_property_full (call, "avatar-icon",
                               iface, "image-path",
                               G_BINDING_SYNC_CREATE,
                               avatar_loadable_icon_transform_to_image_path,
                               NULL, NULL, NULL);

  g_signal_connect (call,
                    "notify::ui-active",
                    G_CALLBACK (on_notify_update_hints),
                    iface);
  on_notify_update_hints (CALLS_UI_CALL_DATA (call), NULL, iface);

  /* Export with properties bound to reduce DBus traffic: */
  g_debug ("Exporting %p at %s", call, path);
  g_dbus_object_manager_server_export (self->object_manager, G_DBUS_OBJECT_SKELETON (object));

  g_list_store_append (self->objs, g_steal_pointer (&object));
}


static void
call_removed_cb (CallsDBusManager *self, CallsUiCallData *call)
{
  const char *path = NULL;
  CallsDBusObjectSkeleton *obj = NULL;
  gint pos = -1;

  g_debug ("Call %p removed", call);

  obj = find_call (self, call, &pos);
  g_return_if_fail (CALLS_DBUS_IS_OBJECT (obj));

  path = g_dbus_object_get_object_path (G_DBUS_OBJECT (obj));
  g_dbus_object_manager_server_unexport (self->object_manager, path);

  g_list_store_remove (self->objs, pos);
}


static void
calls_dbus_manager_constructed (GObject *object)
{
  g_autoptr (GList) calls = NULL;
  GList *c;
  CallsDBusManager *self = CALLS_DBUS_MANAGER (object);

  G_OBJECT_CLASS (calls_dbus_manager_parent_class)->constructed (object);

  self->objs = g_list_store_new (CALLS_DBUS_TYPE_OBJECT_SKELETON);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "ui-call-added",
                            G_CALLBACK (call_added_cb),
                            self);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "ui-call-removed",
                            G_CALLBACK (call_removed_cb),
                            self);
  calls = calls_manager_get_calls (calls_manager_get_default ());
  for (c = calls; c != NULL; c = c->next) {
    call_added_cb (self, c->data);
  }
}


static void
calls_dbus_manager_dispose (GObject *object)
{
  CallsDBusManager *self = CALLS_DBUS_MANAGER (object);

  if (self->objs) {
    for (int i = 0; i >= 0; i++) {
      const char *path;
      g_autoptr (CallsDBusObjectSkeleton) obj = g_list_model_get_item (G_LIST_MODEL (self->objs), i);

      if (!obj)
        break;

      path = g_dbus_object_get_object_path (G_DBUS_OBJECT (obj));
      g_dbus_object_manager_server_unexport (self->object_manager, path);
    }
  }

  if (self->emergency_calls_manager) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->emergency_calls_manager));
    g_clear_object (&self->emergency_calls_manager);
  }

  g_clear_object (&self->objs);
  g_clear_object (&self->object_manager);
  g_clear_pointer (&self->object_path, g_free);

  G_OBJECT_CLASS (calls_dbus_manager_parent_class)->dispose (object);
}


static void
calls_dbus_manager_class_init (CallsDBusManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = calls_dbus_manager_constructed;
  object_class->dispose = calls_dbus_manager_dispose;
}


static void
calls_dbus_manager_init (CallsDBusManager *self)
{
  self->iface_num = 1;
  self->emergency_calls_manager = calls_emergency_calls_manager_new ();
}


CallsDBusManager *
calls_dbus_manager_new (void)
{
  return g_object_new (CALLS_TYPE_DBUS_MANAGER, NULL);
}


gboolean
calls_dbus_manager_register (CallsDBusManager *self,
                             GDBusConnection  *connection,
                             const char       *object_path,
                             GError          **error)
{
  g_autoptr (GError) err = NULL;
  gboolean success;

  g_return_val_if_fail (CALLS_IS_DBUS_MANAGER (self), FALSE);

  self->object_path = g_strdup (object_path);
  g_debug ("Registering at %s", self->object_path);
  self->object_manager = g_dbus_object_manager_server_new (object_path);
  g_dbus_object_manager_server_set_connection (self->object_manager, connection);

  success = g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->emergency_calls_manager),
                                              connection,
                                              object_path,
                                              &err);
  if (success == FALSE)
    g_critical ("Failed to export emergency call interface: %s", err->message);

  return TRUE;
}
