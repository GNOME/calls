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

#include "calls-dummy-call.h"
#include "calls-message-source.h"
#include "calls-call.h"

#include <glib/gi18n.h>


struct _CallsDummyCall
{
  GObject parent_instance;
  gchar *id;
};

static void calls_dummy_call_message_source_interface_init (CallsMessageSourceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsDummyCall, calls_dummy_call, CALLS_TYPE_CALL,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_dummy_call_message_source_interface_init))

enum {
  PROP_0,
  PROP_ID_CONSTRUCTOR,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

static const char *
calls_dummy_call_get_id (CallsCall *call)
{
  CallsDummyCall *self = CALLS_DUMMY_CALL (call);

  return self->id;
}

static const char*
calls_dummy_call_get_protocol (CallsCall *call)
{
  return "tel";
}

static void
calls_dummy_call_answer (CallsCall *call)
{
  g_return_if_fail (CALLS_IS_DUMMY_CALL (call));
  g_return_if_fail (calls_call_get_state (call) == CALLS_CALL_STATE_INCOMING);

  calls_call_set_state (call, CALLS_CALL_STATE_ACTIVE);
}

static void
calls_dummy_call_hang_up (CallsCall *call)
{
  g_return_if_fail (CALLS_IS_DUMMY_CALL (call));

  calls_call_set_state (call, CALLS_CALL_STATE_DISCONNECTED);
}

static void
calls_dummy_call_send_dtmf_tone (CallsCall *call,
                                 char       key)
{
  g_debug ("Beep! (%c)", key);
}

static gboolean
outbound_timeout_cb (CallsDummyCall *self)
{
  CallsCall *call;

  g_assert (CALLS_IS_DUMMY_CALL (self));

  call = CALLS_CALL (self);

  switch (calls_call_get_state (call)) {
  case CALLS_CALL_STATE_DIALING:
    calls_call_set_state (call, CALLS_CALL_STATE_ALERTING);
    g_timeout_add_seconds
      (3, (GSourceFunc)outbound_timeout_cb, self);
    break;

  case CALLS_CALL_STATE_ALERTING:
    calls_call_set_state (call, CALLS_CALL_STATE_ACTIVE);
    break;

  default:
    break;
  }

  return G_SOURCE_REMOVE;
}


CallsDummyCall *
calls_dummy_call_new (const gchar *id,
                      gboolean     inbound)
{
  g_return_val_if_fail (id != NULL, NULL);

  return g_object_new (CALLS_TYPE_DUMMY_CALL,
                       "id-constructor", id,
                       "inbound", inbound,
                       NULL);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsDummyCall *self = CALLS_DUMMY_CALL (object);

  switch (property_id) {
  case PROP_ID_CONSTRUCTOR:
    self->id = g_value_dup_string (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
constructed (GObject *object)
{
  CallsDummyCall *self = CALLS_DUMMY_CALL (object);

  if (!calls_call_get_inbound (CALLS_CALL (object)))
    g_timeout_add_seconds (1, (GSourceFunc)outbound_timeout_cb, self);

  G_OBJECT_CLASS (calls_dummy_call_parent_class)->constructed (object);
}

static void
finalize (GObject *object)
{
  CallsDummyCall *self = CALLS_DUMMY_CALL (object);

  g_free (self->id);

  G_OBJECT_CLASS (calls_dummy_call_parent_class)->finalize (object);
}


static void
calls_dummy_call_class_init (CallsDummyCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsCallClass *call_class = CALLS_CALL_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->finalize = finalize;

  call_class->get_id = calls_dummy_call_get_id;
  call_class->get_protocol = calls_dummy_call_get_protocol;
  call_class->answer = calls_dummy_call_answer;
  call_class->hang_up = calls_dummy_call_hang_up;
  call_class->send_dtmf_tone = calls_dummy_call_send_dtmf_tone;

  props[PROP_ID_CONSTRUCTOR] =
    g_param_spec_string ("id-constructor",
                         "Id (constructor)",
                         "The dialed id (dummy class constructor)",
                         "+441234567890",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_ID_CONSTRUCTOR, props[PROP_ID_CONSTRUCTOR]);
}

static void
calls_dummy_call_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}


static void
calls_dummy_call_init (CallsDummyCall *self)
{
}
