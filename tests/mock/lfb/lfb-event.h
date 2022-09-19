/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */
#pragma once

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* values used for mocking */
extern gboolean success;
extern guint interval_timeout_ms;


typedef enum _LfbEventState {
  LFB_EVENT_STATE_ERRORED = -1,
  LFB_EVENT_STATE_NONE    = 0,
  LFB_EVENT_STATE_RUNNING = 1,
  LFB_EVENT_STATE_ENDED   = 2,
} LfbEventState;

typedef enum _LfbEventEndReason {
  LFB_EVENT_END_REASON_NOT_FOUND = -1,
  LFB_EVENT_END_REASON_NATURAL   = 0,
  LFB_EVENT_END_REASON_EXPIRED   = 1,
  LFB_EVENT_END_REASON_EXPLICIT  = 2,
} LfbEventEndReason;

#define LFB_TYPE_EVENT (lfb_event_get_type())

G_DECLARE_FINAL_TYPE (LfbEvent, lfb_event, LFB, EVENT, GObject)

LfbEvent*   lfb_event_new (const char *event);
void        lfb_event_trigger_feedback_async (LfbEvent            *self,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data);
gboolean    lfb_event_trigger_feedback_finish (LfbEvent            *self,
                                               GAsyncResult        *res,
                                               GError             **error);
void        lfb_event_end_feedback_async (LfbEvent            *self,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data);
gboolean    lfb_event_end_feedback_finish (LfbEvent            *self,
                                           GAsyncResult        *res,
                                           GError             **error);
void        lfb_event_set_timeout (LfbEvent *self, gint timeout);
gint        lfb_event_get_timeout (LfbEvent *self);
void        lfb_event_set_feedback_profile (LfbEvent *self, const char *profile);
char       *lfb_event_get_feedback_profile (LfbEvent *self);
LfbEventState     lfb_event_get_state (LfbEvent *self);

G_END_DECLS
