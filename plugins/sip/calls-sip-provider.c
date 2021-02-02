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

#include "calls-sip-provider.h"

#include "calls-message-source.h"
#include "calls-provider.h"
#include "calls-sip-origin.h"

#include <libpeas/peas.h>


struct _CallsSipProvider
{
  CallsProvider parent_instance;

  GListStore *origins;
};

static void calls_sip_provider_message_source_interface_init (CallsMessageSourceInterface *iface);


G_DEFINE_DYNAMIC_TYPE_EXTENDED
(CallsSipProvider, calls_sip_provider, CALLS_TYPE_PROVIDER, 0,
 G_IMPLEMENT_INTERFACE_DYNAMIC (CALLS_TYPE_MESSAGE_SOURCE,
                                calls_sip_provider_message_source_interface_init))


static const char *
calls_sip_provider_get_name (CallsProvider *provider)
{
  return "SIP provider";
}

static const char *
calls_sip_provider_get_status (CallsProvider *provider)
{
  return "Normal";
}

static GListModel *
calls_sip_provider_get_origins (CallsProvider *provider)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (provider);

  return G_LIST_MODEL (self->origins);
}

static void
calls_sip_provider_constructed (GObject *object)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (object);

  calls_sip_provider_add_origin (self, "Sip origin");

  G_OBJECT_CLASS (calls_sip_provider_parent_class)->constructed (object);
}


static void
calls_sip_provider_dispose (GObject *object)
{
  CallsSipProvider *self = CALLS_SIP_PROVIDER (object);

  g_list_store_remove_all (self->origins);
  g_clear_object (&self->origins);

  G_OBJECT_CLASS (calls_sip_provider_parent_class)->dispose (object);
}


static void
calls_sip_provider_class_init (CallsSipProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsProviderClass *provider_class = CALLS_PROVIDER_CLASS (klass);

  object_class->constructed = calls_sip_provider_constructed;
  object_class->dispose = calls_sip_provider_dispose;

  provider_class->get_name = calls_sip_provider_get_name;
  provider_class->get_status = calls_sip_provider_get_status;
  provider_class->get_origins = calls_sip_provider_get_origins;
}


static void
calls_sip_provider_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}


static void
calls_sip_provider_init (CallsSipProvider *self)
{
  self->origins = g_list_store_new (CALLS_TYPE_SIP_ORIGIN);
}


void
calls_sip_provider_add_origin (CallsSipProvider *self,
                               const gchar      *name)
{
  g_autoptr (CallsSipOrigin) origin = calls_sip_origin_new (name);

  g_list_store_append (self->origins, origin);
}


CallsSipProvider *
calls_sip_provider_new ()
{
  return g_object_new (CALLS_TYPE_SIP_PROVIDER, NULL);
}


#ifndef FOR_TESTING

static void
calls_sip_provider_class_finalize (CallsSipProviderClass *klass)
{
}


G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
  calls_sip_provider_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              CALLS_TYPE_PROVIDER,
                                              CALLS_TYPE_SIP_PROVIDER);
}

#endif /* FOR_TESTING */
