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
#include "calls-call.h"
#include "calls-ui-call-data.h"
#include "calls-util.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib.h>


struct _CallsCallSelectorItem {
  AdwBin     parent_instance;

  CuiCallDisplay *display;

  GtkBox         *main_box;
  GtkLabel       *name;
  GtkLabel       *status;
};

G_DEFINE_TYPE (CallsCallSelectorItem, calls_call_selector_item, ADW_TYPE_BIN);

enum {
  PROP_0,
  PROP_DISPLAY,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
call_state_changed_cb (CallsCallSelectorItem *self,
                       CuiCallState           state)
{
  const char * state_str = cui_call_state_to_string (state);

  gtk_label_set_text (self->status, state_str);
}


static void
set_party (CallsCallSelectorItem *self)
{
  CuiCall *call;
  // FIXME: use AdwAvatar and the contact avatar
  GtkWidget *image = gtk_image_new_from_icon_name ("avatar-default-symbolic");

  gtk_box_append (self->main_box, image);
  gtk_widget_set_visible (image, TRUE);

  call = cui_call_display_get_call (self->display);

  g_object_bind_property (call, "display-name",
                          self->name, "label",
                          G_BINDING_SYNC_CREATE);
}


static void
set_call_display (CallsCallSelectorItem *self, CuiCallDisplay *display)
{
  CuiCall *call;

  g_assert (CALLS_IS_CALL_SELECTOR_ITEM (self));
  g_assert (CUI_IS_CALL_DISPLAY (display));

  call = cui_call_display_get_call (display);
  g_signal_connect_object (call, "notify::state",
                           G_CALLBACK (call_state_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  call_state_changed_cb (self, cui_call_get_state (call));

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
      (self, CUI_CALL_DISPLAY (g_value_get_object (value)));
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
                         CUI_TYPE_CALL_DISPLAY,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/call-selector-item.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsCallSelectorItem, main_box);
  gtk_widget_class_bind_template_child (widget_class, CallsCallSelectorItem, name);
  gtk_widget_class_bind_template_child (widget_class, CallsCallSelectorItem, status);
}


CallsCallSelectorItem *
calls_call_selector_item_new (CuiCallDisplay *display)
{
  g_return_val_if_fail (CUI_IS_CALL_DISPLAY (display), NULL);

  return g_object_new (CALLS_TYPE_CALL_SELECTOR_ITEM,
                       "display", display,
                       NULL);
}


CuiCallDisplay *
calls_call_selector_item_get_display (CallsCallSelectorItem *item)
{
  g_return_val_if_fail (CALLS_IS_CALL_SELECTOR_ITEM (item), NULL);
  return item->display;
}
