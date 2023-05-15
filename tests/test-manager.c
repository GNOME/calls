/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "calls-manager.h"
#include "calls-plugin-manager.h"
#include "calls-util.h"

#include <cui-call.h>
#include <glib.h>

struct TestData {
  GMainLoop          *loop;
  CallsManager       *manager;
  CallsPluginManager *plugin_manager;
  GListModel         *origins;
  GListModel         *origins_tel;
  CallsOrigin        *origin;
  CuiCall            *call;
};

static void
call_add_cb (CallsManager    *manager,
             CuiCall         *call,
             struct TestData *data)
{
  static guint phase = 0;
  g_autoptr (GError) error = NULL;

  data->call = call;
  switch (phase++) {
  case 0:
    cui_call_hang_up (call);
    break;

  case 1:
    /* Unload the provider */
    calls_plugin_manager_unload_plugin (data->plugin_manager, "dummy", &error);
    g_assert_false (calls_plugin_manager_has_plugin (data->plugin_manager, "dummy"));
    g_assert_false (calls_plugin_manager_has_any_plugins (data->plugin_manager));
    g_assert_cmpuint (g_list_model_get_n_items (calls_manager_get_origins (data->manager)),
                      ==, 0);

    break;

  default:
    g_assert_not_reached ();
  }
}

static void
call_remove_cb (CallsManager    *manager,
                CuiCall         *call,
                struct TestData *data)
{
  static guint phase = 0;

  g_assert_true (call == data->call);
  data->call = NULL;
  switch (phase++) {
  case 0:
    /* Add new call do check if we remove it when we unload the provider */
    calls_origin_dial (data->origin, "+393422342");
    break;

  case 1:
    g_main_loop_quit (data->loop);
    break;

  default:
    g_assert_not_reached ();
  }

}

static void
test_calls_manager_without_provider (void)
{
  GListModel *origins;
  CallsManager *manager = calls_manager_get_default ();
  CallsPluginManager *plugin_manager = calls_plugin_manager_get_default ();

  g_assert_true (CALLS_IS_MANAGER (manager));
  g_assert_true (CALLS_IS_PLUGIN_MANAGER (plugin_manager));

  g_assert_cmpuint (calls_manager_get_state_flags (manager), ==, CALLS_MANAGER_FLAGS_UNKNOWN);

  origins = calls_manager_get_origins (manager);
  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 0);

  g_assert_null (calls_manager_get_calls (manager));
  g_assert_false (calls_plugin_manager_has_any_plugins (plugin_manager));

  origins = calls_manager_get_suitable_origins (manager, "tel:+123456789");
  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 0);

  origins = calls_manager_get_suitable_origins (manager, "sip:alice@example.org");
  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 0);

  origins = calls_manager_get_suitable_origins (manager, "sips:bob@example.org");
  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 0);

  g_assert_finalize_object (manager);
  g_assert_finalize_object (plugin_manager);
}


static void
test_calls_manager_dummy_provider (void)
{
  g_autoptr (GError) error = NULL;
  CallsManager *manager;
  GListModel *origins;
  GListModel *origins_tel;
  guint position;
  struct TestData *test_data;

  test_data = g_new0 (struct TestData, 1);
  test_data->manager = calls_manager_get_default ();
  test_data->plugin_manager = calls_plugin_manager_get_default ();

  manager = test_data->manager;
  g_assert_true (CALLS_IS_MANAGER (manager));
  g_assert_cmpuint (calls_manager_get_state_flags (manager), ==, CALLS_MANAGER_FLAGS_UNKNOWN);


  test_data->origins = calls_manager_get_origins (manager);
  origins = test_data->origins;
  g_assert_true (origins);
  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 0);

  g_assert_true (calls_plugin_manager_load_plugin (test_data->plugin_manager, "dummy", &error));
  g_assert_no_error (error);

  g_assert_true (calls_plugin_manager_has_any_plugins (test_data->plugin_manager));
  g_assert_true (calls_plugin_manager_has_plugin (test_data->plugin_manager, "dummy"));
  /* Dummy plugin fakes being a modem */
  g_assert_cmpuint (calls_manager_get_state_flags (manager), ==,
                    CALLS_MANAGER_FLAGS_HAS_CELLULAR_PROVIDER |
                    CALLS_MANAGER_FLAGS_HAS_CELLULAR_MODEM);

  g_assert_cmpuint (g_list_model_get_n_items (origins), >, 0);
  g_assert_null (calls_manager_get_calls (manager));

  test_data->origin = g_list_model_get_item (origins, 0);
  g_assert_true (CALLS_IS_ORIGIN (test_data->origin));

  test_data->origins_tel = calls_manager_get_suitable_origins (manager, "tel:+393422342");
  origins_tel = test_data->origins_tel;
  g_assert_true (G_IS_LIST_MODEL (origins_tel));
  g_assert_true (calls_find_in_model (origins_tel, test_data->origin, &position));

  g_signal_connect (manager, "ui-call-added", G_CALLBACK (call_add_cb), test_data);
  g_signal_connect (manager, "ui-call-removed", G_CALLBACK (call_remove_cb), test_data);

  calls_origin_dial (test_data->origin, "+393422343");
  g_assert_true (CUI_IS_CALL (test_data->call));

  test_data->loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (test_data->loop);

  g_main_loop_unref (test_data->loop);

  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 0);
  g_assert_cmpuint (g_list_model_get_n_items (origins_tel), ==, 0);

  g_assert_cmpuint (calls_manager_get_state_flags (manager), ==, CALLS_MANAGER_FLAGS_UNKNOWN);

  g_assert_finalize_object (test_data->origin);
  g_assert_finalize_object (test_data->manager);
  g_assert_finalize_object (test_data->plugin_manager);

  g_free (test_data);
}


static void
test_calls_manager_mm_provider (void)
{
  GListModel *origins_tel;
  CallsManager *manager = calls_manager_get_default ();
  CallsPluginManager *plugin_manager = calls_plugin_manager_get_default ();
  g_autoptr (GError) error = NULL;

  g_assert_true (CALLS_IS_MANAGER (manager));
  g_assert_true (CALLS_IS_PLUGIN_MANAGER (plugin_manager));

  g_assert_cmpuint (calls_manager_get_state_flags (manager), ==, CALLS_MANAGER_FLAGS_UNKNOWN);

  g_assert_true (calls_plugin_manager_load_plugin (plugin_manager, "mm", &error));
  g_assert_no_error (error);
  g_assert_true (calls_plugin_manager_has_any_plugins (plugin_manager));
  g_assert_true (calls_plugin_manager_has_plugin (plugin_manager, "mm"));

  g_assert_cmpuint (calls_manager_get_state_flags (manager), >, CALLS_MANAGER_FLAGS_UNKNOWN);

  g_assert_null (calls_manager_get_calls (manager));

  origins_tel = calls_manager_get_suitable_origins (manager, "tel:+123456789");
  g_assert_nonnull (origins_tel);
  g_assert_cmpuint (g_list_model_get_n_items (origins_tel), ==, 0);

  g_assert_true (calls_plugin_manager_unload_plugin (plugin_manager, "mm", &error));
  g_assert_no_error (error);
  g_assert_cmpuint (calls_manager_get_state_flags (manager), ==, CALLS_MANAGER_FLAGS_UNKNOWN);

  g_assert_finalize_object (manager);
  g_assert_finalize_object (plugin_manager);
}


static void
test_calls_manager_multiple_providers_mm_sip (void)
{
  CallsManager *manager = calls_manager_get_default ();
  CallsPluginManager *plugin_manager = calls_plugin_manager_get_default ();
  GListModel *origins;
  GListModel *origins_tel;
  GListModel *origins_sip;
  GListModel *origins_sips;
  g_autoptr (GError) error = NULL;

  g_assert_true (CALLS_IS_MANAGER (manager));
  g_assert_true (CALLS_IS_PLUGIN_MANAGER (plugin_manager));

  origins = calls_manager_get_origins (manager);
  g_assert_true (G_IS_LIST_MODEL (origins));

  g_assert_cmpuint (calls_manager_get_state_flags (manager), ==, CALLS_MANAGER_FLAGS_UNKNOWN);

  origins_tel = calls_manager_get_suitable_origins (manager, "tel:+123456789");
  g_assert_nonnull (origins_tel);
  g_assert_cmpuint (g_list_model_get_n_items (origins_tel), ==, 0);

  origins_sip = calls_manager_get_suitable_origins (manager, "sip:alice@example.org");
  g_assert_nonnull (origins_sip);
  g_assert_cmpuint (g_list_model_get_n_items (origins_sip), ==, 0);

  origins_sips = calls_manager_get_suitable_origins (manager, "sips:bob@example.org");
  g_assert_nonnull (origins_sips);
  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 0);

  /* First add the SIP provider, MM provider later */
  g_assert_true (calls_plugin_manager_load_plugin (plugin_manager, "sip", &error));
  g_assert_cmpuint (calls_manager_get_state_flags (manager), ==, CALLS_MANAGER_FLAGS_HAS_VOIP_PROVIDER);

  /* Still no origins */
  origins_tel = calls_manager_get_suitable_origins (manager, "tel:+123456789");
  g_assert_cmpuint (g_list_model_get_n_items (origins_tel), ==, 0);

  origins_sip = calls_manager_get_suitable_origins (manager, "sip:alice@example.org");
  g_assert_cmpuint (g_list_model_get_n_items (origins_sip), ==, 0);

  origins_sips = calls_manager_get_suitable_origins (manager, "sips:bob@example.org");
  g_assert_cmpuint (g_list_model_get_n_items (origins_sips), ==, 0);

  /**
   * If we now load the MM plugin, the manager flag _HAS_CELLULAR_PROVIDER should be set
   * TODO DBus mock to add modems
   * see https://source.puri.sm/Librem5/calls/-/issues/280
   * and https://source.puri.sm/Librem5/calls/-/issues/178
   */
  g_assert_true (calls_plugin_manager_load_plugin (plugin_manager, "mm", &error));
  g_assert_cmpuint (calls_manager_get_state_flags (manager), ==,
                    CALLS_MANAGER_FLAGS_HAS_VOIP_PROVIDER |
                    CALLS_MANAGER_FLAGS_HAS_CELLULAR_PROVIDER);

  g_assert_finalize_object (manager);
  g_assert_finalize_object (plugin_manager);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/Manager/without_provider", test_calls_manager_without_provider);
  g_test_add_func ("/Calls/Manager/dummy_provider", test_calls_manager_dummy_provider);
  g_test_add_func ("/Calls/Manager/mm_provider", test_calls_manager_mm_provider);
  g_test_add_func ("/Calls/Manager/multiple_provider_mm_sip", test_calls_manager_multiple_providers_mm_sip);

  return g_test_run ();
}
