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
#include "calls-manager.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>

/**
 * SECTION:calls-call-holder
 * @short_description: An object to hold both a #CallsCall object and
 * data about it and widgets
 * @Title: CallsCallHolder
 */

struct _CallsCallHolder
{
  GObject parent_instance;

  CallsCallData *data;
  CallsCallDisplay *display;
  CallsCallSelectorItem *selector_item;
};

G_DEFINE_TYPE (CallsCallHolder, calls_call_holder, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_CALL,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


CallsCallHolder *
calls_call_holder_new (CallsCall *call)
{
  return g_object_new (CALLS_TYPE_CALL_HOLDER,
                       "call", call,
                       NULL);
}


CallsCallData *
calls_call_holder_get_data (CallsCallHolder *holder)
{
  g_return_val_if_fail (CALLS_IS_CALL_HOLDER (holder), NULL);
  return holder->data;
}


CallsCallDisplay *
calls_call_holder_get_display (CallsCallHolder *holder)
{
  g_return_val_if_fail (CALLS_IS_CALL_HOLDER (holder), NULL);
  return holder->display;
}


CallsCallSelectorItem *
calls_call_holder_get_selector_item (CallsCallHolder *holder)
{
  g_return_val_if_fail (CALLS_IS_CALL_HOLDER (holder), NULL);
  return holder->selector_item;
}


static void
set_call (CallsCallHolder *self, CallsCall *call)
{
  CallsParty *party = calls_party_new (calls_manager_get_contact_name (call),
                                       calls_call_get_number (call));

  self->data = calls_call_data_new (call, party);
  g_object_unref (party);

  self->display = calls_call_display_new (self->data);
  g_object_ref_sink (self->display);

  self->selector_item =
    calls_call_selector_item_new (self);
  g_object_ref_sink (self->selector_item);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsCallHolder *self = CALLS_CALL_HOLDER (object);

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
calls_call_holder_init (CallsCallHolder *self)
{
}


static void
dispose (GObject *object)
{
  CallsCallHolder *self = CALLS_CALL_HOLDER (object);

  g_clear_object (&self->selector_item);
  g_clear_object (&self->display);
  g_clear_object (&self->data);

  G_OBJECT_CLASS (calls_call_holder_parent_class)->dispose (object);
}


static void
calls_call_holder_class_init (CallsCallHolderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->dispose = dispose;

  props[PROP_CALL] =
    g_param_spec_object ("call",
                         "Call",
                         "The call to hold",
                         CALLS_TYPE_CALL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}
