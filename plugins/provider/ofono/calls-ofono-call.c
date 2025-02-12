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

#define G_LOG_DOMAIN "CallsOfonoCall"

#include "calls-ofono-call.h"
#include "calls-call.h"
#include "calls-message-source.h"

#include <glib/gi18n.h>


struct _CallsOfonoCall {
  GObject        parent_instance;
  GDBOVoiceCall *voice_call;
  gchar         *disconnect_reason;
};

static void calls_ofono_call_message_source_interface_init (CallsMessageSourceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsOfonoCall, calls_ofono_call, CALLS_TYPE_CALL,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_ofono_call_message_source_interface_init))

enum {
  PROP_0,
  PROP_VOICE_CALL,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

enum {
  SIGNAL_TONE,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];

static const char *
calls_ofono_call_get_protocol (CallsCall *call)
{
  return "tel";
}

struct CallsCallOperationData {
  const gchar    *desc;
  CallsOfonoCall *self;
  gboolean        (*finish_func) (GDBOVoiceCall *, GAsyncResult *, GError **);
};


static void
operation_cb (GDBOVoiceCall                 *voice_call,
              GAsyncResult                  *res,
              struct CallsCallOperationData *data)
{
  g_autoptr (GError) error = NULL;

  gboolean ok;

  ok = data->finish_func (voice_call, res, &error);
  if (!ok) {
    g_warning ("Error %s oFono voice call to `%s': %s",
               data->desc,
               calls_call_get_id (CALLS_CALL (data->self)),
               error->message);
    CALLS_ERROR (data->self, error);
  }

  g_object_unref (data->self);
  g_free (data);
}


static void
calls_ofono_call_answer (CallsCall *call)
{
  CallsOfonoCall *self = CALLS_OFONO_CALL (call);
  struct CallsCallOperationData *data;

  data = g_new0 (struct CallsCallOperationData, 1);
  data->desc = "answering";
  data->self = g_object_ref (self);
  data->finish_func = gdbo_voice_call_call_answer_finish;

  gdbo_voice_call_call_answer (self->voice_call, NULL,
                               (GAsyncReadyCallback) operation_cb,
                               data);
}


static void
calls_ofono_call_hang_up (CallsCall *call)
{
  CallsOfonoCall *self = CALLS_OFONO_CALL (call);
  struct CallsCallOperationData *data;

  data = g_new0 (struct CallsCallOperationData, 1);
  data->desc = "hanging up";
  data->self = g_object_ref (self);
  data->finish_func = gdbo_voice_call_call_hangup_finish;

  gdbo_voice_call_call_hangup (self->voice_call, NULL,
                               (GAsyncReadyCallback) operation_cb,
                               data);
}


static void
calls_ofono_call_send_dtmf_tone (CallsCall *call, gchar key)
{
  CallsOfonoCall *self = CALLS_OFONO_CALL (call);

  if (calls_call_get_state (call) != CALLS_CALL_STATE_ACTIVE) {
    g_warning ("Tone start requested for non-active call to `%s'",
               calls_call_get_id (call));
    return;
  }

  g_signal_emit_by_name (self, "tone", key);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsOfonoCall *self = CALLS_OFONO_CALL (object);

  switch (property_id) {
  case PROP_VOICE_CALL:
    g_set_object
      (&self->voice_call, GDBO_VOICE_CALL (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
property_changed_cb (CallsOfonoCall *self,
                     const gchar    *name,
                     GVariant       *value)
{
  GVariant *str_var;
  gchar *str = NULL;
  CallsCallState state;
  gboolean ok;
  g_autofree char *text = g_variant_print (value, TRUE);

  g_debug ("Property `%s' for oFono call to `%s' changed to: %s",
           name,
           calls_call_get_id (CALLS_CALL (self)),
           text);

  if (g_strcmp0 (name, "State") != 0)
    return;

  g_variant_get (value, "v", &str_var);
  g_variant_get (str_var, "&s", &str);
  g_return_if_fail (str != NULL);

  ok = calls_call_state_parse_nick (&state, str);
  if (ok)
    calls_call_set_state (CALLS_CALL (self), state);
  else
    g_warning ("Could not parse new state `%s'"
               " of oFono call to `%s'",
               str, calls_call_get_id (CALLS_CALL (self)));

  g_variant_unref (str_var);
}


static void
disconnect_reason_cb (CallsOfonoCall *self,
                      const gchar    *reason)
{
  if (reason) {
    g_free (self->disconnect_reason);
    self->disconnect_reason = g_strdup (reason);
  }
}


static void
constructed (GObject *object)
{
  CallsOfonoCall *self = CALLS_OFONO_CALL (object);

  g_return_if_fail (self->voice_call != NULL);

  g_signal_connect_object (self->voice_call, "property-changed",
                           G_CALLBACK (property_changed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->voice_call, "disconnect-reason",
                           G_CALLBACK (disconnect_reason_cb),
                           self, G_CONNECT_SWAPPED);

  G_OBJECT_CLASS (calls_ofono_call_parent_class)->constructed (object);
}


static void
dispose (GObject *object)
{
  CallsOfonoCall *self = CALLS_OFONO_CALL (object);

  g_clear_object (&self->voice_call);

  G_OBJECT_CLASS (calls_ofono_call_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsOfonoCall *self = CALLS_OFONO_CALL (object);

  g_free (self->disconnect_reason);

  G_OBJECT_CLASS (calls_ofono_call_parent_class)->finalize (object);
}


static void
calls_ofono_call_class_init (CallsOfonoCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsCallClass *call_class = CALLS_CALL_CLASS (klass);
  GType tone_arg_types = G_TYPE_CHAR;

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  call_class->get_protocol = calls_ofono_call_get_protocol;
  call_class->answer = calls_ofono_call_answer;
  call_class->hang_up = calls_ofono_call_hang_up;
  call_class->send_dtmf_tone = calls_ofono_call_send_dtmf_tone;

  props[PROP_VOICE_CALL] =
    g_param_spec_object ("voice-call",
                         "Voice call",
                         "A GDBO proxy object for the underlying call object",
                         GDBO_TYPE_VOICE_CALL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_VOICE_CALL, props[PROP_VOICE_CALL]);

  signals[SIGNAL_TONE] =
    g_signal_newv ("tone",
                   G_TYPE_FROM_CLASS (klass),
                   G_SIGNAL_RUN_LAST,
                   NULL, NULL, NULL, NULL,
                   G_TYPE_NONE,
                   1, &tone_arg_types);
}


static void
calls_ofono_call_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}

static void
calls_ofono_call_init (CallsOfonoCall *self)
{
}


CallsOfonoCall *
calls_ofono_call_new (GDBOVoiceCall *voice_call,
                      GVariant      *call_props)
{
  const char *state_str = NULL;
  const char *name = NULL;
  const char *id = NULL;
  CallsCallState state = CALLS_CALL_STATE_UNKNOWN;
  gboolean inbound = FALSE;

  g_return_val_if_fail (GDBO_IS_VOICE_CALL (voice_call), NULL);
  g_return_val_if_fail (call_props != NULL, NULL);

  g_variant_lookup (call_props, "LineIdentification", "s", &id);
  g_variant_lookup (call_props, "Name", "s", &name);

  g_variant_lookup (call_props, "State", "&s", &state_str);
  if (state_str)
    calls_call_state_parse_nick (&state, state_str);

  /* `inbound` is derived from `state` at construction time.
   * If the call was already somehow accepted and thus state=active,
   * then it's not possible to know the correct value for `inbound`. */
  if (state == CALLS_CALL_STATE_INCOMING)
    inbound = TRUE;

  return g_object_new (CALLS_TYPE_OFONO_CALL,
                       "voice-call", voice_call,
                       "id", id,
                       "name", name,
                       "inbound", inbound,
                       "state", state,
                       "call-type", CALLS_CALL_TYPE_CELLULAR,
                       NULL);
}


const gchar *
calls_ofono_call_get_object_path (CallsOfonoCall *call)
{
  return g_dbus_proxy_get_object_path (G_DBUS_PROXY (call->voice_call));
}


const gchar *
calls_ofono_call_get_disconnect_reason (CallsOfonoCall *call)
{
  return call->disconnect_reason;
}
