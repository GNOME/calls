/*
 * Copyright (C) 2019 Purism SPC
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

#include "calls-best-match.h"
#include "calls-contacts-provider.h"
#include "calls-manager.h"
#include "calls-vala.h"
#include "util.h"

#include <glib/gi18n.h>


struct _CallsBestMatch
{
  GObject parent_instance;

  FolksSearchView    *view;
  FolksIndividual    *best_match;
  char               *phone_number;
  char               *country_code;
  char               *name_sip;
  gboolean            had_country_code_last_time;
};

G_DEFINE_TYPE (CallsBestMatch, calls_best_match, G_TYPE_OBJECT);


enum {
  PROP_0,
  PROP_PHONE_NUMBER,
  PROP_NAME,
  PROP_AVATAR,
  PROP_HAS_INDIVIDUAL,
  PROP_COUNTRY_CODE,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

static void
search_view_prepare_cb (FolksSearchView *view,
                        GAsyncResult    *res,
                        gpointer        *user_data)
{
  g_autoptr (GError) error = NULL;

  folks_search_view_prepare_finish (view, res, &error);

  if (error)
    g_warning ("Failed to prepare Folks search view: %s", error->message);
}


static void
notify_name (CallsBestMatch *self)
{
  g_object_notify_by_pspec (G_OBJECT (self),
                            props[PROP_NAME]);
}


static void
notify_avatar (CallsBestMatch *self)
{
  g_object_notify_by_pspec (G_OBJECT (self),
                            props[PROP_AVATAR]);
}


static void
update_best_match (CallsBestMatch *self)
{
  GeeSortedSet *individuals = folks_search_view_get_individuals (self->view);
  FolksIndividual *best_match = NULL;
  gboolean notify_has_individual = FALSE;

  g_return_if_fail (GEE_IS_COLLECTION (individuals));

  if (!gee_collection_get_is_empty (GEE_COLLECTION (individuals)))
      best_match = gee_sorted_set_first (individuals);

  if (best_match == self->best_match)
    return;

  if (self->best_match) {
    g_signal_handlers_disconnect_by_data (self->best_match, self);
    g_clear_object (&self->best_match);
    notify_has_individual = TRUE;
  }

  if (best_match) {
    g_set_object (&self->best_match, best_match);
    g_signal_connect_swapped (self->best_match,
                              "notify::display-name",
                              G_CALLBACK (notify_name),
                              self);
    g_signal_connect_swapped (self->best_match,
                              "notify::avatar",
                              G_CALLBACK (notify_avatar),
                              self);
    notify_has_individual = TRUE;
  }

  notify_name (self);
  notify_avatar (self);
  if (notify_has_individual)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HAS_INDIVIDUAL]);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsBestMatch *self = CALLS_BEST_MATCH (object);

  switch (property_id) {
  case PROP_PHONE_NUMBER:
    calls_best_match_set_phone_number (self, g_value_get_string (value));
    break;

  case PROP_COUNTRY_CODE:
    g_free (self->country_code);
    self->country_code = g_value_dup_string (value);

    if (self->phone_number) {
      g_autofree char *number = g_strdup (self->phone_number);
      calls_best_match_set_phone_number (self, number);
    }
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
  CallsBestMatch *self = CALLS_BEST_MATCH (object);

  switch (property_id) {
  case PROP_HAS_INDIVIDUAL:
    g_value_set_boolean (value,
                         calls_best_match_has_individual (self));
    break;

  case PROP_PHONE_NUMBER:
    g_value_set_string (value,
                        calls_best_match_get_phone_number (self));
    break;

  case PROP_COUNTRY_CODE:
    g_value_set_string (value, self->country_code);
    break;

  case PROP_NAME:
    g_value_set_string (value,
                        calls_best_match_get_name (self));
    break;

  case PROP_AVATAR:
    g_value_set_object (value,
                        calls_best_match_get_avatar (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
dispose (GObject *object)
{
  CallsBestMatch *self = CALLS_BEST_MATCH (object);

  g_clear_object (&self->view);
  g_clear_pointer (&self->phone_number, g_free);
  g_clear_pointer (&self->country_code, g_free);
  g_clear_pointer (&self->name_sip, g_free);

  if (self->best_match) {
    g_signal_handlers_disconnect_by_data (self->best_match, self);
    g_clear_object (&self->best_match);
  }

  G_OBJECT_CLASS (calls_best_match_parent_class)->dispose (object);
}


static void
calls_best_match_class_init (CallsBestMatchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->get_property = get_property;
  object_class->dispose = dispose;

  props[PROP_HAS_INDIVIDUAL] =
    g_param_spec_boolean ("has-individual",
                          "Has individual",
                          "Whether a matching individual was found or not",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PHONE_NUMBER] =
    g_param_spec_string ("phone_number",
                         "Phone number",
                         "The phone number of the best match",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_COUNTRY_CODE] =
    g_param_spec_string ("country-code",
                         "Country code",
                         "The country code used for matching",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  props[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The display name of the best match",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_AVATAR] =
    g_param_spec_object ("avatar",
                         "Avatar",
                         "The avatar of the best match",
                         G_TYPE_LOADABLE_ICON,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);


  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_best_match_init (CallsBestMatch *self)
{
}


CallsBestMatch *
calls_best_match_new (const char *number)
{
  return g_object_new (CALLS_TYPE_BEST_MATCH,
                       "phone_number", number,
                       NULL);
}

gboolean
calls_best_match_has_individual (CallsBestMatch *self)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), FALSE);

  return !!self->best_match;
}


const char *
calls_best_match_get_phone_number (CallsBestMatch *self)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  return self->phone_number;
}


void
calls_best_match_set_phone_number (CallsBestMatch *self,
                                   const char     *phone_number)
{
  g_autoptr (CallsPhoneNumberQuery) query = NULL;

  g_return_if_fail (CALLS_IS_BEST_MATCH (self));
  g_return_if_fail (phone_number);

  g_clear_pointer (&self->phone_number, g_free);

  // Consider empty string phone numbers as NULL
  if (phone_number[0] != '\0')
    self->phone_number = g_strdup (phone_number);

  if (self->view)
    g_signal_handlers_disconnect_by_data (self->view, self);
  g_clear_object (&self->view);

  if (self->phone_number) {
    /* This is a SIP address, don' try parsing it as a phone number */
    if (g_str_has_prefix (self->phone_number, "sip")) {
      g_auto (GStrv) split = g_strsplit_set (self->phone_number, ":@", -1);

      self->name_sip = g_strdup (split[1]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PHONE_NUMBER]);
      return;
    }
    query = calls_phone_number_query_new (phone_number, self->country_code);
    self->view = folks_search_view_new (folks_individual_aggregator_dup (), FOLKS_QUERY (query));

    g_signal_connect_swapped (self->view,
                              "individuals-changed-detailed",
                              G_CALLBACK (update_best_match),
                              self);

    folks_search_view_prepare (FOLKS_SEARCH_VIEW (self->view),
                               (GAsyncReadyCallback) search_view_prepare_cb,
                               NULL);
  }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PHONE_NUMBER]);
}

const char *
calls_best_match_get_name (CallsBestMatch *self)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  if (self->best_match)
    return folks_individual_get_display_name (self->best_match);
  else if (self->name_sip)
    return self->name_sip;
  else if (self->phone_number)
    return self->phone_number;
  else
    return _("Anonymous caller");
}


GLoadableIcon *
calls_best_match_get_avatar (CallsBestMatch *self)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  if (self->best_match)
    return folks_avatar_details_get_avatar (FOLKS_AVATAR_DETAILS (self->best_match));
  else
    return NULL;
}
