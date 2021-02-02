/*
 * Copyright (C) 2021 Purism SPC
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
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "calls-sip-origin.h"

#include "calls-message-source.h"
#include "calls-origin.h"
#include "calls-sip-call.h"

#include <glib/gi18n.h>
#include <glib-object.h>


struct _CallsSipOrigin
{
  GObject parent_instance;
  GString *name;
  GList *calls;
};

static void calls_sip_origin_message_source_interface_init (CallsOriginInterface *iface);
static void calls_sip_origin_origin_interface_init (CallsOriginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsSipOrigin, calls_sip_origin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_sip_origin_message_source_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_ORIGIN,
                                                calls_sip_origin_origin_interface_init))

enum {
  PROP_0,
  PROP_NAME,
  PROP_CALLS,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
remove_call (CallsSipOrigin *self,
             CallsCall        *call,
             const gchar      *reason)
{
  CallsOrigin *origin;

  origin = CALLS_ORIGIN (self);
  self->calls = g_list_remove (self->calls, call);

  g_signal_emit_by_name (origin, "call-removed", call, reason);

  g_object_unref (G_OBJECT (call));
}


static void
remove_calls (CallsSipOrigin *self, const gchar *reason)
{
  gpointer call;
  GList *next;

  while (self->calls != NULL) {
    call = self->calls->data;
    next = self->calls->next;
    g_list_free_1 (self->calls);
    self->calls = next;

    g_signal_emit_by_name (self, "call-removed", call, reason);
    g_object_unref (call);
  }
}


struct DisconnectedData
{
  CallsSipOrigin *self;
  CallsCall        *call;
};


static void
on_call_state_changed_cb (CallsSipOrigin *self,
                          CallsCallState  new_state,
                          CallsCallState  old_state,
                          CallsCall      *call)
{
  if (new_state != CALLS_CALL_STATE_DISCONNECTED)
    {
      return;
    }

  g_assert (CALLS_IS_SIP_ORIGIN (self));
  g_assert (CALLS_IS_CALL (call));

  remove_call (self, call, "Disconnected");
}


static void
add_call (CallsSipOrigin *self,
          const gchar *address,
          gboolean inbound)
{
  CallsSipCall *sip_call;
  CallsCall *call;

  sip_call = calls_sip_call_new (address, inbound);
  g_assert (sip_call != NULL);

  call = CALLS_CALL (sip_call);
  g_signal_connect_swapped (call, "state-changed",
                            G_CALLBACK (on_call_state_changed_cb),
                            self);

  self->calls = g_list_append (self->calls, sip_call);

  g_signal_emit_by_name (CALLS_ORIGIN (self), "call-added", call);
}


static void
dial (CallsOrigin *origin,
      const gchar *address)
{
  g_assert (CALLS_ORIGIN (origin));
  g_assert (CALLS_IS_SIP_ORIGIN (origin));

  if (address == NULL) {
    g_warning ("Tried dialing on origin '%s' without an address",
               calls_origin_get_name (origin));
    return;
  }

  add_call (CALLS_SIP_ORIGIN (origin), address, FALSE);
}


static void
calls_sip_origin_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CallsSipOrigin *self = CALLS_SIP_ORIGIN (object);

  switch (property_id) {

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_sip_origin_get_property (GObject      *object,
                               guint         property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  CallsSipOrigin *self = CALLS_SIP_ORIGIN (object);

  switch (property_id) {
  case PROP_NAME:
    g_value_set_string (value, self->name->str);
    break;

  case PROP_CALLS:
    g_value_set_pointer (value, g_list_copy (self->calls));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_sip_origin_dispose (GObject *object)
{
  CallsSipOrigin *self = CALLS_SIP_ORIGIN (object);

  remove_calls (self, NULL);

  G_OBJECT_CLASS (calls_sip_origin_parent_class)->dispose (object);
}


static void
calls_sip_origin_finalize (GObject *object)
{
  CallsSipOrigin *self = CALLS_SIP_ORIGIN (object);

  g_string_free (self->name, TRUE);

  G_OBJECT_CLASS (calls_sip_origin_parent_class)->finalize (object);
}


static void
calls_sip_origin_class_init (CallsSipOriginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = calls_sip_origin_dispose;
  object_class->finalize = calls_sip_origin_finalize;
  object_class->get_property = calls_sip_origin_get_property;
  object_class->set_property = calls_sip_origin_set_property;

#define IMPLEMENTS(ID, NAME) \
  g_object_class_override_property (object_class, ID, NAME);    \
  props[ID] = g_object_class_find_property(object_class, NAME);

  IMPLEMENTS (PROP_NAME, "name");
  IMPLEMENTS (PROP_CALLS, "calls");

#undef IMPLEMENTS
}


static void
calls_sip_origin_message_source_interface_init (CallsOriginInterface *iface)
{
}


static void
calls_sip_origin_origin_interface_init (CallsOriginInterface *iface)
{
  iface->dial = dial;
}


static void
calls_sip_origin_init (CallsSipOrigin *self)
{
  self->name = g_string_new (NULL);
}


void
calls_sip_origin_create_inbound (CallsSipOrigin *self,
                                 const gchar    *address)
{
  g_return_if_fail (address != NULL);
  g_return_if_fail (CALLS_IS_SIP_ORIGIN (self));

  add_call (self, address, TRUE);
}


CallsSipOrigin *
calls_sip_origin_new (const gchar *name)
{
  CallsSipOrigin *origin =
    g_object_new (CALLS_TYPE_SIP_ORIGIN,
                  NULL);

  g_string_assign (origin->name, name);

  return origin;
}
