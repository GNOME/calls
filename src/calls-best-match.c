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
#include "util.h"

#include <glib/gi18n.h>


struct _CallsBestMatch
{
  GObject parent_instance;

  CallsBestMatchView *view;
  FolksIndividual    *best_match;
  gulong display_name_notify_handler_id;
  gulong avatar_notify_handler_id;
};

G_DEFINE_TYPE (CallsBestMatch, calls_best_match, G_TYPE_OBJECT);


enum {
  PROP_0,
  PROP_VIEW,
  PROP_NAME,
  PROP_AVATAR,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

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
set_best_match (CallsBestMatch  *self,
                FolksIndividual *best_match)
{
  g_assert (self->best_match == NULL);
  g_assert (self->display_name_notify_handler_id == 0);
  g_assert (self->avatar_notify_handler_id == 0);

  self->best_match = best_match;
  g_object_ref (best_match);

  self->display_name_notify_handler_id =
    g_signal_connect_swapped (self->best_match,
                              "notify::display-name",
                              G_CALLBACK (notify_name),
                              self);

  self->avatar_notify_handler_id =
    g_signal_connect_swapped (self->best_match,
                              "notify::avatar",
                              G_CALLBACK (notify_avatar),
                              self);
}


static void
clear_best_match (CallsBestMatch *self)
{
  calls_clear_signal (self->best_match,
                      &self->avatar_notify_handler_id);
  calls_clear_signal (self->best_match,
                      &self->display_name_notify_handler_id);

  g_clear_object (&self->best_match);
}


static void
new_best_match (CallsBestMatch  *self,
                FolksIndividual *best_match)
{
  set_best_match (self, best_match);
  notify_name (self);
  notify_avatar (self);
}


static void
change_best_match (CallsBestMatch  *self,
                   FolksIndividual *best_match)
{
  clear_best_match (self);
  set_best_match (self, best_match);
  notify_name (self);
  notify_avatar (self);
}


static void
remove_best_match (CallsBestMatch *self)
{
  clear_best_match (self);
  notify_name (self);
  notify_avatar (self);
}


static void
update_best_match (CallsBestMatch *self)
{
  FolksIndividual *best_match;

  g_debug ("Best match property notified");

  best_match = calls_best_match_view_get_best_match
    (self->view);

  if (best_match)
    {
      if (self->best_match)
        {
          if (self->best_match == best_match)
            {
              // No change
              g_debug (" No best match change");
            }
          else
            {
              // Different best match object
              change_best_match (self, best_match);
              g_debug (" Different best match object");
            }
        }
      else
        {
          // New best match
          new_best_match (self, best_match);
          g_debug (" New best match");
        }
    }
  else
    {
      if (self->best_match)
        {
          // Best match disappeared
          remove_best_match (self);
          g_debug (" Best match disappeared");
        }
      else
        {
          // No change
          g_debug (" No best match change");
        }
    }
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsBestMatch *self = CALLS_BEST_MATCH (object);

  switch (property_id)
    {
    case PROP_VIEW:
      g_set_object (&self->view,
                    CALLS_BEST_MATCH_VIEW (g_value_get_object (value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}


static void
constructed (GObject *object)
{
  CallsBestMatch *self = CALLS_BEST_MATCH (object);


  g_signal_connect_swapped (self->view,
                            "notify::best-match",
                            G_CALLBACK (update_best_match),
                            self);

  G_OBJECT_CLASS (calls_best_match_parent_class)->constructed (object);
}


static void
get_property (GObject      *object,
              guint         property_id,
              GValue       *value,
              GParamSpec   *pspec)
{
  CallsBestMatch *self = CALLS_BEST_MATCH (object);

  switch (property_id)
    {
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

  G_OBJECT_CLASS (calls_best_match_parent_class)->dispose (object);
}


static void
calls_best_match_class_init (CallsBestMatchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->get_property = get_property;
  object_class->dispose = dispose;

  props[PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The CallsBestMatchView to monitor",
                         CALLS_TYPE_BEST_MATCH_VIEW,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

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
calls_best_match_new (CallsBestMatchView *view)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH_VIEW (view), NULL);

  return g_object_new (CALLS_TYPE_BEST_MATCH,
                       "view", view,
                       NULL);
}


const gchar *
calls_best_match_get_name (CallsBestMatch *self)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  if (self->best_match)
    {
      return folks_individual_get_display_name (self->best_match);
    }
  else
    {
      return NULL;
    }
}


GLoadableIcon *
calls_best_match_get_avatar (CallsBestMatch *self)
{
  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  if (self->best_match)
    {
      return folks_avatar_details_get_avatar (FOLKS_AVATAR_DETAILS (self->best_match));
    }
  else
    {
      return NULL;
    }
}
