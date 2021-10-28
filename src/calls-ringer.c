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
#define G_LOG_DOMAIN "CallsRinger"

#include "calls-ringer.h"
#include "calls-manager.h"
#include "config.h"

#include <glib/gi18n.h>
#include <glib-object.h>

#define LIBFEEDBACK_USE_UNSTABLE_API
#include <libfeedback.h>

enum {
  PROP_0,
  PROP_IS_RINGING,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];


typedef enum {
  CALLS_RING_STATE_INACTIVE,
  CALLS_RING_STATE_REQUEST_PLAY,
  CALLS_RING_STATE_PLAYING,
  CALLS_RING_STATE_REQUEST_STOP
} CallsRingState;


struct _CallsRinger {
  GObject parent_instance;

  GList *calls;
  LfbEvent *event;
  GCancellable *cancel_ring;
  CallsRingState state;
};

G_DEFINE_TYPE (CallsRinger, calls_ringer, G_TYPE_OBJECT);


static void
change_ring_state (CallsRinger   *self,
                   CallsRingState state)
{
  if (self->state == state)
    return;

  self->state = state;

  /* Ringing has not yet started/stopped */
  if (state == CALLS_RING_STATE_REQUEST_PLAY ||
      state == CALLS_RING_STATE_REQUEST_STOP)
    return;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IS_RINGING]);
}


static void
on_event_triggered (LfbEvent     *event,
                    GAsyncResult *res,
                    CallsRinger  *self)
{
    g_autoptr (GError) err = NULL;
    g_return_if_fail (LFB_IS_EVENT (event));
    g_return_if_fail (CALLS_IS_RINGER (self));

    if (lfb_event_trigger_feedback_finish (event, res, &err)) {
      change_ring_state (self, CALLS_RING_STATE_PLAYING);
    } else {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to trigger feedback for '%s': %s",
                   lfb_event_get_event (event), err->message);
      change_ring_state (self, CALLS_RING_STATE_INACTIVE);
    }

    g_object_unref (self);
}

static void
start (CallsRinger *self,
       gboolean     quiet)
{
  if (self->event)
    lfb_event_set_feedback_profile (self->event, quiet ? "quiet" : NULL);

  if (self->state == CALLS_RING_STATE_PLAYING ||
      self->state == CALLS_RING_STATE_REQUEST_PLAY)
    return;

  if (self->event) {
    g_clear_object (&self->cancel_ring);
    self->cancel_ring = g_cancellable_new ();

    g_object_ref (self);
    lfb_event_trigger_feedback_async (self->event,
                                      self->cancel_ring,
                                      (GAsyncReadyCallback) on_event_triggered,
                                      self);
    change_ring_state (self, CALLS_RING_STATE_REQUEST_PLAY);
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

    if (self->state == CALLS_RING_STATE_REQUEST_PLAY ||
        self->state == CALLS_RING_STATE_PLAYING)
      g_warning ("Feedback ended although it should be playing");

    if (!lfb_event_end_feedback_finish (event, res, &err))
      g_warning ("Failed to end feedback for '%s': %s",
                 lfb_event_get_event (event), err->message);

    change_ring_state (self, CALLS_RING_STATE_INACTIVE);
}


static void
on_feedback_ended (LfbEvent    *event,
                   CallsRinger *self)
{
  g_debug ("Feedback ended");
  change_ring_state (self, CALLS_RING_STATE_INACTIVE);
}


static void
stop (CallsRinger *self)
{
  if (self->state == CALLS_RING_STATE_INACTIVE ||
      self->state == CALLS_RING_STATE_REQUEST_STOP)
    return;

  g_debug ("Stopping ringtone");
  if (self->state == CALLS_RING_STATE_PLAYING) {
    lfb_event_end_feedback_async (self->event,
                                  NULL,
                                  (GAsyncReadyCallback) on_event_feedback_ended,
                                  self);
    change_ring_state (self, CALLS_RING_STATE_REQUEST_STOP);
  } else if (self->state == CALLS_RING_STATE_REQUEST_PLAY) {
    g_cancellable_cancel (self->cancel_ring);
  }
}


static inline gboolean
is_ring_state (CallsCallState state)
{
  switch (state) {
  case CALLS_CALL_STATE_INCOMING:
  case CALLS_CALL_STATE_WAITING:
    return TRUE;
  default:
    return FALSE;
  }
}


static inline gboolean
is_active_state (CallsCallState state)
{
  switch (state) {
  case CALLS_CALL_STATE_ACTIVE:
  case CALLS_CALL_STATE_DIALING:
  case CALLS_CALL_STATE_ALERTING:
  case CALLS_CALL_STATE_HELD:
    return TRUE;
  default:
    return FALSE;
  }
}


static gboolean
has_active_call (CallsRinger *self)
{
  g_assert (CALLS_IS_RINGER (self));

  for (GList *node = self->calls; node != NULL; node = node->next) {
    CallsCall *call = node->data;

    if (is_active_state (calls_call_get_state (call)))
      return TRUE;
  }
  return FALSE;
}


static gboolean
has_incoming_call (CallsRinger *self)
{
  g_assert (CALLS_IS_RINGER (self));

  for (GList *node = self->calls; node != NULL; node = node->next) {
    CallsCall *call = node->data;

    if (is_ring_state (calls_call_get_state (call)))
      return TRUE;
  }
  return FALSE;
}


static void
update_ring (CallsRinger *self)
{
  g_assert (CALLS_IS_RINGER (self));

  if (!self->event) {
    g_debug ("Can't ring because libfeedback is not initialized");
    return;
  }

  if (has_incoming_call (self))
    start (self, has_active_call (self));
  else
    stop (self);
}


static void
call_added_cb (CallsRinger *self,
               CallsCall   *call)
{
  g_assert (CALLS_IS_RINGER (self));
  g_assert (CALLS_IS_CALL (call));

  self->calls = g_list_append (self->calls, call);

  g_signal_connect_swapped (call,
                            "state-changed",
                            G_CALLBACK (update_ring),
                            self);
  update_ring (self);
}


static void
call_removed_cb (CallsRinger *self,
                 CallsCall *call)
{
  self->calls = g_list_remove (self->calls, call);

  g_signal_handlers_disconnect_by_data (call, self);

  update_ring (self);
}


static void
calls_ringer_init (CallsRinger *self)
{
  g_autoptr (GError) err = NULL;

  if (lfb_init (APP_ID, &err)) {
    self->event = lfb_event_new ("phone-incoming-call");
    /* Let feedbackd do the loop */
    lfb_event_set_timeout (self->event, 0);
    g_signal_connect (self->event,
                      "feedback-ended",
                      G_CALLBACK (on_feedback_ended),
                      self);
  } else {
    g_warning ("Failed to init libfeedback: %s", err->message);
  }
}


static void
get_property (GObject    *object,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
  switch (property_id) {
  case PROP_IS_RINGING:
    g_value_set_boolean (value, calls_ringer_get_is_ringing (CALLS_RINGER (object)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
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

  if (self->event) {
    g_clear_object (&self->event);
    lfb_uninit ();
  }
  g_signal_handlers_disconnect_by_data (calls_manager_get_default (), self);

  G_OBJECT_CLASS (calls_ringer_parent_class)->dispose (object);
}


static void
calls_ringer_class_init (CallsRingerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->get_property = get_property;

  props[PROP_IS_RINGING] =
    g_param_spec_boolean ("ringing",
                          "Ringing",
                          "Whether we're currently ringing",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     PROP_LAST_PROP,
                                     props);
}

CallsRinger *
calls_ringer_new (void)
{
  return g_object_new (CALLS_TYPE_RINGER, NULL);
}


/**
 * calls_ringer_get_ring_is_ringing:
 * @self: A #CallsRinger
 *
 * Returns: %TRUE if currently ringing, %FALSE otherwise.
 */
gboolean
calls_ringer_get_is_ringing (CallsRinger *self)
{
  g_return_val_if_fail (CALLS_IS_RINGER (self), FALSE);

  return self->state == CALLS_RING_STATE_PLAYING ||
    self->state == CALLS_RING_STATE_REQUEST_STOP;
}
