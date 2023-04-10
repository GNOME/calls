/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-best-match.h"
#include "calls-contacts-provider.h"

#include <gtk/gtk.h>

static void
test_contacts_null_contact (void)
{
  g_autoptr (CallsContactsProvider) contacts_provider =
    calls_contacts_provider_new ();
  g_autoptr (CallsBestMatch) best_match = NULL;

  best_match = calls_contacts_provider_lookup_id (contacts_provider, NULL);
  g_assert_nonnull (best_match);

  g_assert_cmpstr (calls_best_match_get_primary_info (best_match), ==, "Anonymous caller");
  g_assert_cmpstr (calls_best_match_get_secondary_info (best_match), ==, "");
}


static void
test_contacts_empty_contact (void)
{
  g_autoptr (CallsContactsProvider) contacts_provider =
    calls_contacts_provider_new ();
  g_autoptr (CallsBestMatch) best_match = NULL;

  best_match = calls_contacts_provider_lookup_id (contacts_provider, "");
  g_assert_nonnull (best_match);

  g_assert_cmpstr (calls_best_match_get_primary_info (best_match), ==, "Anonymous caller");
  g_assert_cmpstr (calls_best_match_get_secondary_info (best_match), ==, "");
}


static void
test_contacts_unknown_contact (void)
{
  g_autoptr (CallsContactsProvider) contacts_provider =
    calls_contacts_provider_new ();
  g_autoptr (CallsBestMatch) best_match = NULL;
  const char *id = "111222333444555666";

  best_match = calls_contacts_provider_lookup_id (contacts_provider, id);
  g_assert_nonnull (best_match);

  g_assert_cmpstr (calls_best_match_get_primary_info (best_match), ==, id);
  g_assert_cmpstr (calls_best_match_get_secondary_info (best_match), ==, "");
}

/* TODO set up folks keyfile backend to test "known" contact */

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/contacts/null", (GTestFunc) test_contacts_null_contact);
  g_test_add_func ("/Calls/contacts/empty", (GTestFunc) test_contacts_empty_contact);
  g_test_add_func ("/Calls/contacts/unknown", (GTestFunc) test_contacts_unknown_contact);

  g_test_run ();
}
