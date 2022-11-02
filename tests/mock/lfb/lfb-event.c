/*
 * Copyright (C) 2020 Purism SPC
 * SPDX-License-Identifier: LGPL-2.1+
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#include "lfb-event.h"
#include "lfb-enums.h"

#include <gio/gio.h>

gboolean success = TRUE;
guint interval_timeout_ms = 50;


enum {
  PROP_0,
  PROP_EVENT,
  PROP_TIMEOUT,
  PROP_STATE,
  PROP_END_REASON,
  PROP_FEEDBACK_PROFILE,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

enum {
  SIGNAL_FEEDBACK_ENDED,
  N_SIGNALS,
};
static guint signals[N_SIGNALS];

typedef struct _LfbEvent {
  GObject       parent;

  char         *event;
  gint          timeout;
  gchar        *profile;

  guint         id;
  LfbEventState state;
  gint          end_reason;
  gulong        handler_id;
} LfbEvent;

G_DEFINE_TYPE (LfbEvent, lfb_event, G_TYPE_OBJECT);

typedef struct _LpfAsyncData {
  LfbEvent *event;
  GTask    *task;
} LpfAsyncData;

static void
lfb_event_set_state (LfbEvent *self, LfbEventState state)
{
  if (self->state == state)
    return;

  self->state = state;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}



static void
lfb_event_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  LfbEvent *self = LFB_EVENT (object);

  switch (property_id) {
  case PROP_EVENT:
    g_free (self->event);
    self->event = g_value_dup_string (value);
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_EVENT]);
    break;
  case PROP_TIMEOUT:
    lfb_event_set_timeout (self, g_value_get_int (value));
    break;
  case PROP_FEEDBACK_PROFILE:
    lfb_event_set_feedback_profile (self, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
lfb_event_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  LfbEvent *self = LFB_EVENT (object);

  switch (property_id) {
  case PROP_EVENT:
    g_value_set_string (value, self->event);
    break;
  case PROP_TIMEOUT:
    g_value_set_int (value, self->timeout);
    break;
  case PROP_FEEDBACK_PROFILE:
    g_value_set_string (value, self->profile);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
lfb_event_finalize (GObject *object)
{
  LfbEvent *self = LFB_EVENT (object);

  /* Signal handler is disconnected automatically due to g_signal_connect_object */
  self->handler_id = 0;

  g_clear_pointer (&self->event, g_free);
  g_clear_pointer (&self->profile, g_free);

  G_OBJECT_CLASS (lfb_event_parent_class)->finalize (object);
}

static void
lfb_event_class_init (LfbEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = lfb_event_set_property;
  object_class->get_property = lfb_event_get_property;

  object_class->finalize = lfb_event_finalize;

  props[PROP_EVENT] =
    g_param_spec_string (
      "event",
      "Event",
      "The name of the event triggering the feedback",
      NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_TIMEOUT] =
    g_param_spec_int (
      "timeout",
      "Timeout",
      "When the event should timeout",
      -1, G_MAXINT, -1,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_STATE] =
    g_param_spec_enum (
      "state",
      "State",
      "The event's state",
      LFB_TYPE_EVENT_STATE,
      LFB_EVENT_END_REASON_NATURAL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_END_REASON] =
    g_param_spec_enum (
      "end-reason",
      "End reason",
      "The reason why the feedbacks ended",
      LFB_TYPE_EVENT_END_REASON,
      LFB_EVENT_END_REASON_NATURAL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_FEEDBACK_PROFILE] =
    g_param_spec_string (
      "feedback-profile",
      "Feedback profile",
      "Feedback profile to use for this event",
      NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  signals[SIGNAL_FEEDBACK_ENDED] = g_signal_new ("feedback-ended",
                                                 G_TYPE_FROM_CLASS (klass),
                                                 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                                 NULL,
                                                 G_TYPE_NONE,
                                                 0);
}

static void
lfb_event_init (LfbEvent *self)
{
  self->timeout = -1;
  self->state = LFB_EVENT_STATE_NONE;
  self->end_reason = LFB_EVENT_END_REASON_NATURAL;
}

LfbEvent *
lfb_event_new (const char *event)
{
  return g_object_new (LFB_TYPE_EVENT, "event", event, NULL);
}

/* mock libfeedback functions */

static gboolean
emit_ended (LfbEvent *self)
{
  g_assert (LFB_IS_EVENT (self));

  g_signal_emit (self, signals[SIGNAL_FEEDBACK_ENDED], 0);

  return G_SOURCE_REMOVE;
}

static gboolean
on_mock_timeout (gpointer user_data)
{
  GTask *task;
  GCancellable *cancellable;

  if (!G_IS_TASK (user_data))
    return G_SOURCE_REMOVE;

  task = G_TASK (user_data);
  cancellable = g_task_get_cancellable (task);

  if (!g_cancellable_is_cancelled (cancellable)) {
    LfbEvent *event = g_task_get_source_object (task);
    LfbEventState state = GPOINTER_TO_INT (g_task_get_task_data (task));

    g_task_return_boolean (task, TRUE);

    if (state == LFB_EVENT_STATE_ENDED)
      g_idle_add (G_SOURCE_FUNC (emit_ended), event);
  }

  return G_SOURCE_REMOVE;
}

static gboolean
on_check_task_cancelled (gpointer user_data)
{
  GTask *task;
  LfbEvent *event;
  LfbEventState state;

  if (!G_IS_TASK (user_data))
    return G_SOURCE_REMOVE;

  task = G_TASK (user_data);
  event = g_task_get_source_object (task);
  state = GPOINTER_TO_INT (g_task_get_task_data (task));

  lfb_event_set_state (event, success ? state : LFB_EVENT_STATE_ERRORED);

  if (g_task_get_completed (task) || g_task_return_error_if_cancelled (task)) {
    g_object_unref (task);

    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

void
lfb_event_trigger_feedback_async (LfbEvent           *self,
                                  GCancellable       *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer            user_data)
{
  GTask *task;
  g_return_if_fail (LFB_IS_EVENT (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, GINT_TO_POINTER (LFB_EVENT_STATE_RUNNING), NULL);

  g_timeout_add (interval_timeout_ms, on_mock_timeout, task);
  g_idle_add (G_SOURCE_FUNC (on_check_task_cancelled), task);
}

gboolean
lfb_event_trigger_feedback_finish (LfbEvent     *self,
                                   GAsyncResult *res,
                                   GError      **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);

  return success;
}

gboolean
lfb_event_end_feedback_finish (LfbEvent     *self,
                               GAsyncResult *res,
                               GError      **error)
{
  g_return_val_if_fail (g_task_is_valid (res, self), FALSE);

  if (success)
    g_idle_add (G_SOURCE_FUNC (emit_ended), self);

  return success;
}

void
lfb_event_end_feedback_async (LfbEvent           *self,
                              GCancellable       *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer            user_data)
{
  GTask *task;
  g_return_if_fail (LFB_IS_EVENT (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, GINT_TO_POINTER (LFB_EVENT_STATE_ENDED), NULL);

  g_timeout_add (interval_timeout_ms, on_mock_timeout, task);
  g_idle_add (G_SOURCE_FUNC (on_check_task_cancelled), task);
}

void
lfb_event_set_timeout (LfbEvent *self, gint timeout)
{
  g_return_if_fail (LFB_IS_EVENT (self));

  if (self->timeout == timeout)
    return;

  self->timeout = timeout;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TIMEOUT]);
}

gint
lfb_event_get_timeout (LfbEvent *self)
{
  g_return_val_if_fail (LFB_IS_EVENT (self), -1);
  return self->timeout;
}

LfbEventState
lfb_event_get_state (LfbEvent *self)
{
  g_return_val_if_fail (LFB_IS_EVENT (self), LFB_EVENT_STATE_NONE);
  return self->state;
}

void
lfb_event_set_feedback_profile (LfbEvent *self, const gchar *profile)
{
  g_return_if_fail (LFB_IS_EVENT (self));

  if (!g_strcmp0 (self->profile, profile))
    return;

  g_free (self->profile);
  self->profile = g_strdup (profile);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FEEDBACK_PROFILE]);
}

char *
lfb_event_get_feedback_profile (LfbEvent *self)
{
  g_return_val_if_fail (LFB_IS_EVENT (self), NULL);

  return g_strdup (self->profile);
}
