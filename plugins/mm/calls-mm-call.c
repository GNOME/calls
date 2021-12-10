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

#define G_LOG_DOMAIN "CallsMMCall"

#include "calls-mm-call.h"
#include "calls-call.h"
#include "calls-message-source.h"
#include "util.h"

#include <libmm-glib/libmm-glib.h>
#include <glib/gi18n.h>


struct _CallsMMCall
{
  GObject parent_instance;
  MMCall *mm_call;
  gchar *disconnect_reason;
};

static void calls_mm_call_message_source_interface_init (CallsMessageSourceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsMMCall, calls_mm_call, CALLS_TYPE_CALL,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_mm_call_message_source_interface_init))

enum {
  PROP_0,
  PROP_MM_CALL,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

static void
notify_id_cb (CallsMMCall *self,
              const gchar *id)
{
  calls_call_set_id (CALLS_CALL (self), id);
}


struct CallsMMCallStateReasonMap
{
  MMCallStateReason  value;
  const gchar       *desc;
};

static const struct CallsMMCallStateReasonMap STATE_REASON_MAP[] = {

#define row(ENUMVALUE,DESCRIPTION)              \
  { MM_CALL_STATE_REASON_##ENUMVALUE, DESCRIPTION }

  row (UNKNOWN,              N_("Call disconnected (unknown reason)")),
  row (OUTGOING_STARTED,     N_("Outgoing call started")),
  row (INCOMING_NEW,         N_("New incoming call")),
  row (ACCEPTED,             N_("Call accepted")),
  row (TERMINATED,           N_("Call ended")),
  row (REFUSED_OR_BUSY,      N_("Call disconnected (busy or call refused)")),
  row (ERROR,                N_("Call disconnected (wrong id or network problem)")),
  row (AUDIO_SETUP_FAILED,   N_("Call disconnected (error setting up audio channel)")),
  row (TRANSFERRED,          N_("Call transferred")),
  row (DEFLECTED,            N_("Call deflected")),

#undef row

  { -1, NULL }
};

static void
set_disconnect_reason (CallsMMCall       *self,
                       MMCallStateReason  reason)
{
  const struct CallsMMCallStateReasonMap *map_row;

  if (self->disconnect_reason)
    {
      g_free (self->disconnect_reason);
    }

  for (map_row = STATE_REASON_MAP; map_row->desc; ++map_row)
    {
      if (map_row->value == reason)
        {
          self->disconnect_reason =
            g_strdup (gettext (map_row->desc));
          return;
        }
    }

  self->disconnect_reason =
    g_strdup_printf (_("Call disconnected (unknown reason code %i)"),
                     (int)reason);

  g_warning ("%s", self->disconnect_reason);
}


struct CallsMMCallStateMap
{
  MMCallState     mm;
  CallsCallState  calls;
  const gchar    *name;
};

static const struct CallsMMCallStateMap STATE_MAP[] = {

#define row(MMENUM,CALLSENUM)                                           \
  { MM_CALL_STATE_##MMENUM, CALLS_CALL_STATE_##CALLSENUM, #MMENUM }     \

  row (UNKNOWN,     DIALING),
  row (DIALING,     DIALING),
  row (RINGING_OUT, ALERTING),
  row (RINGING_IN,  INCOMING),
  row (ACTIVE,      ACTIVE),
  row (HELD,        HELD),
  row (WAITING,     INCOMING),
  row (TERMINATED,  DISCONNECTED),

#undef row

  { -1, -1 }
};

static void
state_changed_cb (CallsMMCall       *self,
                  MMCallState        old,
                  MMCallState        mm_new,
                  MMCallStateReason  reason)
{
  const struct CallsMMCallStateMap *map_row;

  if (mm_new == MM_CALL_STATE_TERMINATED)
    {
      set_disconnect_reason (self, reason);
    }

  for (map_row = STATE_MAP; map_row->mm != -1; ++map_row)
    {
      if (map_row->mm == mm_new)
        {
          g_debug ("MM call state changed to `%s'",
                   map_row->name);
          calls_call_set_state (CALLS_CALL (self), map_row->calls);
          return;
        }
    }
}


static const char *
calls_mm_call_get_protocol (CallsCall *self)
{
  return "tel";
}

struct CallsMMOperationData
{
  const gchar *desc;
  CallsMMCall *self;
  gboolean (*finish_func) (MMCall *, GAsyncResult *, GError **);
};

static void
operation_cb (MMCall                      *mm_call,
              GAsyncResult                *res,
              struct CallsMMOperationData *data)
{
  gboolean ok;
  g_autoptr (GError) error = NULL;

  ok = data->finish_func (mm_call, res, &error);
  if (!ok)
    {
      g_warning ("Error %s ModemManager call to `%s': %s",
                 data->desc,
                 calls_call_get_id (CALLS_CALL (data->self)),
                 error->message);
      CALLS_ERROR (data->self, error);
    }

  g_free (data);
}

#define DEFINE_OPERATION(op,name,desc_str)              \
  static void                                           \
  name (CallsCall *call)                                \
  {                                                     \
    CallsMMCall *self = CALLS_MM_CALL (call);           \
    struct CallsMMOperationData *data;                  \
                                                        \
    data = g_new0 (struct CallsMMOperationData, 1);     \
    data->desc = desc_str;                              \
    data->self = self;                                  \
    data->finish_func = mm_call_##op##_finish;          \
                                                        \
    mm_call_##op                                        \
      (self->mm_call,                                   \
       NULL,                                            \
       (GAsyncReadyCallback) operation_cb,              \
       data);                                           \
  }

DEFINE_OPERATION(accept, calls_mm_call_answer, "accepting");
DEFINE_OPERATION(hangup, calls_mm_call_hang_up, "hanging up");
DEFINE_OPERATION(start, calls_mm_call_start_call, "starting outgoing call");


static void
calls_mm_call_send_dtmf_tone (CallsCall *call, gchar key)
{
  CallsMMCall *self = CALLS_MM_CALL (call);
  struct CallsMMOperationData *data;
  char key_str[2] = { key, '\0' };

  data = g_new0 (struct CallsMMOperationData, 1);
  data->desc = "sending DTMF";
  data->self = self;
  data->finish_func = mm_call_send_dtmf_finish;

  mm_call_send_dtmf
    (self->mm_call,
     key_str,
     NULL,
     (GAsyncReadyCallback) operation_cb,
     data);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsMMCall *self = CALLS_MM_CALL (object);

  switch (property_id) {
  case PROP_MM_CALL:
    g_set_object (&self->mm_call,
                  MM_CALL (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
constructed (GObject *object)
{
  CallsMMCall *self = CALLS_MM_CALL (object);
  MmGdbusCall *gdbus_call = MM_GDBUS_CALL (self->mm_call);
  MMCallState state;

  g_signal_connect_swapped (gdbus_call, "notify::number",
                            G_CALLBACK (notify_id_cb), self);
  g_signal_connect_swapped (gdbus_call, "state-changed",
                            G_CALLBACK (state_changed_cb), self);

  notify_id_cb (self, mm_call_get_number (self->mm_call));

  state = mm_call_get_state (self->mm_call);
  state_changed_cb (self,
                    MM_MODEM_STATE_UNKNOWN,
                    state,
                    mm_call_get_state_reason (self->mm_call));

  /* Start outgoing call */
  if (state == MM_CALL_STATE_UNKNOWN
      && mm_call_get_direction (self->mm_call) == MM_CALL_DIRECTION_OUTGOING)
    calls_mm_call_start_call (CALLS_CALL (self));

  G_OBJECT_CLASS (calls_mm_call_parent_class)->constructed (object);
}

static void
dispose (GObject *object)
{
  CallsMMCall *self = CALLS_MM_CALL (object);

  g_clear_object (&self->mm_call);

  G_OBJECT_CLASS (calls_mm_call_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsMMCall *self = CALLS_MM_CALL (object);

  g_free (self->disconnect_reason);

  G_OBJECT_CLASS (calls_mm_call_parent_class)->finalize (object);
}


static void
calls_mm_call_class_init (CallsMMCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsCallClass *call_class = CALLS_CALL_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  call_class->get_protocol = calls_mm_call_get_protocol;
  call_class->answer = calls_mm_call_answer;
  call_class->hang_up = calls_mm_call_hang_up;
  call_class->send_dtmf_tone = calls_mm_call_send_dtmf_tone;

  props[PROP_MM_CALL] = g_param_spec_object ("mm-call",
                                             "MM call",
                                             "A libmm-glib proxy object for the underlying call object",
                                             MM_TYPE_CALL,
                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_MM_CALL, props[PROP_MM_CALL]);
}


static void
calls_mm_call_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}

static void
calls_mm_call_init (CallsMMCall *self)
{
}


CallsMMCall *
calls_mm_call_new (MMCall *mm_call)
{
  gboolean inbound;

  g_return_val_if_fail (MM_IS_CALL (mm_call), NULL);

  inbound = mm_call_get_direction (mm_call) == MM_CALL_DIRECTION_INCOMING;
  return g_object_new (CALLS_TYPE_MM_CALL,
                       "mm-call", mm_call,
                       "inbound", inbound,
                       NULL);
}


const gchar *
calls_mm_call_get_object_path (CallsMMCall *call)
{
  return mm_call_get_path (call->mm_call);
}


const gchar *
calls_mm_call_get_disconnect_reason (CallsMMCall *call)
{
  return call->disconnect_reason;
}
