/*
 * Copyright (C) 2025 Phosh.mobi e.V.
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
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define G_LOG_DOMAIN "CallsMediaPlayback"

#include "calls-media-playback.h"

#include <gsound.h>


/**
 * SECTION:media-playback
 * @short_description: Playing
 * @Title: CallsMediaPlayback
 *
 * #CallsMediaPlayback allows local playback of sounds such as "busy" or "dialing"".
 */

#define BUSY_MIN_PLAYBACK_TIME_SECONDS 3.0

typedef enum {
  PLAYBACK_CALLING,
  PLAYBACK_BUSY,
  PLAYBACK_LAST
} PlaybackEvent;

typedef struct {
  CallsMediaPlayback *self;
  GCancellable       *cancellable;
  GTimer             *timer;
  PlaybackEvent       event;
  double              min_playback_time;
} MediaPlaybackData;


static void
free_playback_data (MediaPlaybackData *data)
{
  g_cancellable_cancel (data->cancellable);
  g_clear_object (&data->cancellable);
  g_clear_pointer (&data->timer, g_timer_destroy);

  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MediaPlaybackData, free_playback_data);


struct _CallsMediaPlayback {
  GObject   parent_instance;

  GSoundContext *context;
  GCancellable  *cancellable;

  MediaPlaybackData *data_calling;
  MediaPlaybackData *data_busy;
};

G_DEFINE_FINAL_TYPE (CallsMediaPlayback, calls_media_playback, G_TYPE_OBJECT)


static const char *
playback_event_to_string (PlaybackEvent event)
{
  switch (event) {
  case PLAYBACK_CALLING:
    return "phone-outgoing-calling";

  case PLAYBACK_BUSY:
    return "phone-outgoing-busy";

  case PLAYBACK_LAST:
  default:
    return NULL;
  }
}

static void
calls_media_playback_dispose (GObject *object)
{
  CallsMediaPlayback *self = CALLS_MEDIA_PLAYBACK (object);

  g_clear_pointer (&self->data_calling, free_playback_data);
  g_clear_pointer (&self->data_busy, free_playback_data);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (calls_media_playback_parent_class)->dispose (object);
}


static void
calls_media_playback_class_init (CallsMediaPlaybackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = calls_media_playback_dispose;
}


static void
calls_media_playback_init (CallsMediaPlayback *self)
{
  g_autoptr (GError) error = NULL;

  self->cancellable = g_cancellable_new ();
  self->context = gsound_context_new (self->cancellable, &error);

  if (!self->context) {
    g_warning ("Could not initialize sound context: %s", error->message);
    return;
  }

  gsound_context_set_attributes (self->context, &error,
                                 GSOUND_ATTR_MEDIA_ROLE, "phone",
                                 GSOUND_ATTR_MEDIA_ICON_NAME, "media-role-phone",
                                 NULL);
}


static void playback_data (MediaPlaybackData *data);

static void
on_playing_done (GObject      *object,
                 GAsyncResult *res,
                 gpointer      userdata)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (MediaPlaybackData) data = userdata;

  if (!gsound_context_play_full_finish (GSOUND_CONTEXT (object), res, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Playing '%s' failed: %s",
                 playback_event_to_string (data->event),
                 error->message);
  } else {
    if (g_timer_elapsed (data->timer, NULL) < data->min_playback_time) {
      playback_data (g_steal_pointer (&data));
    } else {
      switch (data->event) {
      case PLAYBACK_CALLING:
        data->self->data_calling = NULL;
        break;

      case PLAYBACK_BUSY:
        data->self->data_busy = NULL;
        break;

      case PLAYBACK_LAST:
      default:
        g_warning ("Unknown event %d", data->event);
      }
    }
  }
}


static void
playback_data (MediaPlaybackData *data)
{
  const char *event_id;

  g_assert (CALLS_IS_MEDIA_PLAYBACK (data->self));
  g_assert (data->self->context);

  event_id = playback_event_to_string (data->event);

  if (!event_id) {
    g_warning ("No event id found for %d", data->event);
    return;
  }

  g_debug ("Starting playback of '%s'", event_id);

  gsound_context_play_full (data->self->context,
                            data->cancellable,
                            on_playing_done,
                            data,
                            GSOUND_ATTR_CANBERRA_CACHE_CONTROL, "volatile",
                            GSOUND_ATTR_EVENT_ID, event_id,
                            NULL);
}


/**
 * calls_media_playback_play_calling:
 * @self: The #CallsMediaPlayback
 *
 * Starts playing the "phone-outgoing-calling" sound,
 * which can be stopped by invoking calls_media_playback_stop_calling().
 */
void
calls_media_playback_play_calling (CallsMediaPlayback *self)
{
  MediaPlaybackData *data;

  g_return_if_fail (CALLS_IS_MEDIA_PLAYBACK (self));
  g_return_if_fail (self->context);

  if (self->data_calling)
    return;

  data = g_new0 (MediaPlaybackData, 1);
  data->self = self;
  data->cancellable = g_cancellable_new ();
  data->timer = g_timer_new ();
  data->event = PLAYBACK_CALLING;
  data->min_playback_time = DBL_MAX;

  self->data_calling = data;
  playback_data (data);
}


/**
 * calls_media_playback_stop_calling:
 * @self: The #CallsMediaPlayback
 *
 * Stops playing the "phone-outgoing-calling" sound,
 * which has been started with calls_media_playback_play_calling().
 */
void
calls_media_playback_stop_calling (CallsMediaPlayback *self)
{
  g_return_if_fail (CALLS_IS_MEDIA_PLAYBACK (self));
  g_return_if_fail (self->context);

  if (!self->data_calling)
    return;

  g_cancellable_cancel (self->data_calling->cancellable);
}

/**
 * calls_media_playback_play_calling:
 * @self: The #CallsMediaPlayback
 *
 * Starts playing the "phone-outgoing-busy" sound,
 * which can be stopped immediately by invoking calls_media_playback_stop_busy().
 */
void
calls_media_playback_play_busy (CallsMediaPlayback *self)
{
  MediaPlaybackData *data;

  g_return_if_fail (CALLS_IS_MEDIA_PLAYBACK (self));
  g_return_if_fail (self->context);

  if (self->data_busy) {
    g_timer_reset (self->data_busy->timer);
    return;
  }

  data = g_new0 (MediaPlaybackData, 1);
  data->self = self;
  data->cancellable = g_cancellable_new ();
  data->timer = g_timer_new ();
  data->event = PLAYBACK_BUSY;
  data->min_playback_time = BUSY_MIN_PLAYBACK_TIME_SECONDS;

  self->data_busy = data;
  playback_data (data);
}


/**
 * calls_media_playback_stop_busy:
 * @self: The #CallsMediaPlayback
 *
 * Stops playing the "phone-outgoing-busy" sound,
 * which has been started with calls_media_playback_play_busy().
 */
void
calls_media_playback_stop_busy (CallsMediaPlayback *self)
{
  g_return_if_fail (CALLS_IS_MEDIA_PLAYBACK (self));
  g_return_if_fail (self->context);

  if (!self->data_busy)
    return;

  g_cancellable_cancel (self->data_busy->cancellable);
}


CallsMediaPlayback *
calls_media_playback_new (void)
{
  return g_object_new (CALLS_TYPE_MEDIA_PLAYBACK, NULL);
}
