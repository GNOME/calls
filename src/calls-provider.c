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

#include "calls-provider.h"
#include "calls-origin.h"
#include "calls-message-source.h"
#include "util.h"

#include <glib/gi18n.h>

/**
 * SECTION:calls-provider
 * @short_description: An abstraction of call providers, such as
 * oFono, Telepathy or some SIP library.
 * @Title: CallsProvider
 *
 * The #CallsProvider interface is the root of the interface tree that
 * needs to be implemented by a call provider.  A #CallsProvider
 * provides access to a list of #CallsOrigin interfaces, through the
 * #calls_provider_get_origins function and the #origin-added and
 * #origin-removed signals.
 */


G_DEFINE_INTERFACE (CallsProvider, calls_provider, CALLS_TYPE_MESSAGE_SOURCE);

enum {
  PROP_0,
  PROP_STATUS,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


enum {
  SIGNAL_ORIGIN_ADDED,
  SIGNAL_ORIGIN_REMOVED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals [SIGNAL_LAST_SIGNAL];

static void
calls_provider_default_init (CallsProviderInterface *iface)
{
  GType arg_types = CALLS_TYPE_ORIGIN;

  props[PROP_STATUS] =
    g_param_spec_string ("status",
                         "Status",
                         "A text string describing the status for display to the user",
                         "",
                         G_PARAM_READABLE);

  g_object_interface_install_property (iface, props[PROP_STATUS]);

  signals[SIGNAL_ORIGIN_ADDED] =
    g_signal_newv ("origin-added",
		   G_TYPE_FROM_INTERFACE (iface),
		   G_SIGNAL_RUN_LAST,
		   NULL, NULL, NULL, NULL,
		   G_TYPE_NONE,
		   1, &arg_types);

  signals[SIGNAL_ORIGIN_REMOVED] =
    g_signal_newv ("origin-removed",
		   G_TYPE_FROM_INTERFACE (iface),
		   G_SIGNAL_RUN_LAST,
		   NULL, NULL, NULL, NULL,
		   G_TYPE_NONE,
		   1, &arg_types);
}

#define DEFINE_PROVIDER_FUNC(function,rettype,errval)	\
  CALLS_DEFINE_IFACE_FUNC(provider, Provider, PROVIDER,  \
			 function, rettype, errval)

/**
 * calls_provider_get_name:
 * @self: a #CallsProvider
 *
 * Get the user-presentable name of the provider.
 *
 * Returns: A string containing the name.
 */
DEFINE_PROVIDER_FUNC(get_name, const gchar *, NULL);

gchar *
calls_provider_get_status (CallsProvider *self)
{
  gchar *status;

  g_return_val_if_fail (CALLS_IS_PROVIDER (self), NULL);

  g_object_get (G_OBJECT (self),
                "status", &status,
                NULL);

  return status;
}


/**
 * calls_provider_get_origins:
 * @self: a #CallsProvider
 * @error: a #GError, or #NULL
 *
 * Get the list of #CallsOrigin interfaces offered by this provider.
 *
 * Returns: A newly-allocated GList of objects implementing
 * #CallsOrigin or NULL if there was an error.
 */
DEFINE_PROVIDER_FUNC(get_origins, GList *, NULL);
