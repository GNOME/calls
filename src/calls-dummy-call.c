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
#include "calls-call.h"

#include <glib/gi18n.h>


struct _CallsDummyCall
{
  GObject parent_instance;
  gchar *number;
  CallsCallState state;
};

static void calls_dummy_call_call_interface_init (CallsCallInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsDummyCall, calls_dummy_call, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_CALL,
                                                calls_dummy_call_call_interface_init))

enum {
  PROP_0,
  PROP_NUMBER,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

static const gchar *
get_number (CallsCall *iface)
{
  CallsDummyCall *self;

  g_return_val_if_fail (CALLS_IS_DUMMY_CALL (iface), NULL);
  self = CALLS_DUMMY_CALL (iface);

  return self->number;
}

static const gchar *
get_name (CallsCall *iface)
{
  return NULL;
}

static CallsCallState
get_state (CallsCall *call)
{
  CallsDummyCall *self;

  g_return_val_if_fail (CALLS_IS_DUMMY_CALL (call), 0);
  self = CALLS_DUMMY_CALL (call);

  return self->state;
}

static void
change_state (CallsCall      *call,
              CallsDummyCall *self,
              CallsCallState  state)
{
  self->state = state;
  g_signal_emit_by_name (call, "state-changed", state);
}

static void
answer (CallsCall *call)
{
  CallsDummyCall *self;

  g_return_if_fail (CALLS_IS_DUMMY_CALL (call));
  self = CALLS_DUMMY_CALL (call);

  g_return_if_fail (self->state == CALLS_CALL_STATE_INCOMING);

  change_state (call, self, CALLS_CALL_STATE_ACTIVE);
}

static void
hang_up (CallsCall *call)
{
  CallsDummyCall *self;

  g_return_if_fail (CALLS_IS_DUMMY_CALL (call));
  self = CALLS_DUMMY_CALL (call);

  change_state (call, self, CALLS_CALL_STATE_DISCONNECTED);
}

static void
tone_start (CallsCall *call, gchar key)
{
  g_info ("Beep! (%c)", (int)key);
}

static void
tone_stop (CallsCall *call, gchar key)
{
  g_info ("Beep end (%c)", (int)key);
}

CallsDummyCall *
calls_dummy_call_new (const gchar *number)
{
  g_return_val_if_fail (number != NULL, NULL);

  return g_object_new (CALLS_TYPE_DUMMY_CALL,
                       "number", number,
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
  case PROP_NUMBER:
    self->number = g_value_dup_string (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
finalize (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsDummyCall *self = CALLS_DUMMY_CALL (object);

  g_free (self->number);

  parent_class->finalize (object);
}


static void
calls_dummy_call_class_init (CallsDummyCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = finalize;
  object_class->set_property = set_property;

  props[PROP_NUMBER] =
    g_param_spec_string ("number",
                         _("Number"),
                         _("The dialed number"),
                         "+441234567890",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}

static void
calls_dummy_call_call_interface_init (CallsCallInterface *iface)
{
  iface->get_number = get_number;
  iface->get_name = get_name;
  iface->get_state = get_state;
  iface->answer = answer;
  iface->hang_up = hang_up;
  iface->tone_start = tone_start;
  iface->tone_stop = tone_stop;
}

static void
calls_dummy_call_init (CallsDummyCall *self)
{
}
