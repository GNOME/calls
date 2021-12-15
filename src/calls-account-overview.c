/*
 * Copyright (C) 2021 Purism SPC
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
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define G_LOG_DOMAIN "CallsAccountOverview"

#include "calls-account.h"
#include "calls-account-overview.h"
#include "calls-account-row.h"
#include "calls-account-provider.h"
#include "calls-manager.h"
#include "calls-in-app-notification.h"


/**
 * Section:calls-account-overview
 * short_description: A #HdyWindow to manage VoIP accounts
 * @Title: CallsAccountOverview
 *
 * This is a #HdyWindow derived window to display and manage the
 * VoIP accounts. Each available #CallsAccount from any #CallsAccountProvider
 * will be listed as a #CallsAccountRow.
 */

typedef enum {
  SHOW_INTRO = 0,
  SHOW_OVERVIEW,
} CallsAccountOverviewState;


struct _CallsAccountOverview {
  HdyWindow                  parent;

  /* UI widgets */
  GtkStack                  *stack;
  GtkWidget                 *intro;
  GtkWidget                 *overview;
  GtkWidget                 *add_btn;
  GtkWidget                 *add_row;

  /* The window where we add the account providers widget */
  GtkWindow                 *account_window;
  GtkWidget                 *current_account_widget;

  /* misc */
  CallsAccountOverviewState  state;
  GList                     *providers;
  CallsInAppNotification    *in_app_notification;
};

G_DEFINE_TYPE (CallsAccountOverview, calls_account_overview, HDY_TYPE_WINDOW)


static void
update_visibility (CallsAccountOverview *self)
{
  g_assert (CALLS_IS_ACCOUNT_OVERVIEW (self));

  switch (self->state) {
  case SHOW_INTRO:
    gtk_stack_set_visible_child (self->stack, self->intro);
    break;

  case SHOW_OVERVIEW:
    gtk_stack_set_visible_child (self->stack, self->overview);
    break;

  default:
    g_warn_if_reached ();
  }
}


static void
update_state (CallsAccountOverview *self)
{
  guint n_origins = 0;

  g_assert (CALLS_IS_ACCOUNT_OVERVIEW (self));

  for (GList *node = self->providers; node != NULL; node = node->next) {
    CallsProvider *provider = CALLS_PROVIDER (node->data);
    GListModel *model = calls_provider_get_origins (provider);

    n_origins += g_list_model_get_n_items (model);
  }

  if (n_origins > 0)
    self->state = SHOW_OVERVIEW;
  else
    self->state = SHOW_INTRO;

  update_visibility (self);
}


static void
attach_account_widget (CallsAccountOverview *self,
                       GtkWidget            *widget)
{
  g_assert (CALLS_IS_ACCOUNT_OVERVIEW (self));
  g_assert (!widget || GTK_IS_WIDGET (widget));

  if (widget == self->current_account_widget)
    return;

  if (self->current_account_widget)
    gtk_container_remove (GTK_CONTAINER (self->account_window),
                          self->current_account_widget);

  self->current_account_widget = widget;
  if (widget)
    gtk_container_add (GTK_CONTAINER (self->account_window), widget);
}


static void
on_add_account_clicked (CallsAccountOverview *self)
{
  CallsAccountProvider *provider;
  GtkWidget *widget;

  /* For now we only have a single AccountProvider */
  provider = CALLS_ACCOUNT_PROVIDER (self->providers->data);

  widget = calls_account_provider_get_account_widget (provider);
  attach_account_widget (self, widget);

  calls_account_provider_add_new_account (provider);

  gtk_window_present (self->account_window);
}


static void
on_edit_account_clicked (CallsAccountRow      *row,
                         CallsAccountProvider *provider,
                         CallsAccount         *account,
                         CallsAccountOverview *self)
{
  GtkWidget *widget;

  widget = calls_account_provider_get_account_widget (provider);
  attach_account_widget (self, widget);

  calls_account_provider_edit_account (provider, account);

  gtk_window_present (self->account_window);
}


static void
on_account_message (CallsAccount         *account,
                    const char           *message,
                    GtkMessageType        message_type,
                    CallsAccountOverview *self)
{
  g_autofree char* notification = NULL;

  g_assert (CALLS_IS_ACCOUNT (account));
  g_assert (CALLS_IS_ACCOUNT_OVERVIEW (self));

  notification = g_strdup_printf ("%s: %s",
                                  calls_account_get_address (account),
                                  message);
  calls_in_app_notification_show (self->in_app_notification, notification);
}


static void
update_account_list (CallsAccountOverview *self)
{
  gboolean removed_all = FALSE;

  g_assert (CALLS_IS_ACCOUNT_OVERVIEW (self));

  /* TODO rework with GTK4 FlattenListModel (to flatten a GListModel of GListModels)
   * in particular we could then connect to the items-changed signal of the flattened list.
   * Always rebuilding the account list is not particularly efficient, but since
   * we're not constantly doing this it's fine for now.
   */
  while (!removed_all) {
    GtkListBoxRow *row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->overview), 0);

    if (row == NULL || row == GTK_LIST_BOX_ROW (self->add_row))
      removed_all = TRUE;
    else
      gtk_container_remove (GTK_CONTAINER (self->overview), GTK_WIDGET (row));
  }

  for (GList *node = self->providers; node != NULL; node = node->next) {
    CallsAccountProvider *provider = CALLS_ACCOUNT_PROVIDER (node->data);
    GListModel *model = calls_provider_get_origins (CALLS_PROVIDER (provider));
    guint n_origins = g_list_model_get_n_items (model);

    for (guint i = 0; i < n_origins; i++) {
      g_autoptr (CallsAccount) account = CALLS_ACCOUNT (g_list_model_get_item (model, i));
      CallsAccountRow *account_row = calls_account_row_new (provider, account);

      g_signal_handlers_disconnect_by_data (account, self);
      g_signal_connect (account, "message",
                        G_CALLBACK (on_account_message),
                        self);

      g_signal_connect (account_row, "edit-clicked",
                        G_CALLBACK (on_edit_account_clicked),
                        self);

      gtk_list_box_insert (GTK_LIST_BOX (self->overview),
                           GTK_WIDGET (account_row),
                           0);
    }
  }
  update_state (self);
}



static void
on_providers_changed (CallsAccountOverview *self)
{
  GList *providers;

  g_clear_pointer (&self->providers, g_list_free);
  providers = calls_manager_get_providers (calls_manager_get_default ());

  for (GList *node = providers; node != NULL; node = node->next) {
    CallsProvider *provider = node->data;

    if (CALLS_IS_ACCOUNT_PROVIDER (provider)) {
      self->providers = g_list_append (self->providers, provider);
      g_signal_connect_swapped (calls_provider_get_origins (provider),
                                "items-changed",
                                G_CALLBACK (update_account_list),
                                self);
      g_signal_connect_swapped (provider, "widget-edit-done",
                                G_CALLBACK (gtk_widget_hide), self->account_window);
    }
  }

  /* Clear any acccount widgets, because they might've gone stale */
  attach_account_widget (self, NULL);
  gtk_widget_hide (GTK_WIDGET (self->account_window));

  update_account_list (self);

  gtk_widget_set_sensitive (self->add_btn, !!self->providers);
}

static void
calls_account_overview_class_init (CallsAccountOverviewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/account-overview.ui");

  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, add_btn);
  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, add_row);

  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, stack);
  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, intro);
  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, overview);

  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, account_window);

  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, in_app_notification);

  gtk_widget_class_bind_template_callback (widget_class, on_add_account_clicked);
}


static void
calls_account_overview_init (CallsAccountOverview *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_swapped (calls_manager_get_default (),
                            "providers-changed",
                            G_CALLBACK (on_providers_changed),
                            self);
  on_providers_changed (self);

  gtk_list_box_insert (GTK_LIST_BOX (self->overview),
                       GTK_WIDGET (self->add_row),
                       -1);
  gtk_window_set_transient_for (self->account_window, GTK_WINDOW (self));

  update_state (self);
}


CallsAccountOverview *
calls_account_overview_new (void)
{
  return g_object_new (CALLS_TYPE_ACCOUNT_OVERVIEW,
                       NULL);
}
