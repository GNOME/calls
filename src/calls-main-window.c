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

  GtkInfoBar *info;
  GtkLabel *info_label;

  GtkStack *main_stack;
  GtkStack *header_bar_stack;
};

G_DEFINE_TYPE (CallsMainWindow, calls_main_window, GTK_TYPE_APPLICATION_WINDOW);

enum {
  PROP_0,
  PROP_PROVIDER,
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


static void
new_call_action (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  CallsMainWindow *self = user_data;

  gtk_stack_set_visible_child_name (self->header_bar_stack, "new-call");
}


static void
back_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  CallsMainWindow *self = user_data;

  gtk_stack_set_visible_child_name (self->header_bar_stack, "history");
}


static const GActionEntry window_entries [] =
{
  { "about", about_action },
  { "new-call", new_call_action },
  { "back", back_action },
};


CallsMainWindow *
calls_main_window_new (GtkApplication *application, CallsProvider *provider)
{
  return g_object_new (CALLS_TYPE_MAIN_WINDOW,
                       "application", application,
                       "provider", provider,
                       NULL);
}


static void
show_message (CallsMainWindow *self, const gchar *text, GtkMessageType type)
{
  gtk_info_bar_set_message_type (self->info, type);
  gtk_label_set_text (self->info_label, text);
  gtk_widget_show (GTK_WIDGET (self->info));
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}


static void
info_response_cb (GtkInfoBar      *infobar,
                  gint             response_id,
                  CallsMainWindow *self)
{
  gtk_widget_hide (GTK_WIDGET (self->info));
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}


static void
add_call (CallsMainWindow *self, CallsCall *call)
{
  g_signal_connect_swapped (call, "message",
                            G_CALLBACK (show_message), self);
}


static void
remove_call (CallsMainWindow *self, CallsCall *call, const gchar *reason)
{
  g_return_if_fail (CALLS_IS_MAIN_WINDOW (self));
  g_return_if_fail (CALLS_IS_CALL (call));

  show_message(self, reason, GTK_MESSAGE_INFO);
}


static void
add_origin_calls (CallsMainWindow *self, CallsOrigin *origin)
{
  GList *calls, *node;

  calls = calls_origin_get_calls (origin);

  for (node = calls; node; node = node->next)
    {
      add_call (self, CALLS_CALL (node->data));
    }

  g_list_free (calls);
}


static void
add_origin (CallsMainWindow *self, CallsOrigin *origin)
{
  g_signal_connect_swapped (origin, "message",
                            G_CALLBACK (show_message), self);
  g_signal_connect_swapped (origin, "call-added",
                            G_CALLBACK (add_call), self);
  g_signal_connect_swapped (origin, "call-removed",
                            G_CALLBACK (remove_call), self);

  add_origin_calls (self, origin);
}


static void
add_provider_origins (CallsMainWindow *self, CallsProvider *provider)
{
  GList *origins, *node;

  origins = calls_provider_get_origins (provider);

  for (node = origins; node; node = node->next)
    {
      add_origin (self, CALLS_ORIGIN (node->data));
    }

  g_list_free (origins);
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
    g_set_object (&self->provider, CALLS_PROVIDER (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
constructed (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (GTK_TYPE_APPLICATION_WINDOW);
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);
  GSimpleActionGroup *simple_action_group;
  CallsNewCallBox *new_call_box;

  /* Set up provider */
  g_signal_connect_swapped (self->provider, "message",
                            G_CALLBACK (show_message), self);
  g_signal_connect_swapped (self->provider, "origin-added",
                            G_CALLBACK (add_origin), self);

  add_provider_origins (self, self->provider);

  /* Add new call box */
  new_call_box = calls_new_call_box_new (self->provider);
  gtk_stack_add_titled (self->main_stack, GTK_WIDGET (new_call_box),
                        "new-call", _("New call"));

  /* Add actions */
  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   window_entries,
                                   G_N_ELEMENTS (window_entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "win",
                                  G_ACTION_GROUP (simple_action_group));
  g_object_unref (simple_action_group);

  // FIXME: if (no history)
  {
    new_call_action (NULL, NULL, self);
  }

  parent_class->constructed (object);
}


static void
calls_main_window_init (CallsMainWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


static void
dispose (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (GTK_TYPE_APPLICATION_WINDOW);
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);

  g_clear_object (&self->provider);

  parent_class->dispose (object);
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

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/main-window.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, info);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, info_label);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, header_bar_stack);
  gtk_widget_class_bind_template_callback (widget_class, info_response_cb);
}
