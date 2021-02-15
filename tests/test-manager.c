/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "calls-manager.h"

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
  g_assert (call == test_call);
  test_call = NULL;
}

static void
test_calls_manager_without_provider ()
{
  g_autoptr (CallsManager) manager = calls_manager_new ();
  g_assert (CALLS_IS_MANAGER (manager));

  g_assert_null (calls_manager_get_provider (manager));
  g_assert (calls_manager_get_state (manager) == CALLS_MANAGER_STATE_NO_PROVIDER);
  g_assert_null (calls_manager_get_origins (manager));
  g_assert_null (calls_manager_get_calls (manager));
  g_assert_null (calls_manager_get_default_origin (manager));
}

static void
test_calls_manager_dummy_provider ()
{
  g_autoptr (CallsManager) manager = calls_manager_new ();
  GListModel *origins;
  CallsOrigin *origin;
  g_assert (CALLS_IS_MANAGER (manager));

  g_assert_null (calls_manager_get_provider (manager));
  g_assert (calls_manager_get_state (manager) == CALLS_MANAGER_STATE_NO_PROVIDER);
  g_assert_null (calls_manager_get_origins (manager));

  calls_manager_set_provider (manager, "dummy");
  g_assert_cmpstr (calls_manager_get_provider (manager), ==, "dummy");
  g_assert (calls_manager_get_state (manager) == CALLS_MANAGER_STATE_READY);
  g_assert_nonnull (calls_manager_get_origins (manager));

  origins = calls_manager_get_origins (manager);

  g_assert_nonnull (calls_manager_get_origins (manager));
  g_assert_true (g_list_model_get_n_items (origins) > 0);
  g_assert_null (calls_manager_get_calls (manager));
  g_assert_null (calls_manager_get_default_origin (manager));

  test_call = NULL;
  if (g_list_model_get_n_items (origins) > 0) {
    g_signal_connect (manager, "call-add", G_CALLBACK (call_add_cb), NULL);
    g_signal_connect (manager, "call-remove", G_CALLBACK (call_remove_cb), NULL);

    origin = g_list_model_get_item (origins, 0);
    g_assert (CALLS_IS_ORIGIN (origin));

    calls_manager_set_default_origin (manager, origin);
    g_assert (calls_manager_get_default_origin (manager) == origin);

    calls_origin_dial (origin, "+393422342");
    g_assert (CALLS_IS_CALL (test_call));
    calls_call_hang_up (test_call);
    g_assert_null (test_call);

    /* Add new call do check if we remove it when we unload the provider */
    calls_origin_dial (origin, "+393422342");
  }

  /* Unload the provider */
  calls_manager_set_provider (manager, NULL);

  g_assert_null (test_call);
  g_assert_null (calls_manager_get_origins (manager));

  g_assert (calls_manager_get_state (manager) == CALLS_MANAGER_STATE_NO_PROVIDER);
}

static void
test_calls_manager_mm_provider ()
{
  g_autoptr (CallsManager) manager = calls_manager_new ();
  g_assert (CALLS_IS_MANAGER (manager));

  g_assert_null (calls_manager_get_provider (manager));
  g_assert (calls_manager_get_state (manager) == CALLS_MANAGER_STATE_NO_PROVIDER);
  calls_manager_set_provider (manager, "mm");
  g_assert_cmpstr (calls_manager_get_provider (manager), ==, "mm");
  g_assert (calls_manager_get_state (manager) > CALLS_MANAGER_STATE_NO_PROVIDER);
  g_assert_null (calls_manager_get_calls (manager));
  g_assert_null (calls_manager_get_default_origin (manager));

  calls_manager_set_provider (manager, NULL);
  g_assert (calls_manager_get_state (manager) == CALLS_MANAGER_STATE_NO_PROVIDER);
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

  return g_test_run();
}
