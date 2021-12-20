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
#include "calls-ofono-call.h"
#include "calls-message-source.h"

#include <glib/gi18n.h>


struct _CallsOfonoOrigin
{
  GObject parent_instance;
  GDBusConnection *connection;
  GDBOModem *modem;
  gchar *name;
  GDBOVoiceCallManager *voice;
  gboolean sending_tones;
  GString *tone_queue;
  GHashTable *calls;
};

static void calls_ofono_origin_message_source_interface_init (CallsOriginInterface *iface);
static void calls_ofono_origin_origin_interface_init (CallsOriginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsOfonoOrigin, calls_ofono_origin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_ofono_origin_message_source_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_ORIGIN,
                                                calls_ofono_origin_origin_interface_init))

enum {
  PROP_0,
  PROP_NAME,
  PROP_CALLS,
  PROP_MODEM,
  PROP_COUNTRY_CODE,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
dial_cb (GDBOVoiceCallManager  *voice,
         GAsyncResult          *res,
         CallsOfonoOrigin      *self)
{
  gboolean ok;
  g_autoptr (GError) error = NULL;

  ok = gdbo_voice_call_manager_call_dial_finish
    (voice, NULL, res, &error);
  if (!ok)
    {
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

  gdbo_voice_call_manager_call_dial
    (self->voice,
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
  case PROP_MODEM:
    g_set_object
      (&self->modem, GDBO_MODEM (g_value_get_object (value)));
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
  CallsOfonoOrigin *self = CALLS_OFONO_ORIGIN (object);

  switch (property_id) {
  case PROP_NAME:
    g_value_set_string (value, self->name);
    break;

  case PROP_CALLS:
    g_value_set_pointer(value, g_hash_table_get_values (self->calls));
    break;

  case PROP_COUNTRY_CODE:
    g_value_set_string (value, NULL);
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

  g_signal_emit_by_name (CALLS_ORIGIN(self), "call-removed",
                         CALLS_CALL(call), reason);
  g_hash_table_remove (self->calls, path);
}


struct CallsRemoveCallsData
{
  CallsOrigin *origin;
  const gchar *reason;
};

static gboolean
remove_calls_cb (const gchar                 *path,
                 CallsOfonoCall              *call,
                 struct CallsRemoveCallsData *data)
{
  g_signal_emit_by_name (data->origin, "call-removed",
                         CALLS_CALL(call), data->reason);
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


struct CallsVoiceCallProxyNewData
{
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
  if (!ok)
    {
      g_warning ("Error sending DTMF tones to network on modem `%s': %s",
                 self->name, error->message);
      CALLS_EMIT_MESSAGE (self, error->message, GTK_MESSAGE_WARNING);
    }

  /* Possibly send new tones */
  if (self->tone_queue)
    {
      g_debug ("Sending queued DTMF tones `%s'", self->tone_queue->str);

      gdbo_voice_call_manager_call_send_tones
        (voice,
         self->tone_queue->str,
         NULL,
         (GAsyncReadyCallback) send_tones_cb,
         self);

      g_string_free (self->tone_queue, TRUE);
      self->tone_queue = NULL;
    }
  else
    {
      self->sending_tones = FALSE;
    }
}


static void
tone_cb (CallsOfonoOrigin *self,
         gchar             key)
{
  const gchar key_str[2] = { key, '\0' };

  if (self->sending_tones)
    {
      if (self->tone_queue)
        {
          g_string_append_c (self->tone_queue, key);
        }
      else
        {
          self->tone_queue = g_string_new (key_str);
        }
    }
  else
    {
      g_debug ("Sending immediate DTMF tone `%c'", key);

      gdbo_voice_call_manager_call_send_tones
        (self->voice,
         key_str,
         NULL,
         (GAsyncReadyCallback) send_tones_cb,
         self);

      self->sending_tones = TRUE;
    }
}

static void
voice_call_proxy_new_cb (GDBusConnection *connection,
                         GAsyncResult *res,
                         struct CallsVoiceCallProxyNewData *data)
{
  CallsOfonoOrigin *self = data->self;
  GDBOVoiceCall *voice_call;
  g_autoptr (GError) error = NULL;
  const gchar *path;
  CallsOfonoCall *call;

  voice_call = gdbo_voice_call_proxy_new_finish (res, &error);
  if (!voice_call)
    {
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
  g_hash_table_insert (self->calls, g_strdup(path), call);

  g_signal_emit_by_name (CALLS_ORIGIN(self), "call-added",
                         CALLS_CALL(call));

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

  if (g_hash_table_lookup (self->calls, path))
    {
      g_warning ("Call `%s' already exists", path);
      return;
    }

  data = g_new0 (struct CallsVoiceCallProxyNewData, 1);
  data->self = self;
  data->properties = properties;
  g_variant_ref (properties);

  gdbo_voice_call_proxy_new
    (self->connection,
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
  if (!ofono_call)
    {
      g_warning ("Could not find removed call `%s'", path);
      return;
    }

  reason = g_string_new ("Call removed");

  ofono_reason = calls_ofono_call_get_disconnect_reason (ofono_call);
  if (ofono_reason)
    {
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

  ok = gdbo_voice_call_manager_call_get_calls_finish
    (voice, &calls_with_properties, res, &error);
  if (!ok)
    {
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

  self->voice = gdbo_voice_call_manager_proxy_new_finish
    (res, &error);
  if (!self->voice)
    {
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
    {
      self->name = g_strdup (name);
    }

  gdbo_voice_call_manager_proxy_new
    (self->connection,
     G_DBUS_PROXY_FLAGS_NONE,
     g_dbus_proxy_get_name (modem_proxy),
     g_dbus_proxy_get_object_path (modem_proxy),
     NULL,
     (GAsyncReadyCallback)voice_new_cb,
     self);

  G_OBJECT_CLASS (calls_ofono_origin_parent_class)->constructed (object);
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

  if (self->tone_queue)
    {
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

  IMPLEMENTS (PROP_NAME, "name");
  IMPLEMENTS (PROP_CALLS, "calls");
  IMPLEMENTS (PROP_COUNTRY_CODE, "country-code");

#undef IMPLEMENTS

}


static void
calls_ofono_origin_message_source_interface_init (CallsOriginInterface *iface)
{
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
