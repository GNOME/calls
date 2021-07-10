/*
 * Copyright (C) 2020 Purism SPC
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
 * Author: Julian Sparber <julian@sparber.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "calls-in-app-notification.h"

#define DEFAULT_TIMEOUT_SECONDS 3

struct _CallsInAppNotification
{
  GtkRevealer parent_instance;

  GtkLabel *label;

  gint timeout;
  guint timeout_id;
};

G_DEFINE_TYPE (CallsInAppNotification, calls_in_app_notification, GTK_TYPE_REVEALER)


enum {
  PROP_0,
  PROP_TIMEOUT,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static gboolean
timeout_cb (CallsInAppNotification *self)
{
  calls_in_app_notification_hide (self);
  return G_SOURCE_REMOVE;
}


static void
calls_in_app_notification_get_property (GObject      *object,
              guint         property_id,
              GValue       *value,
              GParamSpec   *pspec)
{
  CallsInAppNotification *self = CALLS_IN_APP_NOTIFICATION (object);

  switch (property_id) {
  case PROP_TIMEOUT:
    g_value_set_int (value, self->timeout);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_in_app_notification_set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsInAppNotification *self = CALLS_IN_APP_NOTIFICATION (object);

  switch (property_id)
    {
    case PROP_TIMEOUT:
      self->timeout = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}


static void
calls_in_app_notification_finalize (GObject *object)
{
  CallsInAppNotification *self = CALLS_IN_APP_NOTIFICATION (object);

  if (self->timeout_id)
    g_source_remove (self->timeout_id);

  G_OBJECT_CLASS (calls_in_app_notification_parent_class)->finalize (object);
}


static void
calls_in_app_notification_class_init (CallsInAppNotificationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = calls_in_app_notification_get_property;
  object_class->set_property = calls_in_app_notification_set_property;
  object_class->finalize = calls_in_app_notification_finalize;

  props[PROP_TIMEOUT] = g_param_spec_int ("timeout",
                                          "Timeout",
                                          "The time the in-app notifaction should be shown",
                                          -1,
                                          G_MAXINT,
                                          3,
                                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/in-app-notification.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsInAppNotification, label);
  gtk_widget_class_bind_template_callback (widget_class, calls_in_app_notification_hide);
}


static void
calls_in_app_notification_init (CallsInAppNotification *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  self->timeout = DEFAULT_TIMEOUT_SECONDS;
}


CallsInAppNotification *
calls_in_app_notification_new (void)
{
  return g_object_new (CALLS_TYPE_IN_APP_NOTIFICATION, NULL);
}


void
calls_in_app_notification_show (CallsInAppNotification *self, const gchar *message)
{
  g_return_if_fail (CALLS_IS_IN_APP_NOTIFICATION (self));

  gtk_label_set_text (self->label, message);

  if (self->timeout_id)
    g_source_remove (self->timeout_id);

  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);
  self->timeout_id = g_timeout_add_seconds (self->timeout, (GSourceFunc)timeout_cb, self);
}


void
calls_in_app_notification_hide (CallsInAppNotification *self)
{
  g_return_if_fail (CALLS_IS_IN_APP_NOTIFICATION (self));

  if (self->timeout_id)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }

  gtk_revealer_set_reveal_child (GTK_REVEALER(self), FALSE);
}
