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
#include "calls-provider.h"
#include "calls-dummy-origin.h"

struct _CallsDummyProvider
{
  GObject parent_instance;

  GList *origins;
};

static void calls_dummy_provider_provider_interface_init (CallsProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsDummyProvider, calls_dummy_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_PROVIDER,
                                                calls_dummy_provider_provider_interface_init))

static const gchar *
get_name (CallsProvider *iface)
{
  return "Dummy provider";
}

static GList *
get_origins (CallsProvider *iface)
{
  CallsDummyProvider *self = CALLS_DUMMY_PROVIDER (iface);
  return self->origins;
}

CallsDummyProvider *
calls_dummy_provider_new ()
{
  return g_object_new (CALLS_TYPE_DUMMY_PROVIDER, NULL);
}


static void
dispose (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsDummyProvider *self = CALLS_DUMMY_PROVIDER (object);

  g_list_free_full (self->origins, g_object_unref);
  self->origins = NULL;

  parent_class->dispose (object);
}

static void
calls_dummy_provider_class_init (CallsDummyProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = dispose;
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
  self->origins = g_list_append (self->origins,
                                 calls_dummy_origin_new (NULL));
  self->origins = g_list_append (self->origins,
                                 calls_dummy_origin_new ("Dummy origin 2"));
}
