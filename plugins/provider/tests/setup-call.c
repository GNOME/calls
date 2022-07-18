/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "setup-call.h"
#include "calls-origin.h"

void
test_dummy_call_set_up (CallFixture *fixture,
                        gconstpointer user_data)
{
  CallsOrigin *origin;
  GList *calls;

  test_dummy_origin_set_up (&fixture->parent, user_data);
  origin = CALLS_ORIGIN (fixture->parent.dummy_origin);

  calls_origin_dial (origin, TEST_CALL_NUMBER);

  calls = calls_origin_get_calls (origin);
  fixture->dummy_call = CALLS_DUMMY_CALL (calls->data);
  g_list_free (calls);
}


void
test_dummy_call_tear_down (CallFixture *fixture,
                           gconstpointer user_data)
{
  fixture->dummy_call = NULL;
  test_dummy_origin_tear_down (&fixture->parent, user_data);
}
