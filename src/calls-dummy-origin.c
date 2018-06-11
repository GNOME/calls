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

#include "calls-dummy-origin.h"
#include "calls-message-source.h"
#include "calls-origin.h"
#include "calls-dummy-call.h"

#include <glib/gi18n.h>
#include <glib-object.h>


struct _CallsDummyOrigin
{
  GObject parent_instance;
  GString *name;
  GList *calls;
};

static void calls_dummy_origin_message_source_interface_init (CallsOriginInterface *iface);
static void calls_dummy_origin_origin_interface_init (CallsOriginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsDummyOrigin, calls_dummy_origin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_dummy_origin_message_source_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_ORIGIN,
                                                calls_dummy_origin_origin_interface_init))

enum {
  PROP_0,
  PROP_NAME,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static const gchar *
get_name (CallsOrigin *origin)
{
  CallsDummyOrigin *self;

  g_return_val_if_fail (CALLS_IS_DUMMY_ORIGIN (origin), NULL);
  self = CALLS_DUMMY_ORIGIN (origin);

  return self->name->str;
}


static GList *
get_calls (CallsOrigin *origin)
{
  CallsDummyOrigin *self;

  g_return_val_if_fail (CALLS_IS_DUMMY_ORIGIN (origin), NULL);
  self = CALLS_DUMMY_ORIGIN (origin);

  return g_list_copy (self->calls);
}


static void
remove_call (CallsDummyOrigin *self,
             CallsCall        *call,
             const gchar      *reason)
{
  CallsOrigin *origin;

  origin = CALLS_ORIGIN (self);
  g_signal_emit_by_name (origin, "call-removed", call, reason);

  self->calls = g_list_remove (self->calls, call);
  g_object_unref (G_OBJECT (call));
}


static void
remove_calls (CallsDummyOrigin *self, const gchar *reason)
{
  GList *node, *next;

  for (node = self->calls; node; node = next)
    {
      next = node->next;

      remove_call (self, CALLS_CALL (node->data), reason);
    }
}


static void
call_state_changed_cb (CallsDummyOrigin *self,
                       CallsCallState    state,
                       CallsCall        *call)
{
  if (state != CALLS_CALL_STATE_DISCONNECTED)
    {
      return;
    }

  g_return_if_fail (CALLS_IS_DUMMY_ORIGIN (self));
  g_return_if_fail (CALLS_IS_CALL (call));

  remove_call (self, call, "Disconnected");
}


static void
dial (CallsOrigin *origin, const gchar *number)
{
  CallsDummyOrigin *self;
  CallsDummyCall *dummy_call;
  CallsCall *call;

  g_return_if_fail (number != NULL);
  g_return_if_fail (CALLS_IS_DUMMY_ORIGIN (origin));

  self = CALLS_DUMMY_ORIGIN (origin);

  dummy_call = calls_dummy_call_new (number);
  g_return_if_fail (dummy_call != NULL);

  call = CALLS_CALL (dummy_call);
  g_signal_connect_swapped (call, "state-changed",
                            G_CALLBACK (call_state_changed_cb),
                            self);

  self->calls = g_list_append (self->calls, dummy_call);

  g_signal_emit_by_name (origin, "call-added", call);
}


CallsDummyOrigin *
calls_dummy_origin_new (const gchar *name)
{
  return g_object_new (CALLS_TYPE_DUMMY_ORIGIN,
                       "name", name,
                       NULL);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsDummyOrigin *self = CALLS_DUMMY_ORIGIN (object);

  switch (property_id) {
  case PROP_NAME:
    g_string_assign (self->name, g_value_get_string (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
dispose (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsDummyOrigin *self = CALLS_DUMMY_ORIGIN (object);

  remove_calls (self, NULL);

  parent_class->dispose (object);
}


static void
finalize (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsDummyOrigin *self = CALLS_DUMMY_ORIGIN (object);

  g_string_free (self->name, TRUE);

  parent_class->finalize (object);
}


static void
calls_dummy_origin_class_init (CallsDummyOriginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = dispose;
  object_class->finalize = finalize;
  object_class->set_property = set_property;

  props[PROP_NAME] =
    g_param_spec_string ("name",
                         _("Name"),
                         _("The name of the origin"),
                         "Dummy origin",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_dummy_origin_message_source_interface_init (CallsOriginInterface *iface)
{
}


static void
calls_dummy_origin_origin_interface_init (CallsOriginInterface *iface)
{
  iface->get_name = get_name;
  iface->get_calls = get_calls;
  iface->dial = dial;
}


static void
calls_dummy_origin_init (CallsDummyOrigin *self)
{
  self->name = g_string_new (NULL);
}
