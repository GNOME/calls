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

#define G_LOG_DOMAIN "CallsAccountRow"

#include "calls-account-row.h"
#include "calls-account-provider.h"


/**
 * Section:calls-account-row
 * short_description: A #HdyActionRow for use in #CallsAccountOverview
 * @Title: CallsAccountRow
 *
 * This is a #HdyActionRow derived widget representing a #CallsAccount
 * for VoIP accounts (currently only SIP).
 */


enum {
  PROP_0,
  PROP_PROVIDER,
  PROP_ACCOUNT,
  PROP_ONLINE,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

enum {
  EDIT_CLICKED,
  N_SIGNALS
};
static guint signals[N_SIGNALS];

struct _CallsAccountRow {
  HdyActionRow          parent;

  CallsAccountProvider *provider;
  CallsAccount         *account;
  gboolean              online;

  /* UI elements */
  HdyAvatar            *avatar;
  GtkSwitch            *online_switch;
  GtkButton            *edit_btn;
};

G_DEFINE_TYPE (CallsAccountRow, calls_account_row, HDY_TYPE_ACTION_ROW)


static void
on_account_state_changed (CallsAccountRow *self)
{
  CallsAccountState state = calls_account_get_state (self->account);

  gtk_switch_set_active (self->online_switch, state == CALLS_ACCOUNT_STATE_ONLINE);
}

static void
on_edit_clicked (CallsAccountRow *self)
{
  /** CallsAccountOverview connects to this signal to show
   *  the window containing the account providers widget.
   *  See calls_account_provider_get_account_widget()
   */
  g_signal_emit (self, signals[EDIT_CLICKED], 0, self->provider, self->account);
}


static void
on_online_switched (CallsAccountRow *self)
{
  gboolean online;

  g_assert (CALLS_IS_ACCOUNT_ROW (self));

  online = gtk_switch_get_active (self->online_switch);
  calls_account_go_online (self->account, online);
}


static void
calls_account_row_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  CallsAccountRow *self = CALLS_ACCOUNT_ROW (object);

  switch (property_id) {
  case PROP_PROVIDER:
    self->provider = g_value_get_object (value);
    break;

  case PROP_ACCOUNT:
    self->account = g_value_get_object (value);
    g_object_bind_property (self->account, "name",
                            self, "title",
                            G_BINDING_SYNC_CREATE);
    g_object_bind_property (self->account, "address",
                            self, "subtitle",
                            G_BINDING_SYNC_CREATE);

    g_signal_connect_object (self->account, "notify::account-state",
                             G_CALLBACK (on_account_state_changed), self,
                             G_CONNECT_SWAPPED);
    on_account_state_changed (self);
    break;

  case PROP_ONLINE:
    calls_account_row_set_online (self, g_value_get_boolean (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_account_row_get_property (GObject      *object,
                                guint         property_id,
                                GValue       *value,
                                GParamSpec   *pspec)
{
  CallsAccountRow *self = CALLS_ACCOUNT_ROW (object);

  switch (property_id) {
  case PROP_ACCOUNT:
    g_value_set_object (value, calls_account_row_get_account (self));
    break;

  case PROP_ONLINE:
    g_value_set_boolean (value, calls_account_row_get_online (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_account_row_class_init (CallsAccountRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = calls_account_row_set_property;
  object_class->get_property = calls_account_row_get_property;

  signals[EDIT_CLICKED] =
    g_signal_new ("edit-clicked",
                  CALLS_TYPE_ACCOUNT_ROW,
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  CALLS_TYPE_ACCOUNT_PROVIDER,
                  CALLS_TYPE_ACCOUNT);

  props[PROP_PROVIDER] =
    g_param_spec_object ("provider",
                         "Provider",
                         "The provider of the account this row represents",
                         CALLS_TYPE_ACCOUNT_PROVIDER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  props[PROP_ACCOUNT] =
    g_param_spec_object ("account",
                         "Account",
                         "The account this row represents",
                         CALLS_TYPE_ACCOUNT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  props[PROP_ONLINE] =
    g_param_spec_boolean ("online",
                          "online",
                          "The state of the online switch",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/account-row.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsAccountRow, avatar);
  gtk_widget_class_bind_template_child (widget_class, CallsAccountRow, online_switch);
  gtk_widget_class_bind_template_child (widget_class, CallsAccountRow, edit_btn);

 gtk_widget_class_bind_template_callback (widget_class, on_edit_clicked);
 gtk_widget_class_bind_template_callback (widget_class, on_online_switched);
}


static void
calls_account_row_init (CallsAccountRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


CallsAccountRow *
calls_account_row_new (CallsAccountProvider *provider,
                       CallsAccount         *account)
{
  g_return_val_if_fail (CALLS_IS_ACCOUNT (account), NULL);

  return g_object_new (CALLS_TYPE_ACCOUNT_ROW,
                       "provider", provider,
                       "account", account,
                       NULL);
}


gboolean
calls_account_row_get_online (CallsAccountRow *self)
{
  g_return_val_if_fail (CALLS_IS_ACCOUNT_ROW (self), FALSE);

  return gtk_switch_get_active (self->online_switch);
}


void
calls_account_row_set_online (CallsAccountRow *self,
                              gboolean         online)
{
  g_return_if_fail (CALLS_IS_ACCOUNT_ROW (self));

  if (online == gtk_switch_get_active (self->online_switch))
    return;

  gtk_switch_set_active (self->online_switch, online);
}


CallsAccount *
calls_account_row_get_account (CallsAccountRow *self)
{
  g_return_val_if_fail (CALLS_IS_ACCOUNT_ROW (self), NULL);

  return self->account;
}
