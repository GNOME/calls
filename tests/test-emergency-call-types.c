/*
 * Copyright (C) Guido Günther
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "calls-emergency-call-types.h"

#include <glib.h>

static void
test_lookup (void)
{
  char *lookup = NULL;

  /* No countyr code -> no match */
  lookup = calls_emergency_call_type_get_name ("112", NULL);
  g_assert_null (lookup);

  /* Country code that's not in the table */
  lookup = calls_emergency_call_type_get_name ("112", "doesnotexist");
  g_assert_null (lookup);

  /* Numbers that match a single type */
  lookup = calls_emergency_call_type_get_name ("117", "CH");
  g_assert_cmpstr (lookup, ==, "Police");
  g_free (lookup);

  lookup = calls_emergency_call_type_get_name ("144", "CH");
  g_assert_cmpstr (lookup, ==, "Ambulance");
  g_free (lookup);

  /* Numbers that match multiple types */
  lookup = calls_emergency_call_type_get_name ("112", "DE");
  g_assert_cmpstr (lookup, ==, "Police, Ambulance, Fire Brigade");
  g_free (lookup);

  /* Numbers that doesn't match */
  lookup = calls_emergency_call_type_get_name ("123456", "DE");
  g_assert_null (lookup);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/EmergencyCallTypes/lookup", test_lookup);

  return g_test_run ();
}
