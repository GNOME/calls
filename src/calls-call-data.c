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

#include "calls-call-data.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>

/**
 * SECTION:calls-call-data
 * @short_description: An object to hold both a #CallsCall object and
 * the #CallsParty participating in the call.  These data are passed
 * to both #CallsCallDisplay and #CallsCallSelectorItem so we create a
 * convenient object to keep them together.
 * @Title: CallsCallData
 */

struct _CallsCallData
{
  GObject parent_instance;

  CallsCall *call;
  CallsParty *party;
};

G_DEFINE_TYPE (CallsCallData, calls_call_data, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_CALL,
  PROP_PARTY,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
calls_call_data_init (CallsCallData *self)
{
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsCallData *self = CALLS_CALL_DATA (object);

  switch (property_id) {
  case PROP_CALL:
    g_set_object (&self->call, CALLS_CALL (g_value_get_object (value)));
    break;

  case PROP_PARTY:
    g_set_object (&self->party, CALLS_PARTY (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
get_property (GObject      *object,
              guint         property_id,
              GValue       *value,
              GParamSpec   *pspec)
{
  CallsCallData *self = CALLS_CALL_DATA (object);

  switch (property_id) {
  case PROP_CALL:
    g_value_set_object (value, self->call);
    break;

  case PROP_PARTY:
    g_value_set_object (value, self->party);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
dispose (GObject *object)
{
  CallsCallData *self = CALLS_CALL_DATA (object);

  g_clear_object (&self->call);
  g_clear_object (&self->party);

  G_OBJECT_CLASS (calls_call_data_parent_class)->dispose (object);
}


static void
calls_call_data_class_init (CallsCallDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->get_property = get_property;
  object_class->dispose = dispose;

  props[PROP_CALL] =
    g_param_spec_object ("call",
                         "Call",
                         "The call",
                         CALLS_TYPE_CALL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  props[PROP_PARTY] =
    g_param_spec_object ("party",
                         "Party",
                         "The party participating in the call",
                         CALLS_TYPE_PARTY,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


/**
 * calls_call_data_new:
 *
 * Create a new #CallsCallData object.
 *
 * Returns: the newly created #CallsCallData
 */
CallsCallData *
calls_call_data_new (CallsCall  *call,
                     CallsParty *party)
{
  return g_object_new (CALLS_TYPE_CALL_DATA,
                       "call", call,
                       "party", party,
                       NULL);
}


/**
 * calls_call_data_get_call:
 * @self: a #CallsCallData
 *
 * Get the #CallsCall stored in the object.
 *
 * Returns: the #CallsCall
 */
CallsCall *
calls_call_data_get_call (CallsCallData *self)
{
  g_return_val_if_fail (CALLS_IS_CALL_DATA (self), NULL);
  return self->call;
}


/**
 * calls_call_data_get_party:
 * @self: a #CallsCallData
 *
 * Get the #CallsParty stored in the object.
 *
 * Returns: the #CallsParty
 */
CallsParty *
calls_call_data_get_party (CallsCallData *self)
{
  g_return_val_if_fail (CALLS_IS_CALL_DATA (self), NULL);
  return self->party;
}
