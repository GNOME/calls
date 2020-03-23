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
#include "calls-mm-call.h"
#include "calls-message-source.h"

#include <glib/gi18n.h>


struct _CallsMMOrigin
{
  GObject parent_instance;
  MMObject *mm_obj;
  MMModemVoice *voice;
  gchar *name;
  GHashTable *calls;
};

static void calls_mm_origin_message_source_interface_init (CallsOriginInterface *iface);
static void calls_mm_origin_origin_interface_init (CallsOriginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsMMOrigin, calls_mm_origin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_mm_origin_message_source_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_ORIGIN,
                                                calls_mm_origin_origin_interface_init))

enum {
  PROP_0,
  PROP_MODEM,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static const gchar *
get_name (CallsOrigin *origin)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (origin);
  return self->name;
}


static GList *
get_calls (CallsOrigin * origin)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (origin);
  return g_hash_table_get_values (self->calls);
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
constructed (GObject *object)
{
  CallsMMOrigin *self = CALLS_MM_ORIGIN (object);
  MmGdbusModemVoice *gdbus_voice;

  self->name = modem_get_name (mm_object_get_modem (self->mm_obj));

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

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  props[PROP_MODEM] =
    g_param_spec_object ("mm-object",
                         _("Modem Object"),
                         _("A libmm-glib proxy object for the modem"),
                         MM_TYPE_OBJECT,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_mm_origin_message_source_interface_init (CallsOriginInterface *iface)
{
}


static void
calls_mm_origin_origin_interface_init (CallsOriginInterface *iface)
{
  iface->get_name = get_name;
  iface->get_calls = get_calls;
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
