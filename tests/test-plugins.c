/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-config.h"

#include "calls-provider.h"
#include "calls-plugin-manager.h"

#include <glib.h>

static void
on_providers_changed (GListModel *model,
                      guint       position,
                      guint       removed,
                      guint       added,
                      gpointer    unused)
{
  g_autoptr (CallsProvider) provider = NULL;
  static guint phase = 0;

  switch (phase) {
  case 0:
  case 1:
  case 2:
  case 3:
    g_assert_cmpint (added, ==, 1);
    provider = g_list_model_get_item (model, position);
    g_assert_true (CALLS_IS_PROVIDER (provider));
    break;

  case 4:
  case 5:
  case 6:
  case 7:
    g_assert_cmpint (removed, ==, 1);
    break;

  default:
    g_assert_not_reached ();
  }

  phase++;
}

static void
test_calls_plugin_loading (void)
{
  CallsPluginManager *manager = calls_plugin_manager_get_default ();
  g_autoptr (GError) error = NULL;

  g_assert_false (calls_plugin_manager_has_any_plugins (manager));

  g_signal_connect (calls_plugin_manager_get_providers (manager),
                    "items-changed",
                    G_CALLBACK (on_providers_changed),
                    NULL);

  g_assert_true (calls_plugin_manager_load_plugin (manager, "dummy", &error));
  g_assert_no_error (error);
  g_assert_true (calls_plugin_manager_has_any_plugins (manager));
  g_assert_true (calls_plugin_manager_has_plugin (manager, "dummy"));

  g_assert_true (calls_plugin_manager_load_plugin (manager, "mm", &error));
  g_assert_no_error (error);
  g_assert_true (calls_plugin_manager_has_any_plugins (manager));
  g_assert_true (calls_plugin_manager_has_plugin (manager, "mm"));

  g_assert_true (calls_plugin_manager_load_plugin (manager, "sip", &error));
  g_assert_no_error (error);
  g_assert_true (calls_plugin_manager_has_any_plugins (manager));
  g_assert_true (calls_plugin_manager_has_plugin (manager, "sip"));

  g_assert_true (calls_plugin_manager_load_plugin (manager, "ofono", &error));
  g_assert_no_error (error);
  g_assert_true (calls_plugin_manager_has_any_plugins (manager));
  g_assert_true (calls_plugin_manager_has_plugin (manager, "ofono"));

  g_assert_false (calls_plugin_manager_load_plugin (manager, "not-a-plugin", &error));
  g_assert_nonnull (error);
  g_clear_error (&error);


  g_assert_true (calls_plugin_manager_unload_plugin (manager, "dummy", &error));
  g_assert_no_error (error);
  g_assert_false (calls_plugin_manager_has_plugin (manager, "dummy"));
  g_assert_true (calls_plugin_manager_has_any_plugins (manager));

  g_assert_true (calls_plugin_manager_unload_plugin (manager, "mm", &error));
  g_assert_no_error (error);
  g_assert_false (calls_plugin_manager_has_plugin (manager, "mm"));
  g_assert_true (calls_plugin_manager_has_any_plugins (manager));

  g_assert_true (calls_plugin_manager_unload_plugin (manager, "sip", &error));
  g_assert_no_error (error);
  g_assert_false (calls_plugin_manager_has_plugin (manager, "sip"));
  g_assert_true (calls_plugin_manager_has_any_plugins (manager));

  g_assert_true (calls_plugin_manager_unload_plugin (manager, "ofono", &error));
  g_assert_no_error (error);
  g_assert_false (calls_plugin_manager_has_plugin (manager, "ofono"));
  g_assert_false (calls_plugin_manager_has_any_plugins (manager));

  g_assert_cmpint (g_list_model_get_n_items (calls_plugin_manager_get_providers (manager)),
                   ==, 0);

  g_assert_finalize_object (manager);
}


static void
test_calls_plugin_unload_all (void)
{
  CallsPluginManager *manager = calls_plugin_manager_get_default ();
  g_autoptr (GError) error = NULL;

  g_assert_false (calls_plugin_manager_has_any_plugins (manager));

  g_assert_true (calls_plugin_manager_load_plugin (manager, "dummy", &error));
  g_assert_no_error (error);
  g_assert_true (calls_plugin_manager_has_plugin (manager, "dummy"));

  g_assert_true (calls_plugin_manager_load_plugin (manager, "mm", &error));
  g_assert_no_error (error);
  g_assert_true (calls_plugin_manager_has_plugin (manager, "mm"));

  g_assert_true (calls_plugin_manager_load_plugin (manager, "sip", &error));
  g_assert_no_error (error);
  g_assert_true (calls_plugin_manager_has_plugin (manager, "sip"));

  g_assert_true (calls_plugin_manager_has_any_plugins (manager));
  g_assert_true (calls_plugin_manager_unload_all_plugins (manager, &error));
  g_assert_no_error (error);
  g_assert_false (calls_plugin_manager_has_plugin (manager, "dummy"));
  g_assert_false (calls_plugin_manager_has_plugin (manager, "mm"));
  g_assert_false (calls_plugin_manager_has_plugin (manager, "sip"));
  g_assert_false (calls_plugin_manager_has_any_plugins (manager));

  g_assert_finalize_object (manager);
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/Plugins/load_plugins", test_calls_plugin_loading);
  g_test_add_func ("/Calls/Plugins/unload_all", test_calls_plugin_unload_all);

  return g_test_run ();
}
