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
#include "calls-util.h"

#include <glib/gi18n.h>


struct _CallsBestMatch {
  GObject          parent_instance;

  FolksSearchView *view;
  FolksIndividual *matched_individual;
  char            *phone_number;
  char            *country_code;
  char            *name_sip;
  gboolean         had_country_code_last_time;
};

G_DEFINE_TYPE (CallsBestMatch, calls_best_match, G_TYPE_OBJECT);


enum {
  PROP_0,
  PROP_PHONE_NUMBER,
  PROP_NAME,
  PROP_AVATAR,
  PROP_HAS_INDIVIDUAL,
  PROP_COUNTRY_CODE,
  PROP_PRIMARY_INFO,
  PROP_SECONDARY_INFO,
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
notify_display_info (CallsBestMatch *self)
{
  g_assert (CALLS_IS_BEST_MATCH (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PRIMARY_INFO]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SECONDARY_INFO]);
}


static void
notify_name (CallsBestMatch *self)
{
  g_assert (CALLS_IS_BEST_MATCH (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
  notify_display_info (self);
}


static void
notify_avatar (CallsBestMatch *self)
{
  g_assert (CALLS_IS_BEST_MATCH (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_AVATAR]);
}


static void
update_best_match (CallsBestMatch *self)
{
  GeeSortedSet *individuals = folks_search_view_get_individuals (self->view);
  FolksIndividual *matched_individual = NULL;
  gboolean notify_has_individual = FALSE;

  g_return_if_fail (GEE_IS_COLLECTION (individuals));

  if (!gee_collection_get_is_empty (GEE_COLLECTION (individuals)))
    matched_individual = gee_sorted_set_first (individuals);

  if (matched_individual == self->matched_individual)
    return;

  if (self->matched_individual) {
    g_signal_handlers_disconnect_by_data (self->matched_individual, self);
    g_clear_object (&self->matched_individual);
    notify_has_individual = TRUE;
  }

  if (matched_individual) {
    g_set_object (&self->matched_individual, matched_individual);
    g_signal_connect_swapped (self->matched_individual,
                              "notify::display-name",
                              G_CALLBACK (notify_name),
                              self);
    g_signal_connect_swapped (self->matched_individual,
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
get_property (GObject    *object,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
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

  case PROP_PRIMARY_INFO:
    g_value_set_string (value,
                        calls_best_match_get_primary_info (self));
    break;

  case PROP_SECONDARY_INFO:
    g_value_set_string (value,
                        calls_best_match_get_secondary_info (self));
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

  if (self->matched_individual) {
    g_signal_handlers_disconnect_by_data (self->matched_individual, self);
    g_clear_object (&self->matched_individual);
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
                         GDK_TYPE_TEXTURE,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_PRIMARY_INFO] =
    g_param_spec_string ("primary-info",
                         "Primary Information",
                         "Primary information to display",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SECONDARY_INFO] =
    g_param_spec_string ("secondary-info",
                         "Secondary Information",
                         "Secondary information to display",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_best_match_init (CallsBestMatch *self)
{
}

/**
 * calls_best_match_new:
 * @number: The number to use for contact matching
 *
 * Returns: (transfer full): A new #CallsBestMatch
 */
CallsBestMatch *
calls_best_match_new (const char *number)
{
  return g_object_new (CALLS_TYPE_BEST_MATCH,
                       "phone_number", number,
                       NULL);
}

/**
 * calls_best_match_has_individual:
 * @self: A #CallsBestMatch
 *
 * Returns: %TRUE if a contact was matched, %FALSE otherwise
 */
gboolean
calls_best_match_has_individual (CallsBestMatch *self)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), FALSE);

  return !!self->matched_individual;
}

/**
 * calls_best_match_is_favourite:
 * @self: A #CallsBestMatch
 *
 * Returns: %TRUE if there's a matched individual and the individual is
 * marked as a favourite, %FALSE otherwise.
 */
gboolean
calls_best_match_is_favourite (CallsBestMatch *self)
{
  gboolean fav;

  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), FALSE);

  if (!self->matched_individual)
    return FALSE;

  g_object_get (G_OBJECT (self->matched_individual), "favourite", &fav, NULL);

  return fav;
}

/**
 * calls_best_match_get_phone_number:
 * @self: A #CallsBestMatch
 *
 * Returns: (nullable): The phone number of @self, or %NULL if unknown.
 */
const char *
calls_best_match_get_phone_number (CallsBestMatch *self)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  return self->phone_number;
}

/**
 * calls_best_match_set_phone_number:
 * @self: A #CallsBestMatch
 * @phone_number: (nullable): The phone number
 *
 * Set the @phone_number to use for matching.
 */
void
calls_best_match_set_phone_number (CallsBestMatch *self,
                                   const char     *phone_number)
{
  g_autoptr (CallsPhoneNumberQuery) query = NULL;

  g_return_if_fail (CALLS_IS_BEST_MATCH (self));

  g_clear_pointer (&self->phone_number, g_free);

  // Consider empty string phone numbers as NULL
  if (!STR_IS_NULL_OR_EMPTY (phone_number))
    self->phone_number = g_strdup (phone_number);

  if (self->view)
    g_signal_handlers_disconnect_by_data (self->view, self);
  g_clear_object (&self->view);

  if (self->phone_number) {
    /* This is a SIP address, don' try parsing it as a phone number */
    if (g_str_has_prefix (self->phone_number, "sip")) {
      g_auto (GStrv) split = g_strsplit_set (self->phone_number, ":@", -1);

      g_free (self->name_sip);
      self->name_sip = g_strdup (split[1]);
      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PHONE_NUMBER]);
      notify_display_info (self);
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
  notify_display_info (self);
}

/**
 * calls_best_match_get_name:
 * @self: A #CallsBestMatch
 *
 * Returns: (nullable): The name of a matched individual,
 * the display name or user portion of a SIP address,
 * or %NULL otherwise.
 */
const char *
calls_best_match_get_name (CallsBestMatch *self)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  if (self->matched_individual)
    return folks_individual_get_display_name (self->matched_individual);
  else if (self->name_sip)
    return self->name_sip;

  return NULL;
}

/**
 * calls_best_match_get_avatar:
 * @self: A #CallsBestMatch
 *
 * Returns: (nullable): The avatar of a matched contact or %NULL when there's no match.
 */
GdkTexture *
calls_best_match_get_avatar (CallsBestMatch *self)
{
  GdkTexture *output;
  GLoadableIcon *loadable_icon;
  g_autoptr (GError) error = NULL;

  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  if (!self->matched_individual)
    return NULL;

  loadable_icon = folks_avatar_details_get_avatar (FOLKS_AVATAR_DETAILS (self->matched_individual));

  if (!G_IS_FILE_ICON (loadable_icon)) {
    return NULL;
  }

  output = gdk_texture_new_from_file (g_file_icon_get_file (G_FILE_ICON (loadable_icon)), &error);

  if (error != NULL) {
    g_print ("Failed to read avatar icon file: %s", error->message);
    return NULL;
  }

  return output;
}

/**
 * calls_best_match_get_primary_info:
 * @self: A #CallsBestMatch
 *
 * Returns: (transfer none): The contact description to be used
 * for primary labels
 */
const char *
calls_best_match_get_primary_info (CallsBestMatch *self)
{
  const char *name;

  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  name = calls_best_match_get_name (self);
  if (name)
    return name;

  if (self->phone_number)
    return self->phone_number;

  return _("Anonymous caller");
}

/**
 * calls_best_match_get_secondary_info:
 * @self: A #CallsBestMatch
 *
 * Returns: (transfer none): The contact description to be used
 * for secondary labels
 */
const char *
calls_best_match_get_secondary_info (CallsBestMatch *self)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  if (self->matched_individual)
    return self->phone_number;
  else if (self->name_sip)
    return self->phone_number; /* XXX despite the misnomer, this is actually a SIP address */

  /** TODO for phone calls:
   *  lookup location information based on country/area code
   *  https://gitlab.gnome.org/GNOME/calls/-/issues/358
   */

  return "";
}
