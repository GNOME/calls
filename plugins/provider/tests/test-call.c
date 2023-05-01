/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "setup-call.h"
#include "calls-message-source.h"
#include "calls-call.h"
#include "calls-origin.h"
#include "common.h"

#include <glib.h>
#include <string.h>

static void
test_dummy_call_object (CallFixture   *fixture,
                        gconstpointer  user_data)
{
  g_assert_true (G_IS_OBJECT (fixture->dummy_call));
  g_assert_true (CALLS_IS_MESSAGE_SOURCE (fixture->dummy_call));
  g_assert_true (CALLS_IS_CALL (fixture->dummy_call));
}


static void
test_dummy_call_get_id (CallFixture   *fixture,
                        gconstpointer  user_data)
{
  const char *id;
  id = calls_call_get_id (CALLS_CALL (fixture->dummy_call));
  g_assert_nonnull (id);
  g_assert_cmpstr (id, ==, TEST_CALL_NUMBER);
}

static void
test_dummy_call_get_state (CallFixture   *fixture,
                           gconstpointer  user_data)
{
  CallsCallState state;
  state = calls_call_get_state (CALLS_CALL (fixture->dummy_call));
  g_assert_true (state == CALLS_CALL_STATE_DIALING);
}


static gboolean
test_dummy_call_hang_up_idle_cb (CallsDummyOrigin *origin)
{
  g_assert_null (calls_origin_get_calls (CALLS_ORIGIN (origin)));
  return G_SOURCE_REMOVE;
}

static void
test_dummy_call_hang_up (CallFixture   *fixture,
                         gconstpointer  user_data)
{
  calls_call_hang_up (CALLS_CALL (fixture->dummy_call));

  // Mirror the dummy origin's use of an idle callback
  g_idle_add ((GSourceFunc)test_dummy_call_hang_up_idle_cb,
              fixture->parent.dummy_origin);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

#define add_test(name) add_calls_test(Call, call, name)

  add_test(object);
  add_test(get_id);
  add_test(get_state);
  add_test(hang_up);

#undef add_test

  return g_test_run();
}
