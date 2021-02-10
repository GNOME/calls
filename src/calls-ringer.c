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
#define G_LOG_DOMAIN "calls-ringer"

#include "calls-ringer.h"
#include "calls-manager.h"
#include "config.h"

#include <glib/gi18n.h>
#include <glib-object.h>

#define LIBFEEDBACK_USE_UNSTABLE_API
#include <libfeedback.h>


struct _CallsRinger
{
  GObject parent_instance;

  unsigned  ring_count;
  gboolean  playing;
  LfbEvent *event;
};

G_DEFINE_TYPE (CallsRinger, calls_ringer, G_TYPE_OBJECT);

static void
on_event_triggered (LfbEvent     *event,
                    GAsyncResult *res,
                    CallsRinger  *self)
{
    g_autoptr (GError) err = NULL;
    g_return_if_fail (LFB_IS_EVENT (event));
    g_return_if_fail (CALLS_IS_RINGER (self));

    if (lfb_event_trigger_feedback_finish (event, res, &err))
      {
        self->playing = TRUE;
      }
    else
      {
        g_warning ("Failed to trigger feedback for '%s': %s",
                   lfb_event_get_event (event), err->message);
      }
    g_object_unref (self);
}

static void
start (CallsRinger *self)
{
  g_return_if_fail (self->playing == FALSE);

  if (self->event)
    {
      g_object_ref (self);
      lfb_event_trigger_feedback_async (self->event,
                                        NULL,
                                        (GAsyncReadyCallback)on_event_triggered,
                                        self);
    }
}

static void
on_event_feedback_ended (LfbEvent     *event,
                         GAsyncResult *res,
                         CallsRinger  *self)
{
    g_autoptr (GError) err = NULL;
    g_return_if_fail (LFB_IS_EVENT (event));
    g_return_if_fail (CALLS_IS_RINGER (self));

    if (lfb_event_end_feedback_finish (event, res, &err))
      {
        self->playing = FALSE;
      }
    else
      {
        g_warning ("Failed to end feedback for '%s': %s",
                   lfb_event_get_event (event), err->message);
      }
}

static void
on_feedback_ended (LfbEvent *event,
                   CallsRinger *self)
{
  g_debug ("Feedback ended");
  self->playing = FALSE;
}

static void
stop (CallsRinger *self)
{
  g_debug ("Stopping ringtone");
  lfb_event_end_feedback_async (self->event,
                                NULL,
                                (GAsyncReadyCallback)on_event_feedback_ended,
                                self);
}


static void
update_ring (CallsRinger *self)
{
  if (!self->playing)
    {
      if (self->ring_count > 0)
        {
          g_debug ("Starting ringer");
          start (self);
        }
    }
  else
    {
      if (self->ring_count == 0)
        {
          g_debug ("Stopping ringer");
          stop (self);
        }
    }
}


static inline gboolean
is_ring_state (CallsCallState state)
{
  switch (state)
    {
    case CALLS_CALL_STATE_INCOMING:
    case CALLS_CALL_STATE_WAITING:
      return TRUE;
    default:
      return FALSE;
    }
}


static void
state_changed_cb (CallsRinger   *self,
                  CallsCallState new_state,
                  CallsCallState old_state)
{
  gboolean old_is_ring;

  g_return_if_fail (old_state != new_state);

  old_is_ring = is_ring_state (old_state);
  if (old_is_ring == is_ring_state (new_state))
    {
      // No change in ring state
      return;
    }

  if (old_is_ring)
    {
      --self->ring_count;
    }
  else
    {
      ++self->ring_count;
    }

  update_ring (self);
}


static void
update_count (CallsRinger    *self,
              CallsCall      *call,
              short           delta)
{
  if (is_ring_state (calls_call_get_state (call)))
    {
      self->ring_count += delta;
    }

  update_ring (self);
}


static void
call_added_cb (CallsRinger *self, CallsCall *call)
{
  update_count (self, call, +1);

  g_signal_connect_swapped (call,
                            "state-changed",
                            G_CALLBACK (state_changed_cb),
                            self);
}


static void
call_removed_cb (CallsRinger *self, CallsCall *call)
{
  update_count (self, call, -1);

  g_signal_handlers_disconnect_by_data (call, self);
}


static void
calls_ringer_init (CallsRinger *self)
{
  g_autoptr (GError) err = NULL;

  if (lfb_init (APP_ID, &err))
    {
      self->event = lfb_event_new ("phone-incoming-call");
      /* Let feedbackd do the loop */
      lfb_event_set_timeout (self->event, 0);
      g_signal_connect (self->event,
                        "feedback-ended",
                        (GCallback)on_feedback_ended,
                        self);
    }
  else
    {
      g_warning ("Failed to init libfeedback: %s", err->message);
    }
}


static void
constructed (GObject *object)
{
  g_autoptr (GList) calls = NULL;
  GList *c;
  CallsRinger *self = CALLS_RINGER (object);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "call-add",
                            G_CALLBACK (call_added_cb),
                            self);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "call-remove",
                            G_CALLBACK (call_removed_cb),
                            self);

  calls = calls_manager_get_calls (calls_manager_get_default ());
  for (c = calls; c != NULL; c = c->next) {
    call_added_cb (self, c->data);
  }

  G_OBJECT_CLASS (calls_ringer_parent_class)->constructed (object);
}


static void
dispose (GObject *object)
{
  CallsRinger *self = CALLS_RINGER (object);

  if (self->event)
    {
      g_clear_object (&self->event);
      lfb_uninit ();
    }

  G_OBJECT_CLASS (calls_ringer_parent_class)->dispose (object);
}


static void
calls_ringer_class_init (CallsRingerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;
}

CallsRinger *
calls_ringer_new (void)
{
  return g_object_new (CALLS_TYPE_RINGER, NULL);
}
