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


struct _CallsDummyCall {
  GObject parent_instance;
};

static void calls_dummy_call_message_source_interface_init (CallsMessageSourceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsDummyCall, calls_dummy_call, CALLS_TYPE_CALL,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_dummy_call_message_source_interface_init))

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
      (3, (GSourceFunc) outbound_timeout_cb, self);
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
  return g_object_new (CALLS_TYPE_DUMMY_CALL,
                       "id", id,
                       "inbound", inbound,
                       "call-type", CALLS_CALL_TYPE_CELLULAR,
                       NULL);
}


static void
constructed (GObject *object)
{
  CallsDummyCall *self = CALLS_DUMMY_CALL (object);

  if (!calls_call_get_inbound (CALLS_CALL (object)))
    g_timeout_add_seconds (1, (GSourceFunc) outbound_timeout_cb, self);

  G_OBJECT_CLASS (calls_dummy_call_parent_class)->constructed (object);
}


static void
calls_dummy_call_class_init (CallsDummyCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsCallClass *call_class = CALLS_CALL_CLASS (klass);

  object_class->constructed = constructed;

  call_class->get_protocol = calls_dummy_call_get_protocol;
  call_class->answer = calls_dummy_call_answer;
  call_class->hang_up = calls_dummy_call_hang_up;
  call_class->send_dtmf_tone = calls_dummy_call_send_dtmf_tone;
}

static void
calls_dummy_call_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}


static void
calls_dummy_call_init (CallsDummyCall *self)
{
}
