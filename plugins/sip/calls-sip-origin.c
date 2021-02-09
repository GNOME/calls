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

  /* Account information */
  gchar *user;
  gchar *password;
  gchar *host;
  gchar *protocol;
  gboolean use_direct_connection;

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
  PROP_ACC_USER,
  PROP_ACC_PASSWORD,
  PROP_ACC_HOST,
  PROP_ACC_PROTOCOL,
  PROP_ACC_DIRECT,
  PROP_CALLS,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

static gboolean
protocol_is_valid (const gchar *protocol)
{
  return g_strcmp0 (protocol, "UDP") == 0 ||
    g_strcmp0 (protocol, "TLS") == 0;
}

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

  case PROP_ACC_USER:
    g_free (self->user);
    self->user = g_value_dup_string (value);
    break;

  case PROP_ACC_PASSWORD:
    g_free (self->password);
    self->password = g_value_dup_string (value);
    break;

  case PROP_ACC_HOST:
    g_free (self->host);
    self->host = g_value_dup_string (value);
    break;

  case PROP_ACC_PROTOCOL:
    if (!protocol_is_valid (g_value_get_string (value))) {
      g_warning ("Tried setting invalid protocol: '%s'\n"
                 "Continue using old protocol: '%s'",
                 g_value_get_string (value), self->protocol);
      return;
    }

    g_free (self->protocol);
    self->protocol = g_value_dup_string (value);
    break;

  case PROP_ACC_DIRECT:
    self->use_direct_connection = g_value_get_boolean (value);
    break;

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

  case PROP_ACC_USER:
    g_value_set_string (value, self->user);
    break;

  case PROP_ACC_HOST:
    g_value_set_string (value, self->host);
    break;

  case PROP_ACC_PROTOCOL:
    g_value_set_string (value, self->protocol);
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
  g_free (self->user);
  g_free (self->password);
  g_free (self->host);
  g_free (self->protocol);

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

  props[PROP_ACC_USER] =
    g_param_spec_string ("user",
                         "User",
                         "The username for authentication",
                         "",
                         G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ACC_USER, props[PROP_ACC_USER]);

  props[PROP_ACC_PASSWORD] =
    g_param_spec_string ("password",
                         "Password",
                         "The password for authentication",
                         "",
                         G_PARAM_WRITABLE);
  g_object_class_install_property (object_class, PROP_ACC_PASSWORD, props[PROP_ACC_PASSWORD]);

  props[PROP_ACC_HOST] =
    g_param_spec_string ("host",
                         "Host",
                         "The fqdn of the SIP server",
                         "",
                         G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ACC_HOST, props[PROP_ACC_HOST]);

  props[PROP_ACC_PROTOCOL] =
    g_param_spec_string ("protocol",
                         "Protocol",
                         "The protocol used to connect to the SIP server",
                         "UDP",
                         G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ACC_PROTOCOL, props[PROP_ACC_PROTOCOL]);

  props[PROP_ACC_DIRECT] =
    g_param_spec_boolean ("direct-connection",
                          "Direct connection",
                          "Whether to use a direct connection (no SIP server)",
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_ACC_DIRECT, props[PROP_ACC_DIRECT]);

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
calls_sip_origin_new (const gchar *name,
                      const gchar *user,
                      const gchar *password,
                      const gchar *host,
                      const gchar *protocol,
                      gboolean     direct_connection)

{
  CallsSipOrigin *origin =
    g_object_new (CALLS_TYPE_SIP_ORIGIN,
                  "user", user,
                  "password", password,
                  "host", host,
                  "protocol", protocol,
                  "direct-connection", direct_connection,
                  NULL);

  g_string_assign (origin->name, name);

  return origin;
}
