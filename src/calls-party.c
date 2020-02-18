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

#include "calls-party.h"
#include "util.h"

#include <glib/gi18n.h>

/**
 * SECTION:calls-party
 * @short_description: A collection of data about an entity that party to a call.
 * @Title: CallsParty
 */

struct _CallsParty
{
  GObject parent_instance;
  gchar *name;
  gchar *number;
};

G_DEFINE_TYPE(CallsParty, calls_party, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  PROP_NUMBER,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


CallsParty *
calls_party_new (const gchar *name, const gchar *number)
{
  return g_object_new (CALLS_TYPE_PARTY,
                       "name", name,
                       "number", number,
                       NULL);
}


/**
 * calls_party_create_image:
 * @party: a #CallsParty
 *
 * Create a new #GtkImage widget that displays an appropriate image for
 * the party.
 *
 * Returns: a new #GtkImage widget.
 */
GtkWidget *
calls_party_create_image (CallsParty *party)
{
  return gtk_image_new_from_icon_name ("avatar-default-symbolic", GTK_ICON_SIZE_DIALOG);
}


const gchar *
calls_party_get_name (CallsParty *party)
{
  g_return_val_if_fail (CALLS_IS_PARTY (party), NULL);
  return CALLS_PARTY (party)->name;
}


const gchar *
calls_party_get_number (CallsParty *party)
{
  g_return_val_if_fail (CALLS_IS_PARTY (party), NULL);
  return CALLS_PARTY (party)->number;
}


/**
 * calls_party_get_label:
 * @party: a #CallsParty
 *
 * Get an appropriate user-visible label for the party.
 *
 * Returns: the party's name if the name is non-NULL, otherwise the
 * party's number.
 */
const gchar * calls_party_get_label (CallsParty *party)
{
  if (party->name)
    {
      return party->name;
    }

  return party->number;
}


static void
get_property (GObject      *object,
              guint         property_id,
              GValue       *value,
              GParamSpec   *pspec)
{
  CallsParty *self = CALLS_PARTY (object);

  switch (property_id) {
  case PROP_NAME:
    g_value_set_string (value, self->name);
    break;

  case PROP_NUMBER:
    g_value_set_string (value, self->number);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsParty *self = CALLS_PARTY (object);

  switch (property_id) {
  case PROP_NAME:
    CALLS_SET_PTR_PROPERTY (self->name, g_value_dup_string (value));
    break;

  case PROP_NUMBER:
    CALLS_SET_PTR_PROPERTY (self->number, g_value_dup_string (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
finalize (GObject *object)
{
  CallsParty *self = CALLS_PARTY (object);

  CALLS_SET_PTR_PROPERTY (self->number, NULL);
  CALLS_SET_PTR_PROPERTY (self->name, NULL);

  G_OBJECT_CLASS (calls_party_parent_class)->finalize (object);
}


static void
calls_party_class_init (CallsPartyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  props[PROP_NAME] =
    g_param_spec_string ("name",
                         _("Name"),
                         _("The party's name"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  props[PROP_NUMBER] =
    g_param_spec_string ("number",
                         _("Number"),
                         _("The party's telephone number"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_party_init (CallsParty *self)
{
}


