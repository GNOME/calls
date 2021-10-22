#pragma once

#define LIBFEEDBACK_USE_UNSTABLE_API

#include <libfeedback.h>
#include <glib.h>


gboolean        __wrap_lfb_init                            (const char *app_id,
                                                            GError    **error);
void            __wrap_lfb_uninit                          (void);
void            __wrap_lfb_event_set_feedback_profile      (LfbEvent   *self,
                                                            const char *profile);

void            __wrap_lfb_event_trigger_feedback_async    (LfbEvent           *self,
                                                            GCancellable       *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer            user_data);
gboolean        __wrap_lfb_event_trigger_feedback_finish   (LfbEvent     *self,
                                                            GAsyncResult *res,
                                                            GError      **error);
void            __wrap_lfb_event_end_feedback_async        (LfbEvent           *self,
                                                            GCancellable       *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer            user_data);
gboolean        __wrap_lfb_event_end_feedback_finish       (LfbEvent     *self,
                                                            GAsyncResult *res,
                                                            GError      **error);
