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

#include "config.h"

#include "calls-manager.h"
#include "calls-ringer.h"
#include "calls-ui-call-data.h"

#include <glib/gi18n.h>
#include <glib-object.h>

#define LIBFEEDBACK_USE_UNSTABLE_API
#include <libfeedback.h>


enum {
  PROP_0,
  PROP_IS_RINGING,
  PROP_RING_IS_QUIET,
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
  GObject        parent_instance;

  GList         *calls;
  LfbEvent      *event;
  GCancellable  *cancel_ring;
  CallsRingState state;

  guint          restart_id;
  gboolean       is_quiet;
};


G_DEFINE_TYPE (CallsRinger, calls_ringer, G_TYPE_OBJECT)


static const char *
ring_state_to_string (CallsRingState state)
{
  switch (state) {
  case CALLS_RING_STATE_INACTIVE:
    return "inactive";
  case CALLS_RING_STATE_REQUEST_PLAY:
    return "request-play";
  case CALLS_RING_STATE_PLAYING:
    return "playing";
  case CALLS_RING_STATE_REQUEST_STOP:
    return "request-stop";
  default:
    return "unknown";
  }
}


static void
change_ring_state (CallsRinger   *self,
                   CallsRingState state)
{
  g_debug ("%s: old: %s; new: %s",
           __func__, ring_state_to_string (self->state), ring_state_to_string (state));
  if (self->state == state)
    return;

  self->state = state;

  /* Currently restarting, so don't notify */
  if (self->restart_id)
    return;

  /* Ringing has not yet started/stopped */
  if (state == CALLS_RING_STATE_REQUEST_PLAY ||
      state == CALLS_RING_STATE_REQUEST_STOP)
    return;

  g_debug ("%s: notify ring", __func__);

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

  g_debug ("%s", __func__);
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


static void restart (CallsRinger *self, gboolean quiet);

static void
start (CallsRinger *self,
       gboolean     quiet)
{
  g_debug ("%s: state: %s", __func__, ring_state_to_string (self->state));
  if (self->event)
    lfb_event_set_feedback_profile (self->event, quiet ? "quiet" : NULL);

  if (self->state == CALLS_RING_STATE_PLAYING ||
      self->state == CALLS_RING_STATE_REQUEST_PLAY) {
    if (self->is_quiet != quiet)
      restart (self, quiet);

    return;
  }

  if (self->event) {
    g_clear_object (&self->cancel_ring);
    self->cancel_ring = g_cancellable_new ();

    self->is_quiet = quiet;
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

  g_debug ("%s: state: %s", __func__, ring_state_to_string (self->state));

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
  g_debug ("%s: state: %s", __func__, ring_state_to_string (self->state));
  change_ring_state (self, CALLS_RING_STATE_INACTIVE);
}


static void
stop (CallsRinger *self)
{
  g_debug ("%s: state: %s", __func__, ring_state_to_string (self->state));
  if (self->state == CALLS_RING_STATE_INACTIVE ||
      self->state == CALLS_RING_STATE_REQUEST_STOP)
    return;

  g_debug ("Stopping ringtone");
  if (self->state == CALLS_RING_STATE_PLAYING) {
    g_debug ("ending event feedback");
    lfb_event_end_feedback_async (self->event,
                                  NULL,
                                  (GAsyncReadyCallback) on_event_feedback_ended,
                                  self);
    change_ring_state (self, CALLS_RING_STATE_REQUEST_STOP);
  } else if (self->state == CALLS_RING_STATE_REQUEST_PLAY) {
    g_debug ("cancelling event feedback");
    g_cancellable_cancel (self->cancel_ring);
  }
}


typedef struct {
  CallsRinger *ringer;
  gboolean     quiet;
} RestartRingerData;


static gboolean
on_ringer_restart (gpointer user_data)
{
  RestartRingerData *data = user_data;

  if (data->ringer->state == CALLS_RING_STATE_PLAYING) {
    stop (data->ringer);

    return G_SOURCE_CONTINUE;
  }

  /* wait until requests have been fulfilled */
  if (data->ringer->state == CALLS_RING_STATE_REQUEST_PLAY ||
      data->ringer->state == CALLS_RING_STATE_REQUEST_STOP) {
    return G_SOURCE_CONTINUE;
  }

  if (data->ringer->state == CALLS_RING_STATE_INACTIVE) {
    start (data->ringer, data->quiet);

    return G_SOURCE_REMOVE;
  }

  g_return_val_if_reached (G_SOURCE_CONTINUE);
}


static void
clean_up_restart_data (gpointer user_data)
{
  RestartRingerData *data = user_data;

  data->ringer->restart_id = 0;

  g_free (data);
}


static void
restart (CallsRinger *self,
         gboolean     quiet)
{
  RestartRingerData *data = g_new0 (RestartRingerData, 1);

  data->ringer = self;
  data->quiet = quiet;

  if (self->restart_id)
    g_source_remove (self->restart_id);

  self->restart_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                      G_SOURCE_FUNC (on_ringer_restart),
                                      data,
                                      clean_up_restart_data);
}


static inline gboolean
is_ring_state (CuiCallState state)
{
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

  switch (state) {
  case CUI_CALL_STATE_INCOMING:
  case CUI_CALL_STATE_WAITING:
    return TRUE;
  default:
    return FALSE;
  }
}


static inline gboolean
is_active_state (CuiCallState state)
{
  switch (state) {
  case CUI_CALL_STATE_ACTIVE:
  case CUI_CALL_STATE_CALLING:
  case CUI_CALL_STATE_ALERTING:
  case CUI_CALL_STATE_HELD:
    return TRUE;
  default:
    return FALSE;
  }
}
#pragma GCC diagnostic warning "-Wdeprecated-declarations"


static gboolean
has_active_call (CallsRinger *self)
{
  g_assert (CALLS_IS_RINGER (self));

  for (GList *node = self->calls; node != NULL; node = node->next) {
    CallsUiCallData *call = node->data;

    if (is_active_state (cui_call_get_state (CUI_CALL (call))))
      return TRUE;
  }
  return FALSE;
}


static gboolean
has_incoming_call (CallsRinger *self)
{
  g_assert (CALLS_IS_RINGER (self));

  for (GList *node = self->calls; node != NULL; node = node->next) {
    CallsUiCallData *call = node->data;

    if (is_ring_state (cui_call_get_state (CUI_CALL (call))) &&
        !calls_ui_call_data_get_silenced (call) &&
        calls_ui_call_data_get_ui_active (call))
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
call_added_cb (CallsRinger     *self,
               CallsUiCallData *call)
{
  g_assert (CALLS_IS_RINGER (self));
  g_assert (CALLS_IS_UI_CALL_DATA (call));

  self->calls = g_list_append (self->calls, call);

  g_object_connect (call,
                    "swapped-signal::notify::state", G_CALLBACK (update_ring), self,
                    "swapped-signal::notify::silenced", G_CALLBACK (update_ring), self,
                    "swapped-signal::notify::ui-active", G_CALLBACK (update_ring), self,
                    NULL);

  update_ring (self);
}


static void
call_removed_cb (CallsRinger     *self,
                 CallsUiCallData *call)
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

  case PROP_RING_IS_QUIET:
    g_value_set_boolean (value, calls_ringer_get_ring_is_quiet (CALLS_RINGER (object)));
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
                            "ui-call-added",
                            G_CALLBACK (call_added_cb),
                            self);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "ui-call-removed",
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

  g_clear_handle_id (&self->restart_id, g_source_remove);

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

  props[PROP_RING_IS_QUIET] =
    g_param_spec_boolean ("is-quiet",
                          "is quiet",
                          "Whether the ringing is of the quiet persuasion",
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

/**
 * calls_ringer_get_ring_is_quiet:
 * @self: A #CallsRinger
 *
 * Returns: %TRUE if currently ringing quietly, %FALSE otherwise.
 */
gboolean
calls_ringer_get_ring_is_quiet (CallsRinger *self)
{
  g_return_val_if_fail (CALLS_IS_RINGER (self), FALSE);

  return calls_ringer_get_is_ringing (self) && self->is_quiet;
}
