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

#define G_LOG_DOMAIN "CallsDummyProvider"

#include "calls-dummy-provider.h"
#include "calls-message-source.h"
#include "calls-provider.h"
#include "calls-dummy-origin.h"

#include <libpeas.h>
#include <glib-unix.h>

static const char * const supported_protocols[] = {
  "tel",
  NULL
};

struct _CallsDummyProvider {
  CallsProvider parent_instance;

  GListStore   *origins;
};

static void calls_dummy_provider_message_source_interface_init (CallsMessageSourceInterface *iface);


#ifdef FOR_TESTING

G_DEFINE_TYPE_WITH_CODE
  (CallsDummyProvider, calls_dummy_provider, CALLS_TYPE_PROVIDER,
  G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                         calls_dummy_provider_message_source_interface_init))

#else

G_DEFINE_DYNAMIC_TYPE_EXTENDED
  (CallsDummyProvider, calls_dummy_provider, CALLS_TYPE_PROVIDER, 0,
   G_IMPLEMENT_INTERFACE_DYNAMIC (CALLS_TYPE_MESSAGE_SOURCE,
                                  calls_dummy_provider_message_source_interface_init))

#endif /* FOR_TESTING */


static gboolean
usr1_handler (CallsDummyProvider *self)
{
  GListModel *model;

  g_autoptr (CallsDummyOrigin) origin = NULL;

  model = G_LIST_MODEL (self->origins);
  g_return_val_if_fail (g_list_model_get_n_items (model) > 0, FALSE);

  g_debug ("Received SIGUSR1, adding new incoming call");

  origin = g_list_model_get_item (model, 0);
  calls_dummy_origin_create_inbound (origin, "0987654321");

  return TRUE;
}


static gboolean
usr2_handler (CallsDummyProvider *self)
{
  g_autoptr (CallsDummyOrigin) origin = NULL;

  GListModel *model;

  model = G_LIST_MODEL (self->origins);
  g_return_val_if_fail (g_list_model_get_n_items (model) > 0, FALSE);

  g_debug ("Received SIGUSR2, adding new anonymous incoming call");

  origin = g_list_model_get_item (model, 0);
  calls_dummy_origin_create_inbound (origin, NULL);

  return TRUE;
}


static const char *
calls_dummy_provider_get_name (CallsProvider *provider)
{
  return "Dummy provider";
}

static const char *
calls_dummy_provider_get_status (CallsProvider *provider)
{
  return "Normal";
}

static GListModel *
calls_dummy_provider_get_origins (CallsProvider *provider)
{
  CallsDummyProvider *self = CALLS_DUMMY_PROVIDER (provider);

  return G_LIST_MODEL (self->origins);
}

static const char *const *
calls_dummy_provider_get_protocols (CallsProvider *provider)
{
  return supported_protocols;
}

static gboolean
calls_dummy_provider_is_modem (CallsProvider *provider)
{
  return TRUE;
}

static void
constructed (GObject *object)
{
  CallsDummyProvider *self = CALLS_DUMMY_PROVIDER (object);

  calls_dummy_provider_add_origin (self, "Dummy origin");

  g_unix_signal_add (SIGUSR1,
                     (GSourceFunc) usr1_handler,
                     self);
  g_unix_signal_add (SIGUSR2,
                     (GSourceFunc) usr2_handler,
                     self);

  G_OBJECT_CLASS (calls_dummy_provider_parent_class)->constructed (object);
}


static void
dispose (GObject *object)
{
  CallsDummyProvider *self = CALLS_DUMMY_PROVIDER (object);

  g_list_store_remove_all (self->origins);
  g_clear_object (&self->origins);

  G_OBJECT_CLASS (calls_dummy_provider_parent_class)->dispose (object);
}


static void
calls_dummy_provider_class_init (CallsDummyProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsProviderClass *provider_class = CALLS_PROVIDER_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;

  provider_class->get_name = calls_dummy_provider_get_name;
  provider_class->get_status = calls_dummy_provider_get_status;
  provider_class->get_origins = calls_dummy_provider_get_origins;
  provider_class->get_protocols = calls_dummy_provider_get_protocols;
  provider_class->is_modem = calls_dummy_provider_is_modem;
}


static void
calls_dummy_provider_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}


static void
calls_dummy_provider_init (CallsDummyProvider *self)
{
  self->origins = g_list_store_new (CALLS_TYPE_ORIGIN);
}


void
calls_dummy_provider_add_origin (CallsDummyProvider *self,
                                 const gchar        *name)
{
  g_autoptr (CallsDummyOrigin) origin = NULL;

  origin = calls_dummy_origin_new (name);
  g_list_store_append (self->origins, origin);
}


CallsDummyProvider *
calls_dummy_provider_new (void)
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
