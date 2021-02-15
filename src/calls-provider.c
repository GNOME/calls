/*
 * Copyright (C) 2018,2021 Purism SPC
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
 * The #CallsProvider abstract class is the root of the class tree that
 * needs to be implemented by a call provider.  A #CallsProvider
 * provides access to a list of #CallsOrigin interfaces, through the
 * #calls_provider_get_origins function and the #origin-added and
 * #origin-removed signals.
 */


typedef struct
{
  int dummy;
} CallsProviderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (CallsProvider, calls_provider, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_STATUS,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
calls_provider_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  CallsProvider *self = CALLS_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_STATUS:
      g_value_set_string (value, calls_provider_get_status (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
calls_provider_class_init (CallsProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_provider_get_property;

  props[PROP_STATUS] =
    g_param_spec_string ("status",
                         "Status",
                         "A text string describing the status for display to the user",
                         "",
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}

static void
calls_provider_init (CallsProvider *self)
{
}

/**
 * calls_provider_get_name:
 * @self: a #CallsProvider
 *
 * Get the user-presentable name of the provider.
 *
 * Returns: A string containing the name.
 */
const char *
calls_provider_get_name (CallsProvider *self)
{
  g_return_val_if_fail (CALLS_IS_PROVIDER (self), NULL);

  return CALLS_PROVIDER_GET_CLASS (self)->get_name (self);
}

const char *
calls_provider_get_status (CallsProvider *self)
{
  g_return_val_if_fail (CALLS_IS_PROVIDER (self), NULL);

  return CALLS_PROVIDER_GET_CLASS (self)->get_status (self);
}

/**
 * calls_provider_get_origins:
 * @self: a #CallsProvider
 * @error: a #GError, or #NULL
 *
 * Get the list of #CallsOrigin interfaces offered by this provider.
 *
 * Returns: (transfer none): A #GListModel of origins
 */
GListModel *
calls_provider_get_origins (CallsProvider *self)
{
  g_return_val_if_fail (CALLS_IS_PROVIDER (self), NULL);

  return CALLS_PROVIDER_GET_CLASS (self)->get_origins (self);
}
