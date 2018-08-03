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

#include "calls-call-holder.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>

/**
 * SECTION:calls-ofono-object
 * @short_description: Object to encapsulate common 
 * @Title: CallsOfonoObject
 */

struct _CallsOfonoObject
{
  GObject parent_instance;

};

G_DEFINE_TYPE (CallsOfonoObject, calls_ofono_object, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_CALL,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


CallsOfonoObject *
calls_ofono_object_new (CallsCall *call)
{
  return g_object_new (CALLS_TYPE_OFONO_OBJECT,
                       "call", call,
                       NULL);
}


CallsCallData *
calls_ofono_object_get_data (CallsOfonoObject *holder)
{
  g_return_val_if_fail (CALLS_IS_OFONO_OBJECT (holder), NULL);
  return holder->data;
}


CallsCallDisplay *
calls_ofono_object_get_display (CallsOfonoObject *holder)
{
  g_return_val_if_fail (CALLS_IS_OFONO_OBJECT (holder), NULL);
  return holder->display;
}


CallsCallSelectorItem *
calls_ofono_object_get_selector_item (CallsOfonoObject *holder)
{
  g_return_val_if_fail (CALLS_IS_OFONO_OBJECT (holder), NULL);
  return holder->selector_item;
}


static void
set_call (CallsOfonoObject *self, CallsCall *call)
{
  CallsParty *party
    = calls_party_new (NULL, calls_call_get_identifier (call));

  self->data = calls_call_data_new (call, party);
  g_object_unref (party);

  self->display = calls_call_display_new (self->data);
  self->selector_item =
    calls_call_selector_item_new (self);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsOfonoObject *self = CALLS_OFONO_OBJECT (object);

  switch (property_id) {
  case PROP_CALL:
    set_call (self, CALLS_CALL (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_ofono_object_init (CallsOfonoObject *self)
{
}


static void
dispose (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsOfonoObject *self = CALLS_OFONO_OBJECT (object);

  g_clear_object (&self->selector_item);
  g_clear_object (&self->display);
  g_clear_object (&self->data);
  
  parent_class->dispose (object);
}


static void
calls_ofono_object_class_init (CallsOfonoObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->dispose = dispose;

  props[PROP_CALL] =
    g_param_spec_object ("call",
                         _("Call"),
                         _("The call to hold"),
                         CALLS_TYPE_CALL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}
