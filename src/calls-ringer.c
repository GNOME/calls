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

#include "calls-config.h"

#include "calls-manager.h"
#include "calls-ringer.h"
#include "calls-ui-call-data.h"

#include "enum-types.h"

#include <glib/gi18n.h>
#include <glib-object.h>

#define LIBFEEDBACK_USE_UNSTABLE_API
#include <libfeedback.h>


enum {
  PROP_0,
  PROP_RING_STATE,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];


struct _CallsRinger {
  GObject        parent_instance;

  GList         *calls;
  LfbEvent      *ringing_event;
  LfbEvent      *ringing_soft_event;

  GCancellable  *cancel;

  CallsRingState state;
  CallsRingState target_state;

  gboolean       freeze_state_notify;
};


G_DEFINE_TYPE (CallsRinger, calls_ringer, G_TYPE_OBJECT)


static void target_state_step (CallsRinger *self);


static const char *
ring_state_to_string (CallsRingState state)
{
  g_autoptr (GEnumClass) enum_class = NULL;
  GEnumValue *enum_val;

  enum_class = g_type_class_ref (CALLS_TYPE_RING_STATE);
  enum_val = g_enum_get_value (enum_class, state);

  return enum_val ? enum_val->value_nick : "invalid-state";
}


static void
set_ring_state (CallsRinger   *self,
                CallsRingState state)
{
  g_assert (CALLS_IS_RINGER (self));
  g_assert (state >= CALLS_RING_STATE_INACTIVE && state <= CALLS_RING_STATE_ERROR);

  if (self->state == state)
    return;

  g_debug ("Setting ring state to '%s'", ring_state_to_string (state));
  self->state = state;

  if (state == self->target_state)
    self->freeze_state_notify = FALSE;
  else
    target_state_step (self);

  if (!self->freeze_state_notify)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_RING_STATE]);
}


static void
on_event_triggered (LfbEvent     *event,
                    GAsyncResult *res,
                    CallsRinger  *self)
{
  g_autoptr (GError) err = NULL;
  CallsRingState state;
  gboolean ok;

  g_return_if_fail (LFB_IS_EVENT (event));

  ok = lfb_event_trigger_feedback_finish (event, res, &err);
  g_debug ("Feedback '%s' triggered %ssuccessfully",
           lfb_event_get_event (event),
           ok ? "" : "un");

  if (!CALLS_IS_RINGER (self)) {
    g_warning ("Event feedback triggered, but ringer is gone");
    return;
  }

  if (!ok) {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_debug ("Feedback has been cancelled");
    else
      g_warning ("Failed to trigger feedback for '%s': %s",
                 lfb_event_get_event (event), err->message);

    set_ring_state (self, CALLS_RING_STATE_ERROR);
    return;
  }


  if (event == self->ringing_event)
    state = CALLS_RING_STATE_RINGING;
  else
    state = CALLS_RING_STATE_RINGING_SOFT;

  set_ring_state (self, state);
}


static void
on_event_feedback_ended (LfbEvent     *event,
                         GAsyncResult *res,
                         CallsRinger  *self)
{
  g_autoptr (GError) err = NULL;
  gboolean ok;

  g_assert (LFB_IS_EVENT (event));

  ok = lfb_event_end_feedback_finish (event, res, &err);
  g_debug ("Ended feedback '%s' %ssuccessfully",
           lfb_event_get_event (event),
           ok ? "" : "un");

  if (!CALLS_IS_RINGER (self)) {
    g_warning ("Event feedback ended, but ringer is gone");
    return;
  }

  if (!ok) {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_debug ("Ending feedback has been cancelled");
    else
      g_warning ("Failed to end feedback for '%s': %s",
                 lfb_event_get_event (event), err->message);

    set_ring_state (self, CALLS_RING_STATE_ERROR);
  }
}


static void
on_feedback_ended (LfbEvent    *event,
                   CallsRinger *self)
{
  g_assert (LFB_IS_EVENT (event));

  g_debug ("Feedback ended");

  if (!CALLS_IS_RINGER (self)) {
    g_warning ("Feedback stopped, but ringer is gone");
    return;
  }

  /* When no feedback is available on the system (e.g. no vibration motor or LEDs)
   * it will get ended immediately on triggering. Changing the target state will
   * break loop that would otherwise occur. */
  if (lfb_event_get_end_reason (event) == LFB_EVENT_END_REASON_NOT_FOUND ||
      lfb_event_get_end_reason (event) == LFB_EVENT_END_REASON_EXPLICIT)
    self->target_state = CALLS_RING_STATE_INACTIVE;

  set_ring_state (self, CALLS_RING_STATE_INACTIVE);
}


static LfbEvent *
get_event_for_state (CallsRinger   *self,
                     CallsRingState state)
{
  switch (state) {
  case CALLS_RING_STATE_RINGING:
    return self->ringing_event;
  case CALLS_RING_STATE_RINGING_SOFT:
    return self->ringing_soft_event;
  default:
    g_assert_not_reached ();
  }
}


static void
target_state_step (CallsRinger *self)
{
  LfbEvent *event;

  g_assert (CALLS_IS_RINGER (self));

  if (self->state == self->target_state ||
      self->state == CALLS_RING_STATE_ERROR)
    return;

  g_clear_object (&self->cancel);
  self->cancel = g_cancellable_new ();

  if (self->state == CALLS_RING_STATE_INACTIVE) {
    event = get_event_for_state (self, self->target_state);

    lfb_event_trigger_feedback_async (event,
                                      self->cancel,
                                      (GAsyncReadyCallback) on_event_triggered,
                                      self);
  } else {
    gboolean needs_restart =
      self->target_state == CALLS_RING_STATE_RINGING ||
      self->target_state == CALLS_RING_STATE_RINGING_SOFT;

    if (needs_restart)
      self->freeze_state_notify = TRUE;

    event = get_event_for_state (self, self->state);

    lfb_event_end_feedback_async (event,
                                  self->cancel,
                                  (GAsyncReadyCallback) on_event_feedback_ended,
                                  self);
  }

}


static void
set_target_state (CallsRinger   *self,
                  CallsRingState state)
{
  g_assert (CALLS_IS_RINGER (self));
  g_assert (state >= CALLS_RING_STATE_INACTIVE &&
            state <= CALLS_RING_STATE_RINGING_SOFT);

  if (self->target_state == state)
    return;

  self->target_state = state;

  target_state_step (self);
}


static inline gboolean
is_ring_state (CuiCallState state)
{
  return state == CUI_CALL_STATE_INCOMING;
}


static inline gboolean
is_active_state (CuiCallState state)
{
  switch (state) {
  case CUI_CALL_STATE_ACTIVE:
  case CUI_CALL_STATE_CALLING:
  case CUI_CALL_STATE_HELD:
    return TRUE;
  default:
    return FALSE;
  }
}


static gboolean
have_active_call (CallsRinger *self)
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
have_incoming_call (CallsRinger *self)
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
  CallsRingState target_state;

  g_assert (CALLS_IS_RINGER (self));

  if (self->state == CALLS_RING_STATE_ERROR) {
    g_debug ("Can't ring because we're in an error state");
    return;
  }

  if (have_incoming_call (self))
    target_state = !have_active_call (self) ? CALLS_RING_STATE_RINGING : CALLS_RING_STATE_RINGING_SOFT;
  else
    target_state = CALLS_RING_STATE_INACTIVE;

  set_target_state (self, target_state);
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
    self->ringing_event = lfb_event_new ("phone-incoming-call");
    /* Let feedbackd do the loop */
    lfb_event_set_timeout (self->ringing_event, 0);
    g_signal_connect (self->ringing_event,
                      "feedback-ended",
                      G_CALLBACK (on_feedback_ended),
                      self);

    /* FIXME change event once it's available in feedbackd,
     * see https://source.puri.sm/Librem5/feedbackd/-/issues/37
     */
    self->ringing_soft_event = lfb_event_new ("phone-incoming-call");
    lfb_event_set_timeout (self->ringing_soft_event, 15);
    lfb_event_set_feedback_profile (self->ringing_soft_event, "quiet");
    g_signal_connect (self->ringing_soft_event,
                      "feedback-ended",
                      G_CALLBACK (on_feedback_ended),
                      self);
  } else {
    g_warning ("Failed to init libfeedback: %s", err->message);
    set_ring_state (self, CALLS_RING_STATE_ERROR);
  }
}


static void
get_property (GObject    *object,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
  switch (property_id) {
  case PROP_RING_STATE:
    g_value_set_enum (value, calls_ringer_get_state (CALLS_RINGER (object)));
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

  g_signal_handlers_disconnect_by_data (calls_manager_get_default (), self);

  g_cancellable_cancel (self->cancel);
  g_clear_object (&self->cancel);

  G_OBJECT_CLASS (calls_ringer_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsRinger *self = CALLS_RINGER (object);

  if (lfb_is_initted ())
    lfb_uninit ();

  g_clear_object (&self->ringing_event);
  g_clear_object (&self->ringing_soft_event);

  G_OBJECT_CLASS (calls_ringer_parent_class)->finalize (object);
}


static void
calls_ringer_class_init (CallsRingerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;
  object_class->get_property = get_property;

  props[PROP_RING_STATE] =
    g_param_spec_enum ("state",
                       "",
                       "",
                       CALLS_TYPE_RING_STATE,
                       CALLS_RING_STATE_INACTIVE,
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
 * calls_ringer_get_state
 * @self: A #CallsRinger
 *
 * Returns: The #CallsRingerState
 */
CallsRingState
calls_ringer_get_state (CallsRinger *self)
{
  g_return_val_if_fail (CALLS_IS_RINGER (self), CALLS_RING_STATE_INACTIVE);

  return self->state;
}
