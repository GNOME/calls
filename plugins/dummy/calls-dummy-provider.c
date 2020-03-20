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

#include "calls-dummy-provider.h"
#include "calls-message-source.h"
#include "calls-provider.h"
#include "calls-dummy-origin.h"

#include <libpeas/peas.h>
#include <glib-unix.h>


struct _CallsDummyProvider
{
  GObject parent_instance;

  GList *origins;
};

static void calls_dummy_provider_message_source_interface_init (CallsProviderInterface *iface);
static void calls_dummy_provider_provider_interface_init (CallsProviderInterface *iface);


#ifdef FOR_TESTING

G_DEFINE_TYPE_WITH_CODE
(CallsDummyProvider, calls_dummy_provider, G_TYPE_OBJECT,
 G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                        calls_dummy_provider_message_source_interface_init)
 G_IMPLEMENT_INTERFACE (CALLS_TYPE_PROVIDER,
                        calls_dummy_provider_provider_interface_init))

#else

G_DEFINE_DYNAMIC_TYPE_EXTENDED
(CallsDummyProvider, calls_dummy_provider, G_TYPE_OBJECT, 0,
 G_IMPLEMENT_INTERFACE_DYNAMIC (CALLS_TYPE_MESSAGE_SOURCE,
                                calls_dummy_provider_message_source_interface_init)
 G_IMPLEMENT_INTERFACE_DYNAMIC (CALLS_TYPE_PROVIDER,
                                calls_dummy_provider_provider_interface_init))

#endif /* FOR_TESTING */

enum {
  PROP_0,
  PROP_STATUS,
  PROP_LAST_PROP,
};

static const gchar *
get_name (CallsProvider *iface)
{
  return "Dummy provider";
}


static GList *
get_origins (CallsProvider *iface)
{
  CallsDummyProvider *self = CALLS_DUMMY_PROVIDER (iface);
  return g_list_copy (self->origins);
}


static gboolean
usr1_handler (CallsDummyProvider *self)
{
  CallsDummyOrigin *origin;

  g_return_val_if_fail (self->origins != NULL, FALSE);

  g_debug ("Received SIGUSR1, adding new incoming call");

  origin = CALLS_DUMMY_ORIGIN (self->origins->data);
  calls_dummy_origin_create_inbound (origin, "0987654321");

  return TRUE;
}


static void
constructed (GObject *object)
{
  CallsDummyProvider *self = CALLS_DUMMY_PROVIDER (object);

  calls_dummy_provider_add_origin (self, "Dummy origin");

  g_unix_signal_add (SIGUSR1,
                     (GSourceFunc)usr1_handler,
                     self);

  G_OBJECT_CLASS (calls_dummy_provider_parent_class)->constructed (object);
}


static void
get_property (GObject      *object,
              guint         property_id,
              GValue       *value,
              GParamSpec   *pspec)
{
  switch (property_id) {
  case PROP_STATUS:
    g_value_set_string (value, "Normal");
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
dispose (GObject *object)
{
  gpointer origin;
  GList *next;
  CallsDummyProvider *self = CALLS_DUMMY_PROVIDER (object);

  while (self->origins != NULL) {
    origin = self->origins->data;
    next = self->origins->next;
    g_list_free_1 (self->origins);
    self->origins = next;

    g_signal_emit_by_name (self, "origin-removed", origin);
    g_object_unref (origin);
  }

  G_OBJECT_CLASS (calls_dummy_provider_parent_class)->dispose (object);
}


static void
calls_dummy_provider_class_init (CallsDummyProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = constructed;
  object_class->get_property = get_property;
  object_class->dispose = dispose;

  g_object_class_override_property (object_class, PROP_STATUS, "status");
}


static void
calls_dummy_provider_message_source_interface_init (CallsProviderInterface *iface)
{
}


static void
calls_dummy_provider_provider_interface_init (CallsProviderInterface *iface)
{
  iface->get_name = get_name;
  iface->get_origins = get_origins;
}


static void
calls_dummy_provider_init (CallsDummyProvider *self)
{
}


void
calls_dummy_provider_add_origin (CallsDummyProvider *self,
                                 const gchar        *name)
{
  CallsDummyOrigin *origin = calls_dummy_origin_new (name);
  self->origins = g_list_append (self->origins, origin);

  g_signal_emit_by_name (CALLS_PROVIDER (self), "origin-added", CALLS_ORIGIN (origin));
}


CallsDummyProvider *
calls_dummy_provider_new ()
{
  return g_object_new (CALLS_TYPE_DUMMY_PROVIDER, NULL);
}


#ifndef FOR_TESTING

static void
calls_dummy_provider_class_finalize (CallsDummyProviderClass *klass)
{
}


G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
  calls_dummy_provider_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              CALLS_TYPE_PROVIDER,
                                              CALLS_TYPE_DUMMY_PROVIDER);
}

#endif /* FOR_TESTING */
