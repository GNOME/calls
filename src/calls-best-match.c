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
  /** All requested gint avatar sizes */
  GList              *avatar_sizes;
  /** GCancellables for in-progress loads */
  GList              *avatar_loads;
  /** Map of gint icon size to GdkPixbuf */
  GHashTable         *avatars;
};

G_DEFINE_TYPE (CallsBestMatch, calls_best_match, G_TYPE_OBJECT);


enum {
  PROP_0,
  PROP_VIEW,
  PROP_NAME,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

enum {
  SIGNAL_AVATAR,
  SIGNAL_LAST_SIGNAL,
};
static guint signals [SIGNAL_LAST_SIGNAL];


struct CallsAvatarRequestData
{
  CallsBestMatch *self;
  GCancellable   *cancellable;
  gint            size;
};


static void
avatar_request_data_destroy (struct CallsAvatarRequestData *data)
{
  data->self->avatar_loads =
    g_list_remove (data->self->avatar_loads,
                   data->cancellable);

  g_free (data);
}


inline static void
add_avatar (CallsBestMatch *self,
            gint            size,
            GdkPixbuf      *avatar)
{
  g_hash_table_insert (self->avatars,
                       GINT_TO_POINTER (size),
                       avatar);

  g_debug ("Added avatar of size %i for best match `%s'",
           size,
           folks_individual_get_display_name (self->best_match));

  g_signal_emit_by_name (self, "avatar", size, avatar);
}


static void
request_avatar_pixbuf_new_cb (GInputStream *stream,
                              GAsyncResult *res,
                              struct CallsAvatarRequestData *data)
{
  GdkPixbuf *avatar;
  GError *error = NULL;

  avatar = gdk_pixbuf_new_from_stream_finish (res, &error);
  if (avatar)
    {
      add_avatar (data->self, data->size, avatar);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Error creating GdkPixbuf from avatar"
                     " icon stream at size %i for Folks"
                     " individual `%s': %s",
                     data->size,
                     calls_best_match_get_name (data->self),
                     error->message);
        }
      g_error_free (error);
    }

  avatar_request_data_destroy (data);
}


static void
request_avatar_icon_load_cb (GLoadableIcon *icon,
                             GAsyncResult *res,
                             struct CallsAvatarRequestData *data)
{
  GInputStream *stream;
  GError *error = NULL;

  stream = g_loadable_icon_load_finish (icon, res, NULL, &error);
  if (!stream)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Error loading avatar icon at size %i"
                     " for Folks individual `%s': %s",
                     data->size,
                     calls_best_match_get_name (data->self),
                     error->message);
        }
      g_error_free (error);

      avatar_request_data_destroy (data);
      return;
    }

  gdk_pixbuf_new_from_stream_at_scale_async
    (stream,
     -1,
     data->size,
     TRUE,
     data->cancellable,
     (GAsyncReadyCallback)request_avatar_pixbuf_new_cb,
     data);

  g_object_unref (stream);
}


static void
request_avatar (CallsBestMatch *self,
                gint            size)
{
  GLoadableIcon *icon;
  struct CallsAvatarRequestData *data;

  if (!self->best_match)
    {
      return;
    }

  icon = folks_avatar_details_get_avatar
    (FOLKS_AVATAR_DETAILS(self->best_match));
  if (!icon)
    {
      return;
    }

  g_debug ("Requesting avatar of size %i for best match `%s'",
           size,
           folks_individual_get_display_name (self->best_match));

  data = g_new (struct CallsAvatarRequestData, 1);
  data->self = self;
  data->size = size;
  data->cancellable = g_cancellable_new ();

  self->avatar_loads = g_list_prepend
    (self->avatar_loads, data->cancellable);

  g_loadable_icon_load_async
    (icon,
     size,
     data->cancellable,
     (GAsyncReadyCallback)request_avatar_icon_load_cb,
     data);

  g_object_unref (data->cancellable);
}


static void
notify_name (CallsBestMatch *self)
{
  g_object_notify_by_pspec (G_OBJECT (self),
                            props[PROP_NAME]);
}


static void
clear_avatars (CallsBestMatch *self)
{
  GList *node;

  for (node = self->avatar_loads; node; node = node->next)
    {
      g_cancellable_cancel ((GCancellable *)node->data);
    }

  g_list_free (self->avatar_loads);
  self->avatar_loads = NULL;

  g_hash_table_remove_all (self->avatars);
}


static void
request_avatars (CallsBestMatch *self)
{
  GList *node;

  for (node = self->avatar_sizes; node; node = node->next)
    {
      request_avatar (self, GPOINTER_TO_INT (node->data));
    }
}


static void
change_avatar (CallsBestMatch *self)
{
  g_debug ("Avatar changed for best match `%s'",
           folks_individual_get_display_name (self->best_match));

  clear_avatars (self);
  request_avatars (self);
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
                              G_CALLBACK (change_avatar),
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
  request_avatars (self);
  notify_name (self);
}


static void
change_best_match (CallsBestMatch  *self,
                   FolksIndividual *best_match)
{
  clear_best_match (self);
  set_best_match (self, best_match);
  change_avatar (self);
  notify_name (self);
}


static void
remove_best_match (CallsBestMatch *self)
{
  GList *node;

  clear_best_match (self);
  clear_avatars (self);

  // Emit empty avatars
  for (node = self->avatar_sizes; node; node = node->next)
    {
      g_signal_emit_by_name (self,
                             "avatar",
                             GPOINTER_TO_INT (node->data),
                             NULL);
    }

  notify_name (self);
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}


static void
dispose (GObject *object)
{
  CallsBestMatch *self = CALLS_BEST_MATCH (object);

  clear_avatars (self);

  g_clear_object (&self->view);

  G_OBJECT_CLASS (calls_best_match_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsBestMatch *self = CALLS_BEST_MATCH (object);

  g_list_free (self->avatar_sizes);
  g_hash_table_unref (self->avatars);

  G_OBJECT_CLASS (calls_best_match_parent_class)->finalize (object);
}


static void
calls_best_match_class_init (CallsBestMatchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->get_property = get_property;
  object_class->dispose = dispose;
  object_class->finalize = finalize;


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

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  signals[SIGNAL_AVATAR] =
    g_signal_new ("avatar",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_INT,
                  GDK_TYPE_PIXBUF);
}


static void
calls_best_match_init (CallsBestMatch *self)
{
  self->avatars = g_hash_table_new_full
    (g_direct_hash,
     g_direct_equal,
     NULL,
     (GDestroyNotify)g_object_unref);
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


GdkPixbuf *
calls_best_match_request_avatar (CallsBestMatch *self,
                                 gint            size)
{
  gpointer sizeptr = GINT_TO_POINTER (size);
  GdkPixbuf *avatar;

  g_return_val_if_fail (CALLS_IS_BEST_MATCH (self), NULL);

  avatar = g_hash_table_lookup (self->avatars, sizeptr);
  if (avatar)
    {
      // Already loaded
      return avatar;
    }

  if (!g_list_find (self->avatar_sizes, sizeptr))
    {
      // Not known, do the actual request
      request_avatar (self, size);

      // Add the size to the list
      self->avatar_sizes = g_list_prepend
        (self->avatar_sizes, sizeptr);
    }

  return NULL;
}
