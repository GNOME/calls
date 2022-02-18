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
  g_autoptr (CallsSettings) settings = calls_settings_new ();
  g_autoptr (CallsContactsProvider) contacts_provider =
    calls_contacts_provider_new (settings);
  CallsBestMatch *best_match;

  best_match = calls_contacts_provider_lookup_id (contacts_provider, NULL);
  g_assert_null (best_match);

  g_assert_cmpstr (calls_best_match_get_primary_info (best_match), ==, "Anonymous caller");
  g_assert_cmpstr (calls_best_match_get_secondary_info (best_match), ==, "");
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/contacts/null", (GTestFunc) test_contacts_null_contact);

  g_test_run ();
}
