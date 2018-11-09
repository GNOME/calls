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

#include "calls-enumerate.h"
#include "calls-origin.h"
#include "calls-call.h"

#include <glib/gi18n.h>



typedef void (*CallAddedCallback) (gpointer     user_data,
                                   CallsCall   *call,
                                   CallsOrigin *origin);

static void
enum_call_added_cb (CallsOrigin          *origin,
                    CallsCall            *call,
                    CallsEnumerateParams *params)
{
  // Call call-added signal
  if (calls_enumerate_params_get_enumerating (params))
    {
      CallAddedCallback call_added_cb;

      call_added_cb = (CallAddedCallback)
        calls_enumerate_params_lookup (params,
                                       CALLS_TYPE_ORIGIN,
                                       "call-added");
      if (call_added_cb)
        {
          gpointer user_data = calls_enumerate_params_get_user_data (params);
          call_added_cb (user_data, call, origin);
        }
    }

  // Connect user's callbacks
  calls_enumerate_params_connect (params, G_OBJECT (call));
}


static void
enum_origin_calls (CallsOrigin          *origin,
                   CallsEnumerateParams *params)
{
  GList *calls, *node;

  calls = calls_origin_get_calls (origin);

  for (node = calls; node; node = node->next)
    {
      enum_call_added_cb (origin,
                          CALLS_CALL (node->data),
                          params);
    }

  g_list_free (calls);
}


typedef void (*OriginAddedCallback) (gpointer       user_data,
                                     CallsOrigin   *origin,
                                     CallsProvider *provider);

static void
enum_origin_added_cb (CallsProvider        *provider,
                      CallsOrigin          *origin,
                      CallsEnumerateParams *params)
{
  gboolean add_callback;

  // Call origin-added signal
  if (calls_enumerate_params_get_enumerating (params))
    {
      OriginAddedCallback origin_added_cb;

      origin_added_cb = (OriginAddedCallback)
        calls_enumerate_params_lookup (params,
                                           CALLS_TYPE_PROVIDER,
                                           "origin-added");
      if (origin_added_cb)
        {
          gpointer user_data = calls_enumerate_params_get_user_data (params);
          origin_added_cb (user_data, origin, provider);
        }
    }

  // Connect user's callbacks
  calls_enumerate_params_connect (params, G_OBJECT (origin));

  // We add a callback for ourselves if we have to set callbacks on
  // anything lower in the hierarchy in future
  add_callback =
    calls_enumerate_params_have_callbacks (params, CALLS_TYPE_CALL);

  if (add_callback)
    {
      g_object_ref (params);
      g_signal_connect_data (origin,
                             "call-added",
                             G_CALLBACK (enum_call_added_cb),
                             params,
                             (GClosureNotify)g_object_unref,
                             0);

    }

  // We enumerate if we've added callbacks and if there's the specific
  // "call-added" callback for this level
  if (add_callback
      ||
      calls_enumerate_params_lookup (params, CALLS_TYPE_ORIGIN,
                                     "call-added"))
    {
      enum_origin_calls (origin, params);
    }
}


static void
enum_provider_origins (CallsProvider        *provider,
                       CallsEnumerateParams *params)
{
  GList *origins, *node;

  origins = calls_provider_get_origins (provider);

  for (node = origins; node; node = node->next)
    {
      enum_origin_added_cb (provider,
                            CALLS_ORIGIN (node->data),
                            params);
    }

  g_list_free (origins);
}


/**
 * calls_enumerateerate:
 * @provider: a #CallsProvider
 * @params: a #CallsEnumerateParams containing callbacks and state
 *
 * Enumerate all of the #CallsOrigin objects in the #CallsProvider and
 * then all of the #CallsCall objects in those #CallsOrigin objects.
 * For any callbacks stored in @params, connect them to the
 * enumerated objects and connect them to any future objects that
 * appear.  Call the "origin-added" callback for a #CallsProvider and
 * "call-added" for a #CallsOrigin if the target objects are
 * enumerated.
 */
void
calls_enumerate (CallsProvider        *provider,
                 CallsEnumerateParams *params)
{
  gboolean add_callback;

  // Connect user's callbacks
  calls_enumerate_params_connect (params, G_OBJECT (provider));

  // We add a callback for ourselves if we have to set callbacks on
  // anything lower in the hierarchy in future
  add_callback =
    calls_enumerate_params_have_callbacks (params, CALLS_TYPE_ORIGIN)
    ||
    calls_enumerate_params_have_callbacks (params, CALLS_TYPE_CALL);

  if (add_callback)
    {
      g_object_ref (params);
      g_signal_connect_data (provider,
                             "origin-added",
                             G_CALLBACK (enum_origin_added_cb),
                             params,
                             (GClosureNotify)g_object_unref,
                             0);

    }

  // We enumerate if we've added callbacks and if there's the specific
  // "origin-added" callback for this level
  if (add_callback ||
      calls_enumerate_params_lookup (params, CALLS_TYPE_PROVIDER,
                                     "origin-added"))
    {
      enum_provider_origins (provider, params);
    }

  calls_enumerate_params_set_enumerating (params, FALSE);
}
