/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 */

#include "calls-manager.h"
#include "calls-ringer.h"
#include "mock-call.h"
#include "mock-libfeedback.h"
#include "mock-contacts-provider.h"

#include <cmocka.h>
#include <setjmp.h>


/* mock libfeedback functions */

static gboolean
on_mock_timeout (gpointer user_data)
{
  GTask *task;
  GCancellable *cancellable;

  if (!G_IS_TASK (user_data))
    return G_SOURCE_REMOVE;

  task = G_TASK (user_data);
  cancellable = g_task_get_cancellable (task);

  if (!g_cancellable_is_cancelled (cancellable))
    g_task_return_boolean (task, TRUE);

  return G_SOURCE_REMOVE;
}

static gboolean
on_check_task_cancelled (gpointer user_data)
{
  GTask *task;

  if (!G_IS_TASK (user_data))
    return G_SOURCE_REMOVE;

  task = G_TASK (user_data);

  if (g_task_get_completed (task) || g_task_return_error_if_cancelled (task))
    return G_SOURCE_REMOVE;

  return G_SOURCE_CONTINUE;
}


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

  g_timeout_add (mock_time_ms, on_mock_timeout, trigger_task);
  g_idle_add (G_SOURCE_FUNC (on_check_task_cancelled), trigger_task);
}


gboolean
__wrap_lfb_event_trigger_feedback_finish (LfbEvent     *self,
                                          GAsyncResult *res,
                                          GError      **error)
{
  GTask *task = G_TASK (res);
  gboolean ret;

  ret = g_task_propagate_boolean (task, error);

  g_object_unref (task);

  return ret;
}


void
__wrap_lfb_event_end_feedback_async (LfbEvent           *self,
                                     GCancellable       *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer            user_data)
{
  GTask *trigger_task = g_task_new (self, cancellable, callback, user_data);
  int mock_time_ms = mock_type (int);

  g_timeout_add (mock_time_ms, on_mock_timeout, trigger_task);
}


gboolean
__wrap_lfb_event_end_feedback_finish (LfbEvent     *self,
                                      GAsyncResult *res,
                                      GError      **error)
{
  GTask *task = G_TASK (res);
  gboolean ret;

  ret = g_task_propagate_boolean (G_TASK (res), error);

  g_object_unref (task);

  return ret;
}


CallsContactsProvider *
__wrap_calls_contacts_provider_new (CallsSettings *settings)
{
  return NULL;
}


/* add or remove calls */

static void
add_call (CallsManager  *manager,
          CallsMockCall *call)
{
  g_assert (CALLS_IS_MANAGER (manager));
  g_assert (CALLS_IS_MOCK_CALL (call));

  g_signal_emit_by_name (manager, "call-add", CALLS_CALL (call), NULL);
}


static void
remove_call (CallsManager  *manager,
             CallsMockCall *call)
{
  g_assert (CALLS_IS_MANAGER (manager));
  g_assert (CALLS_IS_MOCK_CALL (call));

  g_signal_emit_by_name (manager, "call-remove", CALLS_CALL (call), NULL);
}

/* TestData setup and tear down */
typedef struct {
  CallsManager  *manager;
  CallsRinger   *ringer;
  CallsMockCall *call_one;
  CallsMockCall *call_two;
  GMainLoop     *loop;
} TestData;


static int
setup_test_data (void **state)
{
  TestData *data = g_new0 (TestData, 1);

  if (data == NULL)
    return -1;

  data->manager = calls_manager_get_default ();
  data->ringer = calls_ringer_new ();
  data->call_one = calls_mock_call_new ();
  data->call_two = calls_mock_call_new ();
  data->loop = g_main_loop_new (NULL, FALSE);

  *state = data;

  return 0;
}


static int
tear_down_test_data (void **state)
{
  TestData *data = *state;

  g_object_unref (data->call_one);
  g_object_unref (data->call_two);
  g_object_unref (data->ringer);
  g_main_loop_unref (data->loop);

  g_free (data);
  return 0;
}

/* t1: test_ringing_incoming_call */
static void
t1_on_ringer_call_accepted (CallsRinger *ringer,
                            GParamSpec  *pspec,
                            gpointer     user_data)
{
  static guint test_phase = 0;
  TestData *data = user_data;

  switch (test_phase++) {
  case 0: /* incoming call */
    assert_true (calls_ringer_get_is_ringing (ringer));
    calls_call_answer (CALLS_CALL (data->call_one));
    break;
  case 1: /* incoming call accepted */
    assert_false (calls_ringer_get_is_ringing (ringer));
    g_main_loop_quit ((GMainLoop *) data->loop);
    break;
  default:
    g_assert_not_reached (); /* did not find equivalent cmocka assertion */
  }
}


static void
test_ringing_accept_call (void **state)
{
  TestData *data = *state;

  assert_false (calls_ringer_get_is_ringing (data->ringer));

  g_signal_connect (data->ringer,
                    "notify::ringing",
                    G_CALLBACK (t1_on_ringer_call_accepted),
                    data);

  /* delay before completion of __wrap_lfb_event_trigger_feedback_async() */
  will_return (__wrap_lfb_event_trigger_feedback_async, 10);
  /* delay before completion of __wrap_lfb_event_end_feedback_async() */
  will_return (__wrap_lfb_event_end_feedback_async, 10);

  calls_call_set_state (CALLS_CALL (data->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (data->manager, data->call_one);

  /* main loop will quit in callback of notify::ring */
  g_main_loop_run (data->loop);

  remove_call (data->manager, data->call_one);
  assert_false (calls_ringer_get_is_ringing (data->ringer));
}

/* t2: test_ringing_hang_up_call */
static void
t2_on_ringer_call_hang_up (CallsRinger *ringer,
                           GParamSpec  *pspec,
                           gpointer     user_data)
{
  static guint test_phase = 0;
  TestData *data = user_data;

  switch (test_phase++) {
  case 0: /* incoming call */
    assert_true (calls_ringer_get_is_ringing (ringer));
    calls_call_hang_up (CALLS_CALL (data->call_one));
    break;
  case 1: /* incoming call hung up */
    assert_false (calls_ringer_get_is_ringing (ringer));
    g_main_loop_quit ((GMainLoop *) data->loop);
    break;
  default:
    g_assert_not_reached (); /* did not find equivalent cmocka assertion */
  }
}


static void
test_ringing_hang_up_call (void **state)
{
  TestData *data = *state;

  assert_false (calls_ringer_get_is_ringing (data->ringer));

  g_signal_connect (data->ringer,
                    "notify::ringing",
                    G_CALLBACK (t2_on_ringer_call_hang_up),
                    data);

  /* delay before completion of __wrap_lfb_event_trigger_feedback_async() */
  will_return (__wrap_lfb_event_trigger_feedback_async, 10);
  /* delay before completion of __wrap_lfb_event_end_feedback_async() */
  will_return (__wrap_lfb_event_end_feedback_async, 10);

  calls_call_set_state (CALLS_CALL (data->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (data->manager, data->call_one);

  /* main loop will quit in callback of notify::ring */
  g_main_loop_run (data->loop);

  remove_call (data->manager, data->call_one);
  assert_false (calls_ringer_get_is_ringing (data->ringer));
}


/* t3: test_ringing_hang_up_call_ringer_cancelled */
static gboolean
t3_on_ringer_timeout (gpointer user_data)
{
  TestData *data = user_data;
  static guint test_phase = 0;

  assert_false (calls_ringer_get_is_ringing (data->ringer));
  if (test_phase == 0) {
    calls_call_hang_up (CALLS_CALL (data->call_one));
    test_phase++;
    return G_SOURCE_CONTINUE;
  }

  g_main_loop_quit ((GMainLoop *) data->loop);

  return G_SOURCE_REMOVE;
}


/** this test should use cancellable code path by hanging up before
 *  event triggering completes
 */
static void
test_ringing_hang_up_call_ringer_cancelled (void **state)
{
  TestData *data = *state;

  assert_false (calls_ringer_get_is_ringing (data->ringer));

  /* delay before completion of __wrap_lfb_event_trigger_feedback_async() */
  will_return (__wrap_lfb_event_trigger_feedback_async, 50);

  calls_call_set_state (CALLS_CALL (data->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (data->manager, data->call_one);

  g_timeout_add (10, G_SOURCE_FUNC (t3_on_ringer_timeout), data);

  /* main loop will quit in t3_on_ringer_timeout() */
  g_main_loop_run (data->loop);

  remove_call (data->manager, data->call_one);
  assert_false (calls_ringer_get_is_ringing (data->ringer));
}

/* t4: test_ringing_silence_call */
static void
t4_on_ringer_call_silence (CallsRinger *ringer,
                           GParamSpec  *pspec,
                           gpointer     user_data)
{
  static guint test_phase = 0;
  TestData *data = user_data;

  switch (test_phase++) {
  case 0: /* incoming call */
    assert_true (calls_ringer_get_is_ringing (ringer));
    calls_call_silence_ring (CALLS_CALL (data->call_one));
    assert_true (calls_call_get_silenced (CALLS_CALL (data->call_one)));
    break;
  case 1: /* incoming call hung up */
    assert_false (calls_ringer_get_is_ringing (ringer));
    g_main_loop_quit ((GMainLoop *) data->loop);
    break;
  default:
    g_assert_not_reached (); /* did not find equivalent cmocka assertion */
  }
}


static void
test_ringing_silence_call (void **state)
{
  TestData *data = *state;

  assert_false (calls_ringer_get_is_ringing (data->ringer));

  g_signal_connect (data->ringer,
                    "notify::ringing",
                    G_CALLBACK (t4_on_ringer_call_silence),
                    data);

  /* delay before completion of __wrap_lfb_event_trigger_feedback_async() */
  will_return (__wrap_lfb_event_trigger_feedback_async, 10);
  /* delay before completion of __wrap_lfb_event_end_feedback_async() */
  will_return (__wrap_lfb_event_end_feedback_async, 10);

  calls_call_set_state (CALLS_CALL (data->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (data->manager, data->call_one);

  /* main loop will quit in callback of notify::ring */
  g_main_loop_run (data->loop);

  remove_call (data->manager, data->call_one);
  assert_false (calls_ringer_get_is_ringing (data->ringer));
}


/* t5: test_ringing_multiple_call */
static gboolean
t5_remove_calls (gpointer user_data)
{
  static guint test_phase = 0;
  TestData *data = user_data;

  if (test_phase == 0) {
    remove_call (data->manager, data->call_one);
    test_phase++;
    return G_SOURCE_CONTINUE;
  }

  assert_true (calls_ringer_get_is_ringing (data->ringer));
  remove_call (data->manager, data->call_two);

  return G_SOURCE_REMOVE;
}


static void
t5_on_ringer_multiple_calls (CallsRinger *ringer,
                             GParamSpec  *pspec,
                             gpointer     user_data)
{
  static guint test_phase = 0;
  TestData *data = user_data;

  switch (test_phase++) {
  case 0: /* add second call, and schedule call removal */
    assert_true (calls_ringer_get_is_ringing (ringer));
    add_call (data->manager, data->call_two);
    g_timeout_add (25, t5_remove_calls, data);
    break;
  case 1: /* both calls should be removed now */
    assert_false (calls_ringer_get_is_ringing (ringer));
    g_main_loop_quit ((GMainLoop *) data->loop);
    break;
  default:
    g_assert_not_reached (); /* did not find equivalent cmocka assertion */
  }
}


static void
test_ringing_multiple_calls (void **state)
{
  TestData *data = *state;

  assert_false (calls_ringer_get_is_ringing (data->ringer));

  g_signal_connect (data->ringer,
                    "notify::ringing",
                    G_CALLBACK (t5_on_ringer_multiple_calls),
                    data);

  /* delay before completion of __wrap_lfb_event_trigger_feedback_async() */
  will_return (__wrap_lfb_event_trigger_feedback_async, 10);
  /* delay before completion of __wrap_lfb_event_end_feedback_async() */
  will_return (__wrap_lfb_event_end_feedback_async, 10);

  calls_call_set_state (CALLS_CALL (data->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (data->manager, data->call_one);

  /* main loop will quit in callback of notify::ring */
  g_main_loop_run (data->loop);

  assert_false (calls_ringer_get_is_ringing (data->ringer));
}


static void
t6_on_ringer_multiple_calls_with_restart (CallsRinger *ringer,
                                          GParamSpec  *pspec,
                                          gpointer     user_data)
{
  static guint test_phase = 0;
  TestData *data = user_data;

  switch (test_phase++) {
  case 0:
    assert_true (calls_ringer_get_is_ringing (ringer));
    assert_false (calls_ringer_get_ring_is_quiet (ringer));

    calls_call_answer (CALLS_CALL (data->call_one));
    break;
  case 1:
    assert_true (calls_ringer_get_is_ringing (ringer));
    assert_true (calls_ringer_get_ring_is_quiet (ringer));

    calls_call_hang_up (CALLS_CALL (data->call_one));
    break;
  case 2:
    assert_true (calls_ringer_get_is_ringing (ringer));
    assert_false (calls_ringer_get_ring_is_quiet (ringer));

    calls_call_hang_up (CALLS_CALL (data->call_two));
    break;
  case 3:
    assert_false (calls_ringer_get_is_ringing (ringer));
    assert_false (calls_ringer_get_ring_is_quiet (ringer));

    g_main_loop_quit (data->loop);
    break;
  default:
    g_assert_not_reached (); /* did not find equivalent cmocka assertion */
  }
}

static void
test_ringing_multiple_calls_with_restart (void **state)
{
  TestData *data = *state;

  assert_false (calls_ringer_get_is_ringing (data->ringer));

  g_signal_connect (data->ringer,
                    "notify::ringing",
                    G_CALLBACK (t6_on_ringer_multiple_calls_with_restart),
                    data);

  /* delay before completion of __wrap_lfb_event_trigger_feedback_async() */
  will_return_always (__wrap_lfb_event_trigger_feedback_async, 10);
  /* delay before completion of __wrap_lfb_event_end_feedback_async() */
  will_return_always (__wrap_lfb_event_end_feedback_async, 10);

  calls_call_set_state (CALLS_CALL (data->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (data->manager, data->call_one);
  calls_call_set_state (CALLS_CALL (data->call_two), CALLS_CALL_STATE_INCOMING);
  add_call (data->manager, data->call_two);

  /* main loop will quit in callback of notify::ring */
  g_main_loop_run (data->loop);

  assert_false (calls_ringer_get_is_ringing (data->ringer));
}

int
main (int   argc,
      char *argv[])
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test_setup_teardown (test_ringing_accept_call,
                                     setup_test_data,
                                     tear_down_test_data),
    cmocka_unit_test_setup_teardown (test_ringing_hang_up_call,
                                     setup_test_data,
                                     tear_down_test_data),
    cmocka_unit_test_setup_teardown (test_ringing_hang_up_call_ringer_cancelled,
                                     setup_test_data,
                                     tear_down_test_data),
    cmocka_unit_test_setup_teardown (test_ringing_silence_call,
                                     setup_test_data,
                                     tear_down_test_data),
    cmocka_unit_test_setup_teardown (test_ringing_multiple_calls,
                                     setup_test_data,
                                     tear_down_test_data),
    cmocka_unit_test_setup_teardown (test_ringing_multiple_calls_with_restart,
                                     setup_test_data,
                                     tear_down_test_data)
  };

  return cmocka_run_group_tests (tests, NULL, NULL);
}

