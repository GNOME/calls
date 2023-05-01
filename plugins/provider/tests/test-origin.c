/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "setup-origin.h"
#include "calls-message-source.h"
#include "calls-origin.h"
#include "common.h"

#include <glib.h>
#include <string.h>

static void
test_dummy_origin_object (OriginFixture *fixture,
                          gconstpointer user_data)
{
  g_assert_true (G_IS_OBJECT (fixture->dummy_origin));
  g_assert_true (CALLS_IS_MESSAGE_SOURCE (fixture->dummy_origin));
  g_assert_true (CALLS_IS_ORIGIN (fixture->dummy_origin));
}


static void
test_dummy_origin_getters (OriginFixture *fixture,
                           gconstpointer user_data)
{
  CallsOrigin *origin;
  g_autofree char *name = NULL;

  origin = CALLS_ORIGIN (fixture->dummy_origin);

  name = calls_origin_get_name (origin);
  g_assert_nonnull (name);
  g_assert_cmpstr (name, ==, TEST_ORIGIN_NAME);
  g_assert_null (calls_origin_get_country_code (origin));
}


static void
add_call (guint *add_count)
{
  ++(*add_count);
}


static void
test_dummy_origin_calls (OriginFixture *fixture,
                         gconstpointer user_data)
{
  static const guint ADDS = 2;
  CallsOrigin *origin = CALLS_ORIGIN (fixture->dummy_origin);
  guint i, add_count = 0;
  gulong handler;
  GList *calls;

  handler = g_signal_connect_swapped (origin, "call-added",
                                      G_CALLBACK (add_call), &add_count);
  g_assert_cmpuint (handler, >, 0);

  for (i = 1; i <= ADDS; ++i)
    {
      char number[8];
      snprintf (number, 2, "%u", i);
      calls_origin_dial (origin, number);
    }

  g_signal_handler_disconnect (origin, handler);

  g_assert_cmpuint (add_count, ==, ADDS);

  calls = calls_origin_get_calls (origin);
  g_assert_cmpuint (g_list_length (calls), ==, add_count);
  g_list_free (calls);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

#define add_test(name) add_calls_test(Origin, origin, name)

  add_test(object);
  add_test(getters);
  add_test(calls);

#undef add_test

  return g_test_run();
}
