#include "mock-libfeedback.h"

#include <setjmp.h>
#include <cmocka.h>


static gboolean
on_mock_timeout (gpointer user_data)
{
  GTask *task = user_data;

  g_task_return_boolean (task, TRUE);
  return G_SOURCE_REMOVE;
}

/* could probably also live without mocking/stubbing lfb_{un,}init()
 * and run under GTestDBus since we're actually only interested
 * in the feedback trigger and end functions
 */
gboolean
__wrap_lfb_init (const char *app_id,
                 GError    **error)
{
  return TRUE;
}


void
__wrap_lfb_uninit (void)
{
  return;
}


void
__wrap_lfb_event_set_feedback_profile (LfbEvent   *self,
                                       const char *profile)
{
  return;
}


void
__wrap_lfb_event_trigger_feedback_async (LfbEvent           *self,
                                         GCancellable       *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer            user_data)
{
  GTask *trigger_task = g_task_new (self, cancellable, callback, user_data);
  int mock_time_ms = mock_type (int);
  gboolean mock_will_be_cancelled = mock_type (gboolean);

  if (!mock_will_be_cancelled)
    g_timeout_add (mock_time_ms, on_mock_timeout, trigger_task);
}


gboolean
__wrap_lfb_event_trigger_feedback_finish (LfbEvent     *self,
                                          GAsyncResult *res,
                                          GError      **error)
{
  return TRUE;
}


void
__wrap_lfb_event_end_feedback_async (LfbEvent           *self,
                                     GCancellable       *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer            user_data)
{
  ;
}


gboolean
__wrap_lfb_event_end_feedback_finish (LfbEvent     *self,
                                      GAsyncResult *res,
                                      GError      **error)
{
  return TRUE;
}
