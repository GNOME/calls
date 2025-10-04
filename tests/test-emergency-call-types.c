/*
 * Copyright (C) 2022 The Phosh Developers
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "calls-emergency-call-types.h"

#include <glib.h>

static void
test_lookup (void)
{
  char *lookup = NULL;

  /* No country code -> no match */
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

  /* Lookup from service provider db */
  lookup = calls_emergency_call_type_get_name ("112", "yy");
  g_assert_cmpstr (lookup, ==, "Police, Ambulance, Fire Brigade");
  g_free (lookup);
}


static void
test_country (void)
{
  GStrv result = NULL;
  const char *const expected_xx[] = { "114", "117", "118", NULL };

  /* Hit the built in fallbacks */
  result = calls_emergency_call_types_get_numbers_by_country_code ("de");
  g_assert_cmpint (g_strv_length (result), ==, 1);
  g_strfreev (result);

  /* Hit the test data */
  result = calls_emergency_call_types_get_numbers_by_country_code ("xx");
  g_assert_cmpint (g_strv_length (result), ==, 3);
  g_assert_cmpstrv (result, expected_xx);
  g_strfreev (result);

  /* Hit the test data */
  result = calls_emergency_call_types_get_numbers_by_country_code ("yy");
  g_assert_cmpint (g_strv_length (result), ==, 1);
  g_strfreev (result);

  /* Hit the test data */
  result = calls_emergency_call_types_get_numbers_by_country_code ("zz");
  g_assert_null (result);

  result = calls_emergency_call_types_get_numbers_by_country_code ("doesnotexist");
  g_assert_null (result);
}


int
main (int   argc,
      char *argv[])
{
  gint ret;
  g_test_init (&argc, &argv, NULL);

  calls_emergency_call_types_init (TEST_DATABASE);

  g_test_add_func ("/Calls/EmergencyCallTypes/lookup", test_lookup);
  g_test_add_func ("/Calls/EmergencyCallTypes/country", test_country);

  ret = g_test_run ();

  calls_emergency_call_types_destroy ();
  return ret;
}
