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
 * Authors:
 *     Bob Ham <bob.ham@puri.sm>
 *     Adrien Plazas <adrien.plazas@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define G_LOG_DOMAIN "CallsCallWindow"

#include "calls-call-window.h"
#include "calls-origin.h"
#include "calls-call-selector-item.h"
#include "calls-new-call-box.h"
#include "calls-in-app-notification.h"
#include "calls-manager.h"
#include "calls-ui-call-data.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <handy.h>

#define CALLS_WINDOW_HIDE_DELAY 3

struct _CallsCallWindow
{
  GtkApplicationWindow parent_instance;

  GListStore *calls;

  CallsInAppNotification *in_app_notification;

  GtkStack *main_stack;
  GtkStack *header_bar_stack;
  GtkButton *show_calls;
  GtkStack *call_stack;
  GtkFlowBox *call_selector;

  guint inhibit_cookie;
  guint hideout_id;

};

G_DEFINE_TYPE (CallsCallWindow, calls_call_window, GTK_TYPE_APPLICATION_WINDOW);


static void
session_inhibit (CallsCallWindow *self, gboolean inhibit)
{
  if (inhibit) {
    if (self->inhibit_cookie == 0)
      self->inhibit_cookie =
        gtk_application_inhibit (gtk_window_get_application (GTK_WINDOW (self)),
                                 GTK_WINDOW (self),
                                 GTK_APPLICATION_INHIBIT_SUSPEND |
                                 GTK_APPLICATION_INHIBIT_IDLE |
                                 GTK_APPLICATION_INHIBIT_LOGOUT |
                                 GTK_APPLICATION_INHIBIT_SWITCH,
                                 "call active");
  } else {
    if (self->inhibit_cookie != 0)
      gtk_application_uninhibit (gtk_window_get_application (GTK_WINDOW (self)),
                                 self->inhibit_cookie);
    self->inhibit_cookie = 0;
  }

}


static gboolean
on_delayed_window_hide (gpointer user_data)
{
  CallsCallWindow *self = user_data;
  g_assert (CALLS_IS_CALL_WINDOW (self));

  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);

  gtk_stack_set_visible_child_name (self->main_stack, "calls");

  self->hideout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
update_visibility (CallsCallWindow *self)
{
  guint calls = g_list_model_get_n_items (G_LIST_MODEL (self->calls));

  if (calls == 0) {
    self->hideout_id =
      g_timeout_add_seconds (CALLS_WINDOW_HIDE_DELAY,
                             G_SOURCE_FUNC (on_delayed_window_hide),
                             self);
  } else {
    g_clear_handle_id (&self->hideout_id, g_source_remove);

    gtk_widget_set_visible (GTK_WIDGET (self), TRUE);

    if (calls == 1)
      gtk_stack_set_visible_child_name (self->main_stack, "active-call");
  }
  gtk_widget_set_sensitive (GTK_WIDGET (self->show_calls), calls > 1);

  session_inhibit (self, !!calls);
}


static GtkWidget *
calls_create_widget_cb (CallsCallSelectorItem *item,
                        gpointer               user_data)
{
  return GTK_WIDGET (item);
}


static void
new_call_submitted_cb (CallsCallWindow *self,
                       CallsOrigin     *origin,
                       const gchar     *id,
                       CallsNewCallBox *new_call_box)
{
  g_assert (CALLS_IS_CALL_WINDOW (self));

  calls_origin_dial (origin, id);
}


static void
set_focus (CallsCallWindow *self,
           CuiCallDisplay  *display)
{
  gtk_stack_set_visible_child_name (self->main_stack, "active-call");
  gtk_stack_set_visible_child (self->call_stack, GTK_WIDGET (display));
}


static void
show_calls_clicked_cb (GtkButton       *button,
                       CallsCallWindow *self)
{
  gtk_stack_set_visible_child_name (self->main_stack, "calls");
}


static void
call_selector_child_activated_cb (GtkFlowBox      *box,
                                  GtkFlowBoxChild *child,
                                  CallsCallWindow *self)
{
  GtkWidget *widget = gtk_bin_get_child (GTK_BIN (child));
  CallsCallSelectorItem *item = CALLS_CALL_SELECTOR_ITEM (widget);
  CuiCallDisplay *display = calls_call_selector_item_get_display (item);

  set_focus (self, display);
}


static void
add_call (CallsCallWindow *self,
          CallsCall       *call)
{
  CallsUiCallData *call_data;
  CuiCallDisplay *display;
  CallsCallSelectorItem *item;

  g_assert (CALLS_IS_CALL_WINDOW (self));
  g_assert (CALLS_IS_CALL (call));

  call_data = calls_ui_call_data_new (call);
  display = cui_call_display_new (CUI_CALL (call_data));
  item = calls_call_selector_item_new (display);
  gtk_stack_add_named (self->call_stack, GTK_WIDGET (display),
                       calls_call_get_id (call));

  g_list_store_append (self->calls, item);

  update_visibility (self);
  set_focus (self, display);
}

struct DisplayData
{
  GtkStack *call_stack;
  CuiCallDisplay *display;
};

static gboolean
on_remove_delayed (gpointer user_data)
{
  struct DisplayData *display_data = user_data;

  gtk_container_remove (GTK_CONTAINER (display_data->call_stack),
                        GTK_WIDGET (display_data->display));

  g_free (display_data);
  return G_SOURCE_REMOVE;
}

static void
remove_call (CallsCallWindow *self,
             CallsCall       *call,
             const gchar     *reason)
{
  guint n_calls;

  g_assert (CALLS_IS_CALL_WINDOW (self));
  g_assert (CALLS_IS_CALL (call));

  n_calls = g_list_model_get_n_items (G_LIST_MODEL (self->calls));
  for (guint i = 0; i < n_calls; i++) {
    g_autoptr (CallsCallSelectorItem) item =
      g_list_model_get_item (G_LIST_MODEL (self->calls), i);
    CuiCallDisplay *display = calls_call_selector_item_get_display (item);
    CallsUiCallData *call_data =
      CALLS_UI_CALL_DATA (cui_call_display_get_call (display));

    if (calls_ui_call_data_get_call (call_data) == call) {
      struct DisplayData *display_data = g_new0 (struct DisplayData, 1);

      g_list_store_remove (self->calls, i);
      display_data->call_stack = self->call_stack;
      display_data->display = display;
      g_timeout_add_seconds (CALLS_WINDOW_HIDE_DELAY,
                             G_SOURCE_FUNC (on_remove_delayed),
                             display_data);

      break;
    }
  }

  update_visibility (self);
}


static void
remove_calls (CallsCallWindow *self)
{
  GList *children, *child;

  /* Safely remove the call stack's children. */
  children = gtk_container_get_children (GTK_CONTAINER (self->call_stack));
  for (child = children; child != NULL; child = child->next)
    gtk_container_remove (GTK_CONTAINER (self->call_stack),
                          GTK_WIDGET (child->data));
  g_list_free (children);

  g_list_store_remove_all (self->calls);

  update_visibility (self);
}


static void
constructed (GObject *object)
{
  CallsCallWindow *self = CALLS_CALL_WINDOW (object);

  gtk_flow_box_bind_model (self->call_selector,
                           G_LIST_MODEL (self->calls),
                           (GtkFlowBoxCreateWidgetFunc) calls_create_widget_cb,
                           NULL, NULL);

  update_visibility (self);

  G_OBJECT_CLASS (calls_call_window_parent_class)->constructed (object);
}


static void
calls_call_window_init (CallsCallWindow *self)
{
  g_autoptr (GList) calls = NULL;
  GList *c;
  gtk_widget_init_template (GTK_WIDGET (self));

  self->calls = g_list_store_new (CALLS_TYPE_CALL_SELECTOR_ITEM);

  // Show errors in in-app-notification
  g_signal_connect_swapped (calls_manager_get_default (),
                            "message",
                            G_CALLBACK (calls_in_app_notification_show),
                            self->in_app_notification);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "call-add",
                            G_CALLBACK (add_call),
                            self);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "call-remove",
                            G_CALLBACK (remove_call),
                            self);

  calls = calls_manager_get_calls (calls_manager_get_default ());
  for (c = calls; c != NULL; c = c->next) {
    add_call (self, c->data);
  }
}


static void
dispose (GObject *object)
{
  CallsCallWindow *self = CALLS_CALL_WINDOW (object);

  if (self->calls)
    remove_calls (self);

  g_clear_object (&self->calls);

  G_OBJECT_CLASS (calls_call_window_parent_class)->dispose (object);
}


static void
calls_call_window_class_init (CallsCallWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/call-window.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, in_app_notification);
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, header_bar_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, show_calls);
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, call_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, call_selector);
  gtk_widget_class_bind_template_callback (widget_class, call_selector_child_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_calls_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, new_call_submitted_cb);
}


CallsCallWindow *
calls_call_window_new (GtkApplication *application)
{
  return g_object_new (CALLS_TYPE_CALL_WINDOW,
                       "application", application,
                       NULL);
}

#undef CALLS_WINDOW_HIDE_DELAY
