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
#include "calls-ui-call-data.h"
#include "mock-call.h"


/* add or remove calls */

static void
add_call (CallsManager    *manager,
          CallsUiCallData *call)
{
  g_assert (CALLS_IS_MANAGER (manager));
  g_assert (CALLS_IS_UI_CALL_DATA (call));

  g_signal_emit_by_name (manager, "ui-call-added", call, NULL);
}


static void
remove_call (CallsManager    *manager,
             CallsUiCallData *call)
{
  g_assert (CALLS_IS_MANAGER (manager));
  g_assert (CALLS_IS_UI_CALL_DATA (call));

  g_signal_emit_by_name (manager, "ui-call-removed", call, NULL);
}

/* RingerFixture setup and tear down */
typedef struct {
  CallsManager    *manager;
  CallsRinger     *ringer;
  CallsMockCall   *call_one;
  CallsUiCallData *ui_call_one;
  CallsMockCall   *call_two;
  CallsUiCallData *ui_call_two;
  GMainLoop       *loop;
} RingerFixture;


static void
setup_ringer (RingerFixture *fixture,
              gconstpointer  user_data)
{

  fixture->manager = calls_manager_get_default ();
  fixture->ringer = calls_ringer_new ();
  fixture->call_one = calls_mock_call_new ();
  fixture->ui_call_one = calls_ui_call_data_new (CALLS_CALL (fixture->call_one), NULL);
  fixture->call_two = calls_mock_call_new ();
  fixture->ui_call_two = calls_ui_call_data_new (CALLS_CALL (fixture->call_two), NULL);
  fixture->loop = g_main_loop_new (NULL, FALSE);
}


static void
tear_down_ringer (RingerFixture *fixture,
                  gconstpointer  user_data)
{
  g_assert_finalize_object (fixture->ui_call_one);
  g_assert_finalize_object (fixture->call_one);
  g_assert_finalize_object (fixture->ui_call_two);
  g_assert_finalize_object (fixture->call_two);
  g_assert_finalize_object (fixture->ringer);
  g_main_loop_unref (fixture->loop);
}

/* t1: test_ringing_incoming_call */
static void
t1_on_ringer_call_accepted (CallsRinger *ringer,
                            GParamSpec  *pspec,
                            gpointer     user_data)
{
  static guint test_phase = 0;
  RingerFixture *fixture = user_data;

  switch (test_phase++) {
  case 0: /* incoming call */
    g_assert_cmpint (calls_ringer_get_state (ringer), ==, CALLS_RING_STATE_RINGING);
    calls_call_answer (CALLS_CALL (fixture->call_one));
    break;
  case 1: /* incoming call accepted */
    g_assert_cmpint (calls_ringer_get_state (ringer), ==, CALLS_RING_STATE_INACTIVE);
    g_main_loop_quit ((GMainLoop *) fixture->loop);
    break;
  default:
    g_assert_not_reached (); /* did not find equivalent cmocka assertion */
  }
}


static void
test_ringing_accept_call (RingerFixture *fixture,
                          gconstpointer  user_data)
{
  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);

  g_signal_connect (fixture->ringer,
                    "notify::state",
                    G_CALLBACK (t1_on_ringer_call_accepted),
                    fixture);

  calls_call_set_state (CALLS_CALL (fixture->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (fixture->manager, fixture->ui_call_one);

  /* main loop will quit in callback of notify::state */
  g_main_loop_run (fixture->loop);

  remove_call (fixture->manager, fixture->ui_call_one);
  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);
}

/* t2: test_ringing_hang_up_call */
static void
t2_on_ringer_call_hang_up (CallsRinger *ringer,
                           GParamSpec  *pspec,
                           gpointer     user_data)
{
  static guint test_phase = 0;
  RingerFixture *fixture = user_data;

  switch (test_phase++) {
  case 0: /* incoming call */
    g_assert_cmpint (calls_ringer_get_state (ringer), ==, CALLS_RING_STATE_RINGING);
    calls_call_hang_up (CALLS_CALL (fixture->call_one));
    break;
  case 1: /* incoming call hung up */
    g_assert_cmpint (calls_ringer_get_state (ringer), ==, CALLS_RING_STATE_INACTIVE);
    g_main_loop_quit ((GMainLoop *) fixture->loop);
    break;
  default:
    g_assert_not_reached (); /* did not find equivalent cmocka assertion */
  }
}


static void
test_ringing_hang_up_call (RingerFixture *fixture,
                           gconstpointer  user_data)
{
  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);

  g_signal_connect (fixture->ringer,
                    "notify::state",
                    G_CALLBACK (t2_on_ringer_call_hang_up),
                    fixture);

  calls_call_set_state (CALLS_CALL (fixture->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (fixture->manager, fixture->ui_call_one);

  /* main loop will quit in callback of notify::state */
  g_main_loop_run (fixture->loop);

  remove_call (fixture->manager, fixture->ui_call_one);
  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);
}


/* t3: test_ringing_silence_call */
static void
t3_on_ringer_call_silence (CallsRinger *ringer,
                           GParamSpec  *pspec,
                           gpointer     user_data)
{
  static guint test_phase = 0;
  RingerFixture *fixture = user_data;

  switch (test_phase++) {
  case 0: /* incoming call */
    g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_RINGING);
    calls_ui_call_data_silence_ring (fixture->ui_call_one);
    g_assert_true (calls_ui_call_data_get_silenced (fixture->ui_call_one));
    break;
  case 1: /* incoming call hung up */
    g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);
    g_main_loop_quit ((GMainLoop *) fixture->loop);
    break;
  default:
    g_assert_not_reached (); /* did not find equivalent cmocka assertion */
  }
}


static void
test_ringing_silence_call (RingerFixture *fixture,
                           gconstpointer  user_data)
{
  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);

  g_signal_connect (fixture->ringer,
                    "notify::state",
                    G_CALLBACK (t3_on_ringer_call_silence),
                    fixture);

  calls_call_set_state (CALLS_CALL (fixture->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (fixture->manager, fixture->ui_call_one);

  /* main loop will quit in callback of notify::state */
  g_main_loop_run (fixture->loop);

  remove_call (fixture->manager, fixture->ui_call_one);
  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);
}


/* t4: test_ringing_multiple_call */
static gboolean
t4_remove_calls (gpointer user_data)
{
  static guint test_phase = 0;
  RingerFixture *fixture = user_data;

  if (test_phase == 0) {
    remove_call (fixture->manager, fixture->ui_call_one);
    test_phase++;
    return G_SOURCE_CONTINUE;
  }

  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_RINGING);
  remove_call (fixture->manager, fixture->ui_call_two);

  return G_SOURCE_REMOVE;
}


static void
t4_on_ringer_multiple_calls (CallsRinger *ringer,
                             GParamSpec  *pspec,
                             gpointer     user_data)
{
  static guint test_phase = 0;
  RingerFixture *fixture = user_data;

  switch (test_phase++) {
  case 0: /* add second call, and schedule call removal */
    g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_RINGING);
    add_call (fixture->manager, fixture->ui_call_two);
    g_timeout_add (25, t4_remove_calls, fixture);
    break;
  case 1: /* both calls should be removed now */
    g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);
    g_main_loop_quit ((GMainLoop *) fixture->loop);
    break;
  default:
    g_assert_not_reached (); /* did not find equivalent cmocka assertion */
  }
}


static void
test_ringing_multiple_calls (RingerFixture *fixture,
                             gconstpointer  user_data)
{
  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);

  g_signal_connect (fixture->ringer,
                    "notify::state",
                    G_CALLBACK (t4_on_ringer_multiple_calls),
                    fixture);

  calls_call_set_state (CALLS_CALL (fixture->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (fixture->manager, fixture->ui_call_one);

  /* main loop will quit in callback of notify::state */
  g_main_loop_run (fixture->loop);

  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);
}


static void
t5_on_ringer_multiple_calls_with_restart (CallsRinger *ringer,
                                          GParamSpec  *pspec,
                                          gpointer     user_data)
{
  static guint test_phase = 0;
  RingerFixture *fixture = user_data;

  switch (test_phase++) {
  case 0:
    g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_RINGING);

    calls_call_answer (CALLS_CALL (fixture->call_one));
    break;
  case 1:
    g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_RINGING_SOFT);

    calls_call_hang_up (CALLS_CALL (fixture->call_one));
    break;
  case 2:
    g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_RINGING);

    calls_call_hang_up (CALLS_CALL (fixture->call_two));
    break;
  case 3:
    g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);

    g_main_loop_quit (fixture->loop);
    break;
  default:
    g_assert_not_reached ();
  }
}

static void
test_ringing_multiple_calls_with_restart (RingerFixture *fixture,
                                          gconstpointer  user_data)
{
  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);

  g_signal_connect (fixture->ringer,
                    "notify::state",
                    G_CALLBACK (t5_on_ringer_multiple_calls_with_restart),
                    fixture);

  calls_call_set_state (CALLS_CALL (fixture->call_one), CALLS_CALL_STATE_INCOMING);
  add_call (fixture->manager, fixture->ui_call_one);
  calls_call_set_state (CALLS_CALL (fixture->call_two), CALLS_CALL_STATE_INCOMING);
  add_call (fixture->manager, fixture->ui_call_two);

  /* main loop will quit in callback of notify::state */
  g_main_loop_run (fixture->loop);

  g_assert_cmpint (calls_ringer_get_state (fixture->ringer), ==, CALLS_RING_STATE_INACTIVE);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/Calls/Ringer/accept_call", RingerFixture, NULL,
              setup_ringer, test_ringing_accept_call, tear_down_ringer);
  g_test_add ("/Calls/Ringer/hang_up_call", RingerFixture, NULL,
              setup_ringer, test_ringing_hang_up_call, tear_down_ringer);
  g_test_add ("/Calls/Ringer/silence_call", RingerFixture, NULL,
              setup_ringer, test_ringing_silence_call, tear_down_ringer);
  g_test_add ("/Calls/Ringer/multiple_call", RingerFixture, NULL,
              setup_ringer, test_ringing_multiple_calls, tear_down_ringer);
  g_test_add ("/Calls/Ringer/multiple_call_restart", RingerFixture, NULL,
              setup_ringer, test_ringing_multiple_calls_with_restart, tear_down_ringer);

  return g_test_run ();
}
