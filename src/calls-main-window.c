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
#include "calls-account-overview.h"
#include "calls-origin.h"
#include "calls-ussd.h"
#include "calls-new-call-box.h"
#include "calls-contacts-box.h"
#include "calls-history-box.h"
#include "calls-in-app-notification.h"
#include "calls-manager.h"
#include "calls-config.h"
#include "calls-util.h"
#include "version.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <handy.h>


struct _CallsMainWindow {
  HdyApplicationWindow    parent_instance;

  GListModel             *record_store;

  CallsInAppNotification *in_app_notification;

  HdyViewSwitcherTitle   *title_switcher;
  GtkStack               *main_stack;

  GtkRevealer            *permanent_error_revealer;
  GtkLabel               *permanent_error_label;

  CallsAccountOverview   *account_overview;
  CallsNewCallBox        *new_call;

  GtkDialog              *ussd_dialog;
  GtkStack               *ussd_stack;
  GtkSpinner             *ussd_spinner;
  GtkBox                 *ussd_content;
  GtkLabel               *ussd_label;
  GtkEntry               *ussd_entry;
  GtkButton              *ussd_close_button;
  GtkButton              *ussd_cancel_button;
  GtkButton              *ussd_reply_button;
};

G_DEFINE_TYPE (CallsMainWindow, calls_main_window, HDY_TYPE_APPLICATION_WINDOW);

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
    "Julian Sparber <julian@sparber.net>",
    "Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>",
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

  version = g_str_equal (VCS_TAG, "") ?
            PACKAGE_VERSION : PACKAGE_VERSION "-" VCS_TAG;

  /*
   * “program-name” defaults to g_get_application_name().
   * Don’t set it explicitly so that there is one less
   * string to translate.
   */
  gtk_show_about_dialog (GTK_WINDOW (self),
                         "artists", artists,
                         "authors", authors,
                         "copyright", "Copyright © 2018 - 2022 Purism",
                         "documenters", documenters,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", APP_ID,
                         "translator-credits", _("translator-credits"),
                         "version", version,
                         "website", PACKAGE_URL,
                         NULL);
}


static const GActionEntry window_entries[] =
{
  { "about", about_action },
};


static void
window_update_ussd_state (CallsMainWindow *self,
                          CallsUssd       *ussd)
{
  CallsUssdState state;

  g_assert (CALLS_IS_MAIN_WINDOW (self));
  g_assert (CALLS_IS_USSD (ussd));

  state = calls_ussd_get_state (ussd);

  /* If state changed in an active USSD session, don't update */
  if (state == CALLS_USSD_STATE_ACTIVE &&
      gtk_widget_get_visible (GTK_WIDGET (self->ussd_reply_button)))
    return;

  gtk_widget_set_visible (GTK_WIDGET (self->ussd_reply_button),
                          state == CALLS_USSD_STATE_USER_RESPONSE);
  gtk_widget_set_visible (GTK_WIDGET (self->ussd_entry),
                          state == CALLS_USSD_STATE_USER_RESPONSE);

  if (state == CALLS_USSD_STATE_USER_RESPONSE ||
      state == CALLS_USSD_STATE_ACTIVE)
    gtk_widget_show (GTK_WIDGET (self->ussd_cancel_button));
  else
    gtk_widget_show (GTK_WIDGET (self->ussd_close_button));
}

static void
window_ussd_added_cb (CallsMainWindow *self,
                      CallsUssd       *ussd,
                      char            *response)
{
  g_assert (CALLS_IS_MAIN_WINDOW (self));
  g_assert (CALLS_IS_USSD (ussd));

  if (!response || !*response)
    return;

  gtk_label_set_label (self->ussd_label, response);
  g_object_set_data_full (G_OBJECT (self->ussd_dialog), "ussd",
                          g_object_ref (ussd), g_object_unref);
  window_update_ussd_state (self, ussd);
  gtk_window_present (GTK_WINDOW (self->ussd_dialog));
}

static void
window_ussd_cancel_clicked_cb (CallsMainWindow *self)
{
  CallsUssd *ussd;

  g_assert (CALLS_IS_MAIN_WINDOW (self));

  ussd = g_object_get_data (G_OBJECT (self->ussd_dialog), "ussd");

  if (ussd)
    calls_ussd_cancel_async (ussd, NULL, NULL, NULL);

  gtk_window_close (GTK_WINDOW (self->ussd_dialog));
}

static void
window_ussd_entry_changed_cb (CallsMainWindow *self,
                              GtkEntry        *entry)
{
  const char *text;
  gboolean allow_send;

  g_assert (CALLS_IS_MAIN_WINDOW (self));
  g_assert (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);
  allow_send = text && *text;

  gtk_widget_set_sensitive (GTK_WIDGET (self->ussd_reply_button), allow_send);
}

static void
window_ussd_respond_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *response = NULL;
  CallsMainWindow *self = user_data;
  CallsUssd *ussd;

  ussd = g_object_get_data (G_OBJECT (self->ussd_dialog), "ussd");
  response = calls_ussd_respond_finish (ussd, result, &error);

  if (error) {
    gtk_dialog_response (self->ussd_dialog, GTK_RESPONSE_CLOSE);
    g_warning ("USSD Error: %s", error->message);
    return;
  }

  if (response && *response) {
    window_update_ussd_state (self, ussd);
    gtk_label_set_text (self->ussd_label, response);
  }

  gtk_spinner_stop (self->ussd_spinner);
  gtk_stack_set_visible_child (self->ussd_stack, GTK_WIDGET (self->ussd_content));
}

static void
window_ussd_reply_clicked_cb (CallsMainWindow *self)
{
  CallsUssd *ussd;
  g_autofree char *response = NULL;

  g_assert (CALLS_IS_MAIN_WINDOW (self));

  ussd = g_object_get_data (G_OBJECT (self->ussd_dialog), "ussd");
  g_assert (CALLS_IS_USSD (ussd));

  response = g_strdup (gtk_entry_get_text (self->ussd_entry));
  gtk_entry_set_text (self->ussd_entry, "");
  calls_ussd_respond_async (ussd, response, NULL,
                            window_ussd_respond_cb, self);
}

static void
main_window_ussd_send_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *response = NULL;
  CallsMainWindow *self = user_data;
  CallsUssd *ussd;

  response = calls_new_call_box_send_ussd_finish (self->new_call, result, &error);
  ussd = g_task_get_task_data (G_TASK (result));

  if (error) {
    gtk_dialog_response (self->ussd_dialog, GTK_RESPONSE_CLOSE);
    g_warning ("USSD Error: %s", error->message);
    return;
  }

  g_object_set_data_full (G_OBJECT (self->ussd_dialog), "ussd",
                          g_object_ref (ussd), g_object_unref);
  window_update_ussd_state (self, ussd);
  gtk_label_set_text (self->ussd_label, response);
  gtk_spinner_stop (self->ussd_spinner);
  gtk_stack_set_visible_child (self->ussd_stack, GTK_WIDGET (self->ussd_content));
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
  CallsManagerFlags state_flags = calls_manager_get_state_flags (manager);

  if (!(state_flags & CALLS_MANAGER_FLAGS_HAS_CELLULAR_MODEM) &&
      !(state_flags & CALLS_MANAGER_FLAGS_HAS_VOIP_ACCOUNT))
    error = _("Can't place calls: No modem or VoIP account available");
  else if (state_flags & CALLS_MANAGER_FLAGS_UNKNOWN)
    error = _("Can't place calls: No plugin loaded");

  gtk_label_set_text (self->permanent_error_label, error);
  gtk_revealer_set_reveal_child (self->permanent_error_revealer, !!error);
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
  g_signal_connect_object (calls_manager_get_default (),
                           "message",
                           G_CALLBACK (calls_in_app_notification_show),
                           self->in_app_notification,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (calls_manager_get_default (),
                           "ussd-added",
                           G_CALLBACK (window_ussd_added_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (calls_manager_get_default (),
                           "ussd-state-changed",
                           G_CALLBACK (window_update_ussd_state),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_window_set_transient_for (GTK_WINDOW (self->ussd_dialog), GTK_WINDOW (self));

  // Add contacs box
  widget = calls_contacts_box_new ();
  gtk_stack_add_titled (self->main_stack, widget,
                        "contacts", _("Contacts"));
  gtk_container_child_set (main_stack, widget,
                           "icon-name", "system-users-symbolic",
                           NULL);
  gtk_widget_set_visible (widget, TRUE);

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
                        /* Recent as in "Recent calls" (the call history) */
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


  g_signal_connect_object (calls_manager_get_default (),
                           "notify::state",
                           G_CALLBACK (state_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  state_changed_cb (self, NULL, calls_manager_get_default ());

  G_OBJECT_CLASS (calls_main_window_parent_class)->constructed (object);
}



static void
dispose (GObject *object)
{
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);

  g_clear_object (&self->record_store);
  g_clear_object (&self->account_overview);

  G_OBJECT_CLASS (calls_main_window_parent_class)->dispose (object);
}


static void
size_allocate (GtkWidget     *widget,
               GtkAllocation *allocation)
{
  CallsMainWindow *self = CALLS_MAIN_WINDOW (widget);

  hdy_view_switcher_title_set_view_switcher_enabled (self->title_switcher,
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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/main-window.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, in_app_notification);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, title_switcher);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, permanent_error_revealer);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, permanent_error_label);

  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, ussd_dialog);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, ussd_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, ussd_spinner);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, ussd_content);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, ussd_label);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, ussd_entry);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, ussd_close_button);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, ussd_cancel_button);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, ussd_reply_button);

  gtk_widget_class_bind_template_callback (widget_class, window_ussd_cancel_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, window_ussd_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, window_ussd_reply_clicked_cb);
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
  if (calls_number_is_ussd (target)) {
    gtk_widget_hide (GTK_WIDGET (self->ussd_cancel_button));
    gtk_widget_hide (GTK_WIDGET (self->ussd_reply_button));
    gtk_stack_set_visible_child (self->ussd_stack, GTK_WIDGET (self->ussd_spinner));
    gtk_spinner_start (self->ussd_spinner);

    calls_new_call_box_send_ussd_async (self->new_call, target, NULL,
                                        main_window_ussd_send_cb, self);

    gtk_window_present (GTK_WINDOW (self->ussd_dialog));
  } else {
    calls_new_call_box_dial (self->new_call, target);
  }
}

void
calls_main_window_show_accounts_overview (CallsMainWindow *self)
{
  g_return_if_fail (CALLS_IS_MAIN_WINDOW (self));

  if (self->account_overview == NULL) {
    self->account_overview = calls_account_overview_new ();
    gtk_window_set_transient_for (GTK_WINDOW (self->account_overview),
                                  GTK_WINDOW (self));
  }

  gtk_window_present (GTK_WINDOW (self->account_overview));
}
