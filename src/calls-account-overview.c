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

  gtk_widget_class_bind_template_callback (widget_class, on_add_account_clicked);
}


static void
calls_account_overview_init (CallsAccountOverview *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

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
