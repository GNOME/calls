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
  gchar *number;
  gboolean inbound;
  CallsCallState state;
};

static void calls_dummy_call_message_source_interface_init (CallsMessageSourceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsDummyCall, calls_dummy_call, CALLS_TYPE_CALL,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_dummy_call_message_source_interface_init))

enum {
  PROP_0,
  PROP_NUMBER_CONSTRUCTOR,
  PROP_INBOUND_CONSTRUCTOR,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

static void
change_state (CallsDummyCall *self,
              CallsCallState  state)
{
  CallsCallState old_state = self->state;

  if (old_state == state)
    {
      return;
    }

  self->state = state;
  g_object_notify (G_OBJECT (self), "state");
  g_signal_emit_by_name (CALLS_CALL (self),
                         "state-changed",
                         state,
                         old_state);
}

static const char *
calls_dummy_call_get_number (CallsCall *call)
{
  CallsDummyCall *self = CALLS_DUMMY_CALL (call);

  return self->number;
}

static CallsCallState
calls_dummy_call_get_state (CallsCall *call)
{
  CallsDummyCall *self = CALLS_DUMMY_CALL (call);

  return self->state;
}

static gboolean
calls_dummy_call_get_inbound (CallsCall *call)
{
  CallsDummyCall *self = CALLS_DUMMY_CALL (call);

  return self->inbound;
}

static const char*
calls_dummy_call_get_protocol (CallsCall *call)
{
  return "tel";
}

static void
calls_dummy_call_answer (CallsCall *call)
{
  CallsDummyCall *self;

  g_return_if_fail (CALLS_IS_DUMMY_CALL (call));
  self = CALLS_DUMMY_CALL (call);

  g_return_if_fail (self->state == CALLS_CALL_STATE_INCOMING);

  change_state (self, CALLS_CALL_STATE_ACTIVE);
}

static void
calls_dummy_call_hang_up (CallsCall *call)
{
  CallsDummyCall *self;

  g_return_if_fail (CALLS_IS_DUMMY_CALL (call));
  self = CALLS_DUMMY_CALL (call);

  change_state (self, CALLS_CALL_STATE_DISCONNECTED);
}

static gboolean
outbound_timeout_cb (CallsDummyCall *self)
{
  switch (self->state)
    {
    case CALLS_CALL_STATE_DIALING:
      change_state (self,
                    CALLS_CALL_STATE_ALERTING);
      g_timeout_add_seconds
        (3, (GSourceFunc)outbound_timeout_cb, self);
      break;

    case CALLS_CALL_STATE_ALERTING:
      change_state (self,
                    CALLS_CALL_STATE_ACTIVE);
      break;

    default:
      break;
    }

  return FALSE;
}


CallsDummyCall *
calls_dummy_call_new (const gchar *number,
                      gboolean     inbound)
{
  g_return_val_if_fail (number != NULL, NULL);

  return g_object_new (CALLS_TYPE_DUMMY_CALL,
                       "number-constructor", number,
                       "inbound-constructor", inbound,
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
  case PROP_NUMBER_CONSTRUCTOR:
    self->number = g_value_dup_string (value);
    break;

  case PROP_INBOUND_CONSTRUCTOR:
    self->inbound = g_value_get_boolean (value);
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

  if (self->inbound)
    {
      self->state = CALLS_CALL_STATE_INCOMING;
    }
  else
    {
      self->state = CALLS_CALL_STATE_DIALING;
      g_timeout_add_seconds (1, (GSourceFunc)outbound_timeout_cb, self);
    }

  G_OBJECT_CLASS (calls_dummy_call_parent_class)->constructed (object);
}

static void
finalize (GObject *object)
{
  CallsDummyCall *self = CALLS_DUMMY_CALL (object);

  g_free (self->number);

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

  call_class->get_number = calls_dummy_call_get_number;
  call_class->get_state = calls_dummy_call_get_state;
  call_class->get_inbound = calls_dummy_call_get_inbound;
  call_class->get_protocol = calls_dummy_call_get_protocol;
  call_class->answer = calls_dummy_call_answer;
  call_class->hang_up = calls_dummy_call_hang_up;

  props[PROP_NUMBER_CONSTRUCTOR] =
    g_param_spec_string ("number-constructor",
                         "Number (constructor)",
                         "The dialed number (dummy class constructor)",
                         "+441234567890",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_NUMBER_CONSTRUCTOR, props[PROP_NUMBER_CONSTRUCTOR]);

  props[PROP_INBOUND_CONSTRUCTOR] =
    g_param_spec_boolean ("inbound-constructor",
                          "Inbound (constructor)",
                          "Whether the calls is inbound (dummy class constructor)",
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_INBOUND_CONSTRUCTOR, props[PROP_INBOUND_CONSTRUCTOR]);
}

static void
calls_dummy_call_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}


static void
calls_dummy_call_init (CallsDummyCall *self)
{
}
