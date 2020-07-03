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

#include "calls-main-window.h"
#include "calls-origin.h"
#include "calls-call-holder.h"
#include "calls-call-selector-item.h"
#include "calls-new-call-box.h"
#include "calls-history-box.h"
#include "calls-in-app-notification.h"
#include "calls-manager.h"
#include "config.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>


struct _CallsMainWindow
{
  GtkApplicationWindow parent_instance;

  GListModel *record_store;

  CallsInAppNotification *in_app_notification;

  HdySqueezer *squeezer;
  GtkLabel *title_label;
  HdyViewSwitcher *wide_switcher;
  HdyViewSwitcher *narrow_switcher;
  HdyViewSwitcherBar *switcher_bar;
  GtkStack *main_stack;

  GtkRevealer *permanent_error_revealer;
  GtkLabel *permanent_error_label;

  CallsNewCallBox *new_call;
};

G_DEFINE_TYPE (CallsMainWindow, calls_main_window, GTK_TYPE_APPLICATION_WINDOW);

enum {
  PROP_0,
  PROP_RECORD_STORE,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
about_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  CallsMainWindow *self = user_data;

  const gchar *version = NULL;

  static const gchar *authors[] = {
    "Adrien Plazas <kekun.plazas@laposte.net>",
    "Bob Ham <rah@settrans.net>",
    "Guido Günther <agx@sigxcpu.org>",
    NULL
  };

  static const gchar *artists[] = {
    "Tobias Bernard <tbernard@gnome.org>",
    NULL
  };

  static const gchar *documenters[] = {
    "Heather Ellsworth <heather.ellsworth@puri.sm>",
    NULL
  };

  version = g_str_equal (VCS_TAG, "") ? PACKAGE_VERSION:
                                        PACKAGE_VERSION "-" VCS_TAG;

  /*
   * “program-name” defaults to g_get_application_name().
   * Don’t set it explicitly so that there is one less
   * string to translate.
   */
  gtk_show_about_dialog (GTK_WINDOW (self),
                         "artists", artists,
                         "authors", authors,
                         "copyright", "Copyright © 2018 Purism",
                         "documenters", documenters,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", APP_ID,
                         "translator-credits", _("translator-credits"),
                         "version", version,
                         "website", PACKAGE_URL,
                         NULL);
}


static const GActionEntry window_entries [] =
{
  { "about", about_action },
};


static gboolean
set_switcher_bar_reveal (GBinding     *binding,
                         const GValue *from_value,
                         GValue       *to_value,
                         gpointer      title_label)
{
  g_value_set_boolean (to_value, g_value_get_object (from_value) == title_label);

  return TRUE;
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);

  switch (property_id) {
  case PROP_RECORD_STORE:
    g_set_object (&self->record_store,
                  G_LIST_MODEL (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
state_changed_cb (CallsMainWindow *self,
                  GParamSpec      *pspec,
                  CallsManager    *manager)
{
  const gchar *error = NULL;
  switch (calls_manager_get_state (manager))
    {
    case CALLS_MANAGER_STATE_READY:
      break;

    case CALLS_MANAGER_STATE_NO_ORIGIN:
      error = _("Can't place calls: No voice-capable modem available");
      break;

    case CALLS_MANAGER_STATE_UNKNOWN:
    case CALLS_MANAGER_STATE_NO_PROVIDER:
      error = _("Can't place calls: No backend service");
      break;

    case CALLS_MANAGER_STATE_NO_PLUGIN:
      error = _("Can't place calls: No plugin");
      break;
    }

  gtk_label_set_text (self->permanent_error_label, error);
  gtk_revealer_set_reveal_child (self->permanent_error_revealer, error != NULL);
}


static void
constructed (GObject *object)
{
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);
  GSimpleActionGroup *simple_action_group;
  GtkContainer *main_stack = GTK_CONTAINER (self->main_stack);
  GtkWidget *widget;
  CallsHistoryBox *history;

  // Show errors in in-app-notification
  g_signal_connect_swapped (calls_manager_get_default (),
                            "error",
                            G_CALLBACK (calls_in_app_notification_show),
                            self->in_app_notification);

  // Add new call box
  self->new_call = calls_new_call_box_new ();
  widget = GTK_WIDGET (self->new_call);
  gtk_stack_add_titled (self->main_stack, widget,
                        "dial-pad", _("Dial Pad"));
  gtk_container_child_set (main_stack, widget,
                           "icon-name", "input-dialpad-symbolic",
                           NULL);

  // Add call records
  history = calls_history_box_new (self->record_store);
  widget = GTK_WIDGET (history);
  gtk_stack_add_titled (self->main_stack, widget,
                        "recent", _("Recent"));
  gtk_container_child_set
    (main_stack, widget,
     "icon-name", "document-open-recent-symbolic",
     "position", 0,
     NULL);
  gtk_widget_set_visible (widget, TRUE);
  gtk_stack_set_visible_child_name (self->main_stack, "recent");

  // Add actions
  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   window_entries,
                                   G_N_ELEMENTS (window_entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "win",
                                  G_ACTION_GROUP (simple_action_group));
  g_object_unref (simple_action_group);


  g_object_bind_property_full (self->squeezer,
                               "visible-child",
                               self->switcher_bar,
                               "reveal",
                               G_BINDING_SYNC_CREATE,
                               set_switcher_bar_reveal,
                               NULL,
                               self->title_label,
                               NULL);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "notify::state",
                            G_CALLBACK (state_changed_cb),
                            self);

  state_changed_cb (self, NULL, calls_manager_get_default ());

  G_OBJECT_CLASS (calls_main_window_parent_class)->constructed (object);
}



static void
dispose (GObject *object)
{
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);

  g_clear_object (&self->record_store);

  G_OBJECT_CLASS (calls_main_window_parent_class)->dispose (object);
}


static void
size_allocate (GtkWidget     *widget,
               GtkAllocation *allocation)
{
  CallsMainWindow *self = CALLS_MAIN_WINDOW (widget);

  hdy_squeezer_set_child_enabled (self->squeezer,
                                  GTK_WIDGET (self->wide_switcher),
                                  allocation->width > 600);
  hdy_squeezer_set_child_enabled (self->squeezer,
                                  GTK_WIDGET (self->narrow_switcher),
                                  allocation->width > 400);

  GTK_WIDGET_CLASS (calls_main_window_parent_class)->size_allocate (widget, allocation);
}


static void
calls_main_window_class_init (CallsMainWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;

  props[PROP_RECORD_STORE] =
    g_param_spec_object ("record-store",
                         "Record store",
                         "The store of call records",
                         G_TYPE_LIST_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  widget_class->size_allocate = size_allocate;

  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/main-window.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, in_app_notification);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, squeezer);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, title_label);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, wide_switcher);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, narrow_switcher);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, switcher_bar);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, permanent_error_revealer);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, permanent_error_label);
}


static void
calls_main_window_init (CallsMainWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


CallsMainWindow *
calls_main_window_new (GtkApplication *application,
                       GListModel     *record_store)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
  g_return_val_if_fail (G_IS_LIST_MODEL (record_store), NULL);

  return g_object_new (CALLS_TYPE_MAIN_WINDOW,
                       "application", application,
                       "record-store", record_store,
                       NULL);
}


void
calls_main_window_dial (CallsMainWindow *self,
                        const gchar     *target)
{
  calls_new_call_box_dial (self->new_call, target);
}
