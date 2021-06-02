/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-account.h"
#include "calls-account-provider.h"
#include "calls-provider.h"
#include "calls-sip-provider.h"

#include <gtk/gtk.h>

#include <sofia-sip/su_uniqueid.h>
#include <libpeas/peas.h>

static void
test_account_basic (void)
{
  CallsCredentials *alice = calls_credentials_new ();
  CallsCredentials *bob = calls_credentials_new ();
  CallsSipProvider *sip =
    CALLS_SIP_PROVIDER (calls_provider_load_plugin ("sip"));
  CallsAccountProvider *acc_provider;
  GListModel *origins;
  CallsOrigin *origin_alice;
  CallsOrigin *origin_bob;

  g_assert_true (CALLS_IS_ACCOUNT_PROVIDER (sip));
  acc_provider = CALLS_ACCOUNT_PROVIDER (sip);

  g_object_set (alice,
                "name", "Alice",
                "user", "alice",
                "host", "example.org",
                "password", "password123",
                NULL);
  g_object_set (bob,
                "name", "Bob",
                "user", "bob",
                "host", "example.org",
                "password", "password123",
                NULL);

  /* Add credentials */
  g_assert_true (calls_account_provider_add_account (acc_provider, alice));
  g_assert_true (calls_account_provider_add_account (acc_provider, bob));

  /* Are the returned accounts of the correct types? */
  g_assert_true (CALLS_IS_ACCOUNT (calls_account_provider_get_account (acc_provider, alice)));
  g_assert_true (CALLS_IS_ORIGIN (calls_account_provider_get_account (acc_provider, alice)));

  g_assert_true (CALLS_IS_ACCOUNT (calls_account_provider_get_account (acc_provider, bob)));
  g_assert_true (CALLS_IS_ORIGIN (calls_account_provider_get_account (acc_provider, bob)));

  /* Are we getting the correct corresponding origins back? */
  origins = calls_provider_get_origins (CALLS_PROVIDER (sip));

  g_assert_cmpint (g_list_model_get_n_items (origins), ==, 2);

  origin_alice = g_list_model_get_item (origins, 0);
  origin_bob = g_list_model_get_item (origins, 1);

  g_assert_true (origin_alice ==
                 CALLS_ORIGIN (calls_account_provider_get_account (acc_provider, alice)));
  g_assert_true (origin_bob ==
                 CALLS_ORIGIN (calls_account_provider_get_account (acc_provider, bob)));

  g_object_unref (origin_alice);
  g_object_unref (origin_bob);

  /* Try adding credentials a second time */
  g_assert_false (calls_account_provider_add_account (acc_provider, alice));

  /* Remove credentials */
  g_assert_true (calls_account_provider_remove_account (acc_provider, alice));
  g_assert_false (calls_account_provider_remove_account (acc_provider, alice));
  g_assert_true (calls_account_provider_remove_account (acc_provider, bob));
  g_assert_false (calls_account_provider_remove_account (acc_provider, bob));

  g_assert_cmpint (g_list_model_get_n_items (origins), ==, 0);
}

gint
main (gint   argc,
      gchar *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

#ifdef PLUGIN_BUILDDIR
  peas_engine_add_search_path (peas_engine_get_default (), PLUGIN_BUILDDIR, NULL);
#endif
  /* this is a workaround for an issue with sofia: https://github.com/freeswitch/sofia-sip/issues/58 */
  su_random64 ();

  g_test_add_func ("/Calls/Account/basic", test_account_basic);

  return g_test_run();
}
