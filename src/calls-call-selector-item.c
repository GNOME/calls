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

#include "calls-call-selector-item.h"
#include "calls-call-display.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib.h>


struct _CallsCallSelectorItem
{
  GtkEventBox parent_instance;

  CallsCallDisplay *display;
  CallsBestMatch *contact;

  GtkBox *main_box;
  GtkLabel *name;
  GtkLabel *status;
};

G_DEFINE_TYPE (CallsCallSelectorItem, calls_call_selector_item, GTK_TYPE_EVENT_BOX);

enum {
  PROP_0,
  PROP_DISPLAY,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
call_state_changed_cb (CallsCallSelectorItem *self,
                       CallsCallState         state)
{
  GString *state_str = g_string_new("");
  calls_call_state_to_string (state_str, state);
  gtk_label_set_text (self->status, state_str->str);
  g_string_free (state_str, TRUE);
}


CallsCallSelectorItem *
calls_call_selector_item_new (CallsCallDisplay *display)
{
  g_return_val_if_fail (CALLS_IS_CALL_DISPLAY (display), NULL);

  return g_object_new (CALLS_TYPE_CALL_SELECTOR_ITEM,
                       "display", display,
                       NULL);
}


CallsCallDisplay *
calls_call_selector_item_get_display (CallsCallSelectorItem *item)
{
  g_return_val_if_fail (CALLS_IS_CALL_SELECTOR_ITEM (item), NULL);
  return item->display;
}

static void
set_party (CallsCallSelectorItem *self)
{
  // FIXME: use HdyAvatar and the contact avatar
  GtkWidget *image = gtk_image_new_from_icon_name ("avatar-default-symbolic", GTK_ICON_SIZE_DIALOG);

  gtk_box_pack_start (self->main_box, image, TRUE, TRUE, 0);
  gtk_widget_show (image);

  self->contact = calls_call_get_contact (calls_call_display_get_call (self->display));

  g_object_bind_property (self->contact, "name",
                          self->name, "label",
                          G_BINDING_SYNC_CREATE);
}


static void
set_call_display (CallsCallSelectorItem *self, CallsCallDisplay *display)
{
  CallsCall *call = NULL;

  g_return_if_fail (CALLS_IS_CALL_SELECTOR_ITEM (self));
  g_return_if_fail (CALLS_IS_CALL_DISPLAY (display));

  call = calls_call_display_get_call (display);
  g_signal_connect_object (call, "state-changed",
                           G_CALLBACK (call_state_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  call_state_changed_cb (self, calls_call_get_state (call));

  g_set_object (&self->display, display);
  set_party (self);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsCallSelectorItem *self = CALLS_CALL_SELECTOR_ITEM (object);

  switch (property_id) {
  case PROP_DISPLAY:
    set_call_display
      (self, CALLS_CALL_DISPLAY (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_call_selector_item_init (CallsCallSelectorItem *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


static void
dispose (GObject *object)
{
  CallsCallSelectorItem *self = CALLS_CALL_SELECTOR_ITEM (object);

  g_clear_object (&self->display);
  g_clear_object (&self->contact);

  G_OBJECT_CLASS (calls_call_selector_item_parent_class)->dispose (object);
}


static void
calls_call_selector_item_class_init (CallsCallSelectorItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = set_property;
  object_class->dispose = dispose;

  props[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         "Call display",
                         "The display for this call",
                         CALLS_TYPE_CALL_DISPLAY,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
   
  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/call-selector-item.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsCallSelectorItem, main_box);
  gtk_widget_class_bind_template_child (widget_class, CallsCallSelectorItem, name);
  gtk_widget_class_bind_template_child (widget_class, CallsCallSelectorItem, status);
}
