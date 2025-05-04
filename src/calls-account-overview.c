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

#include "calls-config.h"

#include "calls-account.h"
#include "calls-account-overview.h"
#include "calls-account-row.h"
#include "calls-account-provider.h"
#include "calls-manager.h"
#include "calls-in-app-notification.h"
#include "calls-plugin-manager.h"
#include "calls-util.h"

#include <glib/gi18n-lib.h>


/**
 * Section:calls-account-overview
 * short_description: A #AdwWindow to manage VoIP accounts
 * @Title: CallsAccountOverview
 *
 * This is a #AdwWindow derived window to display and manage the
 * VoIP accounts. Each available #CallsAccount from any #CallsAccountProvider
 * will be listed as a #CallsAccountRow.
 */

typedef enum {
  SHOW_INTRO = 0,
  SHOW_OVERVIEW,
} CallsAccountOverviewState;


struct _CallsAccountOverview {
  AdwWindow                 parent;

  /* UI widgets */
  GtkStack                 *stack;
  GtkWidget                *intro;
  GtkWidget                *overview;
  GtkWidget                *add_btn;
  GtkListBoxRow            *add_row;

  /* The window where we add the account providers widget */
  GtkWindow                *account_window;
  GtkWidget                *current_account_widget;

  /* models */
  GListModel               *providers;
  GListModel               *accounts;

  /* misc */
  GtkEventController       *key_controller;
  GtkEventController       *key_controller_account;
  CallsAccountOverviewState state;
  AdwToastOverlay          *toast_overlay;
};

G_DEFINE_TYPE (CallsAccountOverview, calls_account_overview, ADW_TYPE_WINDOW)


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
  guint n_providers;

  g_assert (CALLS_IS_ACCOUNT_OVERVIEW (self));

  n_providers = g_list_model_get_n_items (self->providers);

  for (guint i = 0; i < n_providers; i++) {
    g_autoptr (CallsProvider) provider = g_list_model_get_item (self->providers, i);
    GListModel *origins;

    /* we might be in the middle of the lists being updated! */
    if (!provider) {
      n_providers--;
      break;
    }

    origins = calls_provider_get_origins (provider);

    n_origins += g_list_model_get_n_items (origins);
  }

  if (n_origins > 0)
    self->state = SHOW_OVERVIEW;
  else
    self->state = SHOW_INTRO;

  gtk_widget_set_sensitive (self->add_btn, n_providers > 0);

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

  adw_window_set_content (ADW_WINDOW (self->account_window), widget);

  self->current_account_widget = widget;
}


static void
on_account_row_activated (GtkListBox           *box,
                          GtkListBoxRow        *row,
                          CallsAccountOverview *self)
{
  CallsAccountProvider *provider;
  CallsAccount *account = NULL;
  CallsAccountRow *acc_row;
  GtkWidget *widget;

  g_assert (GTK_IS_LIST_BOX_ROW (row) );
  g_assert (CALLS_IS_ACCOUNT_OVERVIEW (self));
  g_assert (g_list_model_get_n_items (self->providers) > 0);

  if (row == self->add_row) {
    /* TODO this needs changing if we ever have multiple account providers */
    provider = g_list_model_get_item (self->providers, 0);
    widget = calls_account_provider_get_account_widget (provider);
    g_object_unref (provider);

  } else if (CALLS_IS_ACCOUNT_ROW (row)) {
    acc_row = CALLS_ACCOUNT_ROW (row);

    provider = calls_account_row_get_account_provider (acc_row);
    widget = calls_account_provider_get_account_widget (provider);
    account = calls_account_row_get_account (acc_row);

  } else {
    g_warning ("Unknown type of row activated!");
    g_assert_not_reached ();
    return;
  }

  attach_account_widget (self, widget);

  if (account) {
    g_autofree char *title = g_strdup_printf (_("Edit account: %s"),
                                              calls_account_get_address (account));

    calls_account_provider_edit_account (provider, account);
    gtk_window_set_title (self->account_window, title);
  } else {
    calls_account_provider_add_new_account (provider);
    gtk_window_set_title (self->account_window, _("Add new account"));

  }

  gtk_window_present (self->account_window);
}


static void
on_add_account_clicked (CallsAccountOverview *self)
{
  on_account_row_activated (NULL, self->add_row, self);
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
  calls_in_app_notification_show (self->toast_overlay, notification);
}


static void
on_accounts_changed (GListModel           *accounts,
                     guint                 position,
                     guint                 removed,
                     guint                 added,
                     CallsAccountOverview *self)
{
  guint n_providers = g_list_model_get_n_items (self->providers);

  for (guint i = removed; i > 0; i--) {
    GtkListBoxRow *row =
      gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->overview), position + i - 1);

    gtk_list_box_remove (GTK_LIST_BOX (self->overview), GTK_WIDGET (row));
  }

  for (guint i = 0; i < added; i++) {
    g_autoptr (CallsAccount) account =
      CALLS_ACCOUNT (g_list_model_get_item (accounts, position + i));
    CallsAccountProvider *provider = NULL;
    CallsAccountRow *account_row;

    /* which provider does this account belong to? */
    for (guint j = 0; j < n_providers; j++) {
      g_autoptr (CallsProvider) candidate = g_list_model_get_item (self->providers, j);

      if (calls_find_in_model (calls_provider_get_origins (candidate), account, NULL)) {
        provider = CALLS_ACCOUNT_PROVIDER (candidate);
        break;
      }
    }

    g_assert (CALLS_IS_ACCOUNT_PROVIDER (provider));

    account_row = calls_account_row_new (provider, account);

    g_signal_connect_object (account, "message",
                             G_CALLBACK (on_account_message),
                             self,
                             0);
    gtk_list_box_insert (GTK_LIST_BOX (self->overview),
                         GTK_WIDGET (account_row),
                         position + i);
  }

  update_state (self);
}


/*
 * Helper for `on_providers_changed` signal callback
 */
static void
hide_widget (GtkWidget *widget)
{
  gtk_widget_set_visible (widget, FALSE);
}


static void
on_providers_changed (GListModel           *providers,
                      guint                 position,
                      guint                 removed,
                      guint                 added,
                      CallsAccountOverview *self)
{
  for (guint i = 0; i < added; i++) {
    g_autoptr (CallsProvider) provider =
      g_list_model_get_item (providers, position + i);

    g_signal_connect_swapped (provider, "widget-edit-done",
                              G_CALLBACK (hide_widget), self->account_window);
  }

  /* Clear any acccount widgets, because they might've gone stale */
  attach_account_widget (self, NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->account_window), FALSE);
}


static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        modifiers,
                GtkWidget             *widget)
{
  if (keyval == GDK_KEY_Escape) {
    gtk_widget_set_visible (widget, FALSE);
    return GDK_EVENT_STOP;
  }

  return GDK_EVENT_PROPAGATE;
}

static void
calls_account_overview_dispose (GObject *object)
{
  CallsAccountOverview *self = CALLS_ACCOUNT_OVERVIEW (object);

  g_clear_object (&self->providers);

  g_clear_object (&self->accounts);

  g_clear_object (&self->key_controller);
  g_clear_object (&self->key_controller_account);

  G_OBJECT_CLASS (calls_account_overview_parent_class)->dispose (object);
}

static void
calls_account_overview_class_init (CallsAccountOverviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = calls_account_overview_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/account-overview.ui");

  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, add_btn);
  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, add_row);

  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, stack);
  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, intro);
  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, overview);

  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, account_window);

  gtk_widget_class_bind_template_child (widget_class, CallsAccountOverview, toast_overlay);

  gtk_widget_class_bind_template_callback (widget_class, on_add_account_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_account_row_activated);
}


static gboolean
match_account_provider (gpointer item,
                        gpointer unused)
{
  g_assert (CALLS_IS_PROVIDER (item));

  return CALLS_IS_ACCOUNT_PROVIDER (item);
}


static gboolean
match_account (gpointer item,
               gpointer unused)
{
  g_assert (CALLS_IS_ORIGIN (item));

  return CALLS_IS_ACCOUNT (item);
}


static void
calls_account_overview_init (CallsAccountOverview *self)
{
  GtkFilter *account_provider_filter;
  GtkFilter *account_filter;

  GListModel *all_providers =
    calls_plugin_manager_get_providers (calls_plugin_manager_get_default ());
  GListModel *all_origins =
    calls_manager_get_origins (calls_manager_get_default ());

  gtk_widget_init_template (GTK_WIDGET (self));

  account_provider_filter = GTK_FILTER (gtk_custom_filter_new (match_account_provider, NULL, NULL));
  self->providers =
    G_LIST_MODEL (gtk_filter_list_model_new (g_object_ref (all_providers), account_provider_filter));

  g_signal_connect (self->providers,
                    "items-changed",
                    G_CALLBACK (on_providers_changed),
                    self);
  on_providers_changed (self->providers,
                        0, 0, g_list_model_get_n_items (self->providers),
                        self);

  account_filter = GTK_FILTER (gtk_custom_filter_new (match_account, NULL, NULL));
  self->accounts =
    G_LIST_MODEL (gtk_filter_list_model_new (g_object_ref (all_origins), account_filter));
  g_signal_connect_object (self->accounts,
                           "items-changed",
                           G_CALLBACK (on_accounts_changed),
                           self,
                           G_CONNECT_AFTER);
  on_accounts_changed (self->accounts, 0, 0, g_list_model_get_n_items (self->accounts), self);

  gtk_list_box_insert (GTK_LIST_BOX (self->overview),
                       GTK_WIDGET (self->add_row),
                       -1);
  gtk_window_set_transient_for (self->account_window, GTK_WINDOW (self));

  self->key_controller = gtk_event_controller_key_new ();
  g_signal_connect (self->key_controller,
                    "key-pressed",
                    G_CALLBACK (on_key_pressed),
                    self);

  self->key_controller_account = gtk_event_controller_key_new ();
  g_signal_connect (self->key_controller_account,
                    "key-pressed",
                    G_CALLBACK (on_key_pressed),
                    self->account_window);

  gtk_window_set_title (GTK_WINDOW (self), _("VoIP Accounts"));
}


CallsAccountOverview *
calls_account_overview_new (void)
{
  return g_object_new (CALLS_TYPE_ACCOUNT_OVERVIEW,
                       NULL);
}
