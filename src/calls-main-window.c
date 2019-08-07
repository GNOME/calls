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
#include "calls-enumerate.h"
#include "config.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>


struct _CallsMainWindow
{
  GtkApplicationWindow parent_instance;

  CallsProvider *provider;
  GListModel *record_store;

  GtkRevealer *info_revealer;
  guint info_timeout;
  GtkInfoBar *info;
  GtkLabel *info_label;

  HdySqueezer *squeezer;
  GtkLabel *title_label;
  HdyViewSwitcher *wide_switcher;
  HdyViewSwitcher *narrow_switcher;
  HdyViewSwitcherBar *switcher_bar;
  GtkStack *main_stack;

  CallsNewCallBox *new_call;
};

G_DEFINE_TYPE (CallsMainWindow, calls_main_window, GTK_TYPE_APPLICATION_WINDOW);

enum {
  PROP_0,
  PROP_PROVIDER,
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

  gtk_show_about_dialog (GTK_WINDOW (self),
                         "artists", artists,
                         "authors", authors,
                         "copyright", "Copyright © 2018 Purism",
                         "documenters", documenters,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", APP_ID,
                         "program-name", _("Calls"),
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
show_message_timeout_cb (CallsMainWindow *self)
{
  gtk_revealer_set_reveal_child (self->info_revealer, FALSE);
  self->info_timeout = 0;
  return FALSE;
}


static void
show_message (CallsMainWindow *self, const gchar *text, GtkMessageType type)
{
  gtk_info_bar_set_message_type (self->info, type);
  gtk_label_set_text (self->info_label, text);
  gtk_revealer_set_reveal_child (self->info_revealer, TRUE);

  if (self->info_timeout)
    {
      g_source_remove (self->info_timeout);
    }
  self->info_timeout = g_timeout_add_seconds
    (3,
     (GSourceFunc)show_message_timeout_cb,
     self);
}


static inline void
stop_info_timeout (CallsMainWindow *self)
{
  if (self->info_timeout)
    {
      g_source_remove (self->info_timeout);
      self->info_timeout = 0;
    }
}


static void
info_response_cb (GtkInfoBar      *infobar,
                  gint             response_id,
                  CallsMainWindow *self)
{
  stop_info_timeout (self);
  gtk_revealer_set_reveal_child (self->info_revealer, FALSE);
}


static void
call_removed_cb (CallsMainWindow *self, CallsCall *call, const gchar *reason)
{
  show_message(self, reason, GTK_MESSAGE_INFO);
}


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
  case PROP_PROVIDER:
    g_set_object (&self->provider,
                  CALLS_PROVIDER (g_value_get_object (value)));
    break;

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
set_up_provider (CallsMainWindow *self)
{
  const GType msg_obj_types[3] =
    {
      CALLS_TYPE_PROVIDER,
      CALLS_TYPE_ORIGIN,
      CALLS_TYPE_CALL
    };
  CallsEnumerateParams *params;
  unsigned i;

  params = calls_enumerate_params_new (self);

  for (i = 0; i < 3; ++i)
    {
      calls_enumerate_params_add
        (params, msg_obj_types[i], "message", G_CALLBACK (show_message));
    }

  calls_enumerate_params_add
    (params, CALLS_TYPE_ORIGIN, "call-removed", G_CALLBACK (call_removed_cb));

  calls_enumerate (self->provider, params);

  g_object_unref (params);
}


static void
constructed (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);
  GSimpleActionGroup *simple_action_group;
  GtkContainer *main_stack = GTK_CONTAINER (self->main_stack);
  GtkWidget *widget;
  CallsHistoryBox *history;

  set_up_provider (self);

  // Add new call box
  self->new_call = calls_new_call_box_new (self->provider);
  widget = GTK_WIDGET (self->new_call);
  gtk_stack_add_titled (self->main_stack, widget,
                        "dial-pad", _("Dial Pad"));
  gtk_container_child_set (main_stack, widget,
                           "icon-name", "input-dialpad-symbolic",
                           NULL);

  // Add call records
  history = calls_history_box_new (self->record_store,
                                   self->new_call);
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

  parent_class->constructed (object);
}



static void
dispose (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);

  stop_info_timeout (self);
  g_clear_object (&self->record_store);
  g_clear_object (&self->provider);

  parent_class->dispose (object);
}


static void
size_allocate (GtkWidget     *widget,
               GtkAllocation *allocation)
{
  CallsMainWindow *self = CALLS_MAIN_WINDOW (widget);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (calls_main_window_parent_class);

  hdy_squeezer_set_child_enabled (self->squeezer,
                                  GTK_WIDGET (self->wide_switcher),
                                  allocation->width > 600);
  hdy_squeezer_set_child_enabled (self->squeezer,
                                  GTK_WIDGET (self->narrow_switcher),
                                  allocation->width > 400);

  widget_class->size_allocate (widget, allocation);
}


static void
calls_main_window_class_init (CallsMainWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;

  props[PROP_PROVIDER] =
    g_param_spec_object ("provider",
                         _("Provider"),
                         _("An object implementing low-level call-making functionality"),
                         CALLS_TYPE_PROVIDER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  props[PROP_RECORD_STORE] =
    g_param_spec_object ("record-store",
                         _("Record store"),
                         _("The store of call records"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  widget_class->size_allocate = size_allocate;

  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/main-window.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, info_revealer);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, info);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, info_label);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, squeezer);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, title_label);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, wide_switcher);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, narrow_switcher);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, switcher_bar);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, main_stack);
  gtk_widget_class_bind_template_callback (widget_class, info_response_cb);
}


static void
calls_main_window_init (CallsMainWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


CallsMainWindow *
calls_main_window_new (GtkApplication *application,
                       CallsProvider  *provider,
                       GListModel     *record_store)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
  g_return_val_if_fail (CALLS_IS_PROVIDER (provider), NULL);
  g_return_val_if_fail (G_IS_LIST_MODEL (record_store), NULL);

  return g_object_new (CALLS_TYPE_MAIN_WINDOW,
                       "application", application,
                       "provider", provider,
                       "record-store", record_store,
                       NULL);
}


void
calls_main_window_dial (CallsMainWindow *self,
                        const gchar     *target)
{
  calls_new_call_box_dial (self->new_call, target);
}
