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
 * Author: Adrien Plazas <adrien.plazas@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "calls-encryption-indicator.h"

#include <glib/gi18n.h>

struct _CallsEncryptionIndicator
{
  GtkStack parent_instance;

  GtkBox *is_not_encrypted;
  GtkBox *is_encrypted;
};

G_DEFINE_TYPE (CallsEncryptionIndicator, calls_encryption_indicator, GTK_TYPE_STACK);

enum {
  PROP_0,
  PROP_ENCRYPTED,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


void
calls_encryption_indicator_set_encrypted (CallsEncryptionIndicator *self,
                                          gboolean                  encrypted)
{
  g_return_if_fail (CALLS_IS_ENCRYPTION_INDICATOR (self));

  encrypted = !!encrypted;

  gtk_stack_set_visible_child (
    GTK_STACK (self),
    GTK_WIDGET (encrypted ? self->is_encrypted : self->is_not_encrypted));
}


gboolean
calls_encryption_indicator_get_encrypted (CallsEncryptionIndicator *self)
{
  g_return_val_if_fail (CALLS_IS_ENCRYPTION_INDICATOR (self), FALSE);

  return gtk_stack_get_visible_child (GTK_STACK (self)) == GTK_WIDGET (self->is_encrypted);
}


static void
calls_encryption_indicator_init (CallsEncryptionIndicator *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsEncryptionIndicator *self = CALLS_ENCRYPTION_INDICATOR (object);

  switch (property_id) {
  case PROP_ENCRYPTED:
    calls_encryption_indicator_set_encrypted (self, g_value_get_boolean (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
get_property (GObject      *object,
              guint         property_id,
              GValue       *value,
              GParamSpec   *pspec)
{
  CallsEncryptionIndicator *self = CALLS_ENCRYPTION_INDICATOR (object);

  switch (property_id) {
  case PROP_ENCRYPTED:
    g_value_set_boolean (value, calls_encryption_indicator_get_encrypted (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_encryption_indicator_class_init (CallsEncryptionIndicatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = set_property;
  object_class->get_property = get_property;

  props[PROP_ENCRYPTED] =
    g_param_spec_boolean ("encrypted",
                          "Encrypted",
                          "The party participating in the call",
                          FALSE,
                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/encryption-indicator.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsEncryptionIndicator, is_not_encrypted);
  gtk_widget_class_bind_template_child (widget_class, CallsEncryptionIndicator, is_encrypted);
}

