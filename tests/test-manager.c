/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "calls-manager.h"
#include "calls-credentials.h"

#include <gtk/gtk.h>
#include <libpeas/peas.h>

CallsCall *test_call = NULL;

static void
call_add_cb (CallsManager *manager, CallsCall *call)
{
  test_call = call;
}

static void
call_remove_cb (CallsManager *manager, CallsCall *call)
{
  g_assert_true (call == test_call);
  test_call = NULL;
}

static void
test_calls_manager_without_provider (void)
{
  guint no_origins;
  GListModel *origins;
  g_autoptr (CallsManager) manager = calls_manager_new ();
  g_assert_true (CALLS_IS_MANAGER (manager));

  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_NO_PROVIDER);

  origins = calls_manager_get_origins (manager);
  no_origins = g_list_model_get_n_items (origins);
  g_assert_cmpuint (no_origins, ==, 0);

  g_assert_null (calls_manager_get_calls (manager));
  g_assert_false (calls_manager_has_any_provider (manager));
  g_assert_null (calls_manager_get_suitable_origins (manager, "tel:+123456789"));
  g_assert_null (calls_manager_get_suitable_origins (manager, "sip:alice@example.org"));
  g_assert_null (calls_manager_get_suitable_origins (manager, "sips:bob@example.org"));
}

static void
test_calls_manager_dummy_provider (void)
{
  g_autoptr (CallsManager) manager = calls_manager_new ();
  GListModel *origins;
  GListModel *origins_tel;
  guint position;
  g_autoptr (CallsOrigin) origin = NULL;

  g_assert_true (CALLS_IS_MANAGER (manager));
  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_NO_PROVIDER);

  origins = calls_manager_get_origins (manager);
  g_assert_true (origins);
  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 0);

  calls_manager_add_provider (manager, "dummy");
  g_assert_true (calls_manager_has_any_provider (manager));
  g_assert_true (calls_manager_has_provider (manager, "dummy"));
  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_READY);

  g_assert_cmpuint (g_list_model_get_n_items (origins), >, 0);
  g_assert_null (calls_manager_get_calls (manager));

  test_call = NULL;
  g_signal_connect (manager, "call-add", G_CALLBACK (call_add_cb), NULL);
  g_signal_connect (manager, "call-remove", G_CALLBACK (call_remove_cb), NULL);

  origin = g_list_model_get_item (origins, 0);
  g_assert_true (CALLS_IS_ORIGIN (origin));

  origins_tel = calls_manager_get_suitable_origins (manager, "tel:+393422342");
  g_assert_true (G_IS_LIST_MODEL (origins_tel));
  g_assert_true (G_IS_LIST_STORE (origins_tel));
  g_assert_true (g_list_store_find (G_LIST_STORE (origins_tel), origin, &position));

  calls_origin_dial (origin, "+393422342");
  g_assert_true (CALLS_IS_CALL (test_call));
  calls_call_hang_up (test_call);
  g_assert_null (test_call);

  /* Add new call do check if we remove it when we unload the provider */
  calls_origin_dial (origin, "+393422342");

  /* Unload the provider */
  calls_manager_remove_provider (manager, "dummy");
  g_assert_false (calls_manager_has_provider (manager, "dummy"));
  g_assert_false (calls_manager_has_any_provider (manager));

  g_assert_null (test_call);
  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 0);
  g_assert_cmpuint (g_list_model_get_n_items (origins_tel), ==, 0);

  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_NO_PROVIDER);
}

static void
test_calls_manager_mm_provider (void)
{
  GListModel *origins_tel;
  g_autoptr (CallsManager) manager = calls_manager_new ();
  g_assert_true (CALLS_IS_MANAGER (manager));

  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_NO_PROVIDER);

  calls_manager_add_provider (manager, "mm");
  g_assert_true (calls_manager_has_any_provider (manager));
  g_assert_true (calls_manager_has_provider (manager, "mm"));

  g_assert_cmpuint (calls_manager_get_state (manager), >, CALLS_MANAGER_STATE_NO_PROVIDER);

  g_assert_null (calls_manager_get_calls (manager));

  origins_tel = calls_manager_get_suitable_origins (manager, "tel:+123456789");
  g_assert_nonnull (origins_tel);
  g_assert_cmpuint (g_list_model_get_n_items (origins_tel), ==, 0);

  calls_manager_remove_provider (manager, "mm");
  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_NO_PROVIDER);
}

static void
test_calls_manager_multiple_providers_mm_sip (void)
{
  g_autoptr (CallsCredentials) alice = NULL;
  g_autoptr (CallsCredentials) bob = NULL;
  g_autoptr (CallsOrigin) origin_alice = NULL;
  g_autoptr (CallsOrigin) origin_bob = NULL;
  g_autoptr (CallsManager) manager = calls_manager_new ();
  GListModel *origins;
  GListModel *origins_tel;
  GListModel *origins_sip;
  GListModel *origins_sips;

  g_assert_true (CALLS_IS_MANAGER (manager));

  origins = calls_manager_get_origins (manager);
  g_assert_true (G_IS_LIST_MODEL (origins));

  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_NO_PROVIDER);
  g_assert_null (calls_manager_get_suitable_origins (manager, "tel:+123456789"));
  g_assert_null (calls_manager_get_suitable_origins (manager, "sip:alice@example.org"));
  g_assert_null (calls_manager_get_suitable_origins (manager, "sips:bob@example.org"));

  /* First add the SIP provider, MM provider later */
  calls_manager_add_provider (manager, "sip");
  g_assert_true (calls_manager_has_any_provider (manager));
  g_assert_true (calls_manager_has_provider (manager, "sip"));
  g_assert_true (calls_manager_is_modem_provider (manager, "sip") == FALSE);

  /* Now we should have (empty) GListModels for the suitable origins for our protocols */
  origins_tel = calls_manager_get_suitable_origins (manager, "tel:+123456789");
  g_assert_nonnull (origins_tel);
  g_assert_cmpuint (g_list_model_get_n_items (origins_tel), ==, 0);

  origins_sip = calls_manager_get_suitable_origins (manager, "sip:alice@example.org");
  g_assert_nonnull (origins_sip);
  g_assert_cmpuint (g_list_model_get_n_items (origins_sip), ==, 0);

  origins_sips = calls_manager_get_suitable_origins (manager, "sips:bob@example.org");
  g_assert_nonnull (origins_sips);
  g_assert_cmpuint (g_list_model_get_n_items (origins_sips), ==, 0);

  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_NO_ORIGIN);

  /* Add Alice SIP account */
  alice = calls_credentials_new ();
  g_object_set (alice,
                "name", "Alice",
                "user", "alice",
                "host", "example.org",
                "password", "password123",
                NULL);
  g_assert_true (calls_manager_provider_add_account (manager, "sip", alice));
  g_assert_false (calls_manager_provider_add_account (manager, "sip", alice));

  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_READY);
  g_assert_cmpuint (g_list_model_get_n_items (origins_sip), ==, 1);

  /**
   * Add a second SIP origin to mix things up.
   * TODO We can expand on this later to test the call routing
   * starting with a simple "default" mechanism for now
   * needs https://source.puri.sm/Librem5/calls/-/issues/259 first though
   */
  bob = calls_credentials_new ();
  g_object_set (bob,
                "name", "Bob",
                "user", "bob",
                "host", "example.org",
                "password", "password123",
                NULL);

  g_assert_true (calls_manager_provider_add_account (manager, "sip", bob));

  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_READY);
  g_assert_cmpuint (g_list_model_get_n_items (origins_sip), ==, 2);

  /**
   * If we now load the MM plugin, the manager state should be *_STATE_NO_VOICE_MODEM
   * (unless run on a phone I guess?)
   * TODO DBus mock to add modems
   * see https://source.puri.sm/Librem5/calls/-/issues/280
   * and https://source.puri.sm/Librem5/calls/-/issues/178
   */
  calls_manager_add_provider (manager, "mm");
  g_assert_true (calls_manager_has_any_provider (manager));
  g_assert_true (calls_manager_has_provider (manager, "mm"));
  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_NO_VOICE_MODEM);

  /* Remove alice */
  g_assert_true (calls_manager_provider_remove_account (manager, "sip", alice));
  g_assert_false (calls_manager_provider_remove_account (manager, "sip", alice));
  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_NO_VOICE_MODEM);
  g_assert_cmpuint (g_list_model_get_n_items (origins_sip), ==, 1);

  /* Unload MM plugin, since we still have Bob we should be ready (and bob should be the default sip origin) */
  calls_manager_remove_provider (manager, "mm");
  g_assert_true (calls_manager_has_any_provider (manager));
  g_assert_cmpuint (calls_manager_get_state (manager), ==, CALLS_MANAGER_STATE_READY);

  g_assert_true (calls_manager_provider_remove_account (manager, "sip", bob));
  g_assert_cmpuint (g_list_model_get_n_items (origins_sip), ==, 0);
}

gint
main (gint   argc,
      gchar *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  /* Add builddir as search path */
#ifdef PLUGIN_BUILDDIR
  peas_engine_add_search_path (peas_engine_get_default (), PLUGIN_BUILDDIR, NULL);
#endif

  g_test_add_func("/Calls/Manager/without_provider", test_calls_manager_without_provider);
  g_test_add_func("/Calls/Manager/dummy_provider", test_calls_manager_dummy_provider);
  g_test_add_func("/Calls/Manager/mm_provider", test_calls_manager_mm_provider);
  g_test_add_func("/Calls/Manager/multiple_provider_mm_sip", test_calls_manager_multiple_providers_mm_sip);

  return g_test_run();
}
