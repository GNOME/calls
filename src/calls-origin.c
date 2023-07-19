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

#include "calls-origin.h"
#include "calls-message-source.h"
#include "calls-util.h"

/**
 * SECTION:calls-origin
 * @short_description: An object that originates calls.
 * @Title: CallsOrigin
 */


G_DEFINE_INTERFACE (CallsOrigin, calls_origin, CALLS_TYPE_MESSAGE_SOURCE)

enum {
  SIGNAL_CALL_ADDED,
  SIGNAL_CALL_REMOVED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];

static void
calls_origin_default_init (CallsOriginInterface *iface)
{
  GType arg_types[2] = { CALLS_TYPE_CALL, G_TYPE_STRING };

  g_object_interface_install_property (
    iface,
    g_param_spec_string ("name",
                         "Name",
                         "The name of the origin",
                         NULL,
                         G_PARAM_READABLE));

  g_object_interface_install_property (
    iface,
    g_param_spec_pointer ("calls",
                          "Calls",
                          "The list of current calls",
                          G_PARAM_READABLE));

  g_object_interface_install_property (
    iface,
    g_param_spec_string ("country-code",
                         "country code",
                         "The country code of the origin, if any",
                         NULL,
                         G_PARAM_READABLE));

  g_object_interface_install_property (
    iface,
    g_param_spec_string ("id",
                         "ID",
                         "ID of the origin",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * CallsOrigin:emergency-numbers:
   *
   * The available emergency numbers.
   */
  g_object_interface_install_property (
    iface,
    g_param_spec_boxed ("emergency-numbers",
                        "",
                        "",
                        G_TYPE_STRV,
                        G_PARAM_READABLE));

  signals[SIGNAL_CALL_ADDED] =
    g_signal_newv ("call-added",
                   G_TYPE_FROM_INTERFACE (iface),
                   G_SIGNAL_RUN_LAST,
                   NULL, NULL, NULL, NULL,
                   G_TYPE_NONE,
                   1, arg_types);

  signals[SIGNAL_CALL_REMOVED] =
    g_signal_newv ("call-removed",
                   G_TYPE_FROM_INTERFACE (iface),
                   G_SIGNAL_RUN_LAST,
                   NULL, NULL, NULL, NULL,
                   G_TYPE_NONE,
                   2, arg_types);
}

#define DEFINE_ORIGIN_GETTER(prop,rettype,errval) \
  CALLS_DEFINE_IFACE_GETTER(origin, Origin, ORIGIN, prop, rettype, errval)

/**
 * calls_origin_get_name:
 * @self: a #CallsOrigin
 *
 * Get the user-presentable name of the origin.
 *
 * Returns (transfer full): A string containing the name.  The string must be freed by
 * the caller.
 */
DEFINE_ORIGIN_GETTER (name, char *, NULL);

/**
 * calls_origin_get_id:
 * @self: a #CallsOrigin
 *
 * Get the id of the origin.
 *
 * Returns (transfer full): A string containing the id.  The string must be freed by
 * the caller.
 */
DEFINE_ORIGIN_GETTER (id, char *, NULL);

/**
 * calls_origin_get_calls:
 * @self: a #CallsOrigin
 *
 * Get the list of current calls.
 *
 * Returns: (transfer container): A newly-allocated GList of objects implementing
 * #CallsCall or NULL if there was an error.
 */
DEFINE_ORIGIN_GETTER (calls, GList *, NULL);

/**
 * calls_origin_get_emergency_numbers:
 * @self: a #CallsOrigin
 *
 * Returns: (transfer full): The supported emergency numbers
 */
GStrv
calls_origin_get_emergency_numbers (CallsOrigin *origin)
{
  GStrv numbers;

  g_return_val_if_fail (CALLS_IS_ORIGIN (origin), NULL);

  g_object_get (origin, "emergency-numbers", &numbers, NULL);
  return numbers;
}

/**
 * calls_origin_foreach_call:
 * @self: a #CallsOrigin
 * @callback: function to be called for each call from the origin
 * @param: user data for @callback
 *
 * Iterate over all current calls from this origin
 **/
void
calls_origin_foreach_call (CallsOrigin               *self,
                           CallsOriginForeachCallFunc callback,
                           gpointer                   param)
{
  g_autoptr (GList) calls = NULL;
  GList *node;

  calls = calls_origin_get_calls (self);

  for (node = calls; node; node = node->next)
    callback (param, CALLS_CALL (node->data), self);
}


/**
 * calls_origin_dial:
 * @self: a #CallsOrigin
 * @number: the number to dial
 *
 * Dial a new number from this origin.  If a new call is successfully
 * created, the #call-added signal will be emitted with the call.  If
 * there is an error, an appropriate #message signal will be emitted.
 */
void
calls_origin_dial (CallsOrigin *self,
                   const char  *number)
{
  CallsOriginInterface *iface;

  g_return_if_fail (CALLS_IS_ORIGIN (self));
  g_return_if_fail (number != NULL);

  iface = CALLS_ORIGIN_GET_IFACE (self);
  g_return_if_fail (iface->dial != NULL);

  return iface->dial (self, number);
}

/**
 * calls_origin_supports_protocol:
 * @self: A #CallsOrigin
 * @protocol: The protocol to check support for
 *
 * Returns: %TRUE if the origin supports the protocol, %FALSE otherwise
 */
gboolean
calls_origin_supports_protocol (CallsOrigin *self,
                                const char  *protocol)
{
  CallsOriginInterface *iface;

  g_return_val_if_fail (CALLS_IS_ORIGIN (self), FALSE);
  g_return_val_if_fail (protocol != NULL, FALSE);

  iface = CALLS_ORIGIN_GET_IFACE (self);
  g_return_val_if_fail (iface->supports_protocol != NULL, FALSE);

  return iface->supports_protocol (self, protocol);
}

/**
 * calls_origin_get_country_code:
 * @self: A #CallsOrigin
 *
 * Returns: (nullable): The iso country code
 */
const char *
calls_origin_get_country_code (CallsOrigin *self)
{
  CallsOriginInterface *iface;

  g_return_val_if_fail (CALLS_IS_ORIGIN (self), FALSE);

  iface = CALLS_ORIGIN_GET_IFACE (self);
  /* country code is optional */
  if (iface->get_country_code == NULL)
    return NULL;

  return iface->get_country_code (self);
}

/**
 * calls_origin_get_network_country_code:
 * @self: A #CallsOrigin
 *
 * Gets the country code of the mobile network we're currently
 * connected to.
 *
 * Returns: (nullable): The country code
 */
const char *
calls_origin_get_network_country_code (CallsOrigin *self)
{
  CallsOriginInterface *iface;

  g_return_val_if_fail (CALLS_IS_ORIGIN (self), FALSE);

  iface = CALLS_ORIGIN_GET_IFACE (self);
  /* network country code is optional */
  if (iface->get_network_country_code == NULL)
    return NULL;

  return iface->get_network_country_code (self);
}
