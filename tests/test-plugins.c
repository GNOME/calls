/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "config.h"

#include "calls-provider.h"

#include <gtk/gtk.h>
#include <libpeas/peas.h>

static void
test_calls_plugin_loading (void)
{
  g_autoptr (CallsProvider) dummy_provider = NULL;
  g_autoptr (CallsProvider) mm_provider = NULL;
  g_autoptr (CallsProvider) ofono_provider = NULL;
  g_autoptr (CallsProvider) sip_provider = NULL;
  g_autoptr (CallsProvider) not_a_provider = NULL;

  dummy_provider = calls_provider_load_plugin ("dummy");
  g_assert_nonnull (dummy_provider);

  mm_provider = calls_provider_load_plugin ("mm");
  g_assert_nonnull (mm_provider);

  ofono_provider = calls_provider_load_plugin ("ofono");
  g_assert_nonnull (ofono_provider);

  sip_provider = calls_provider_load_plugin ("sip");
  g_assert_nonnull (sip_provider);

  not_a_provider = calls_provider_load_plugin ("not-a-valid-provider-plugin");
  g_assert_null (not_a_provider);

  calls_provider_unload_plugin ("dummy");
  calls_provider_unload_plugin ("mm");
  calls_provider_unload_plugin ("ofono");
  calls_provider_unload_plugin ("sip");
}


gint
main (gint   argc,
      gchar *argv[])
{
  g_autofree char *plugin_dir_provider = NULL;
  PeasEngine *peas;
  const gchar *dir;
  g_autofree char *default_plugin_dir_provider = NULL;

  gtk_test_init (&argc, &argv, NULL);

  peas = peas_engine_get_default ();

  /* Add builddir as search path */
#ifdef PLUGIN_BUILDDIR
  plugin_dir_provider = g_build_filename (PLUGIN_BUILDDIR, "provider", NULL);
  peas_engine_add_search_path (peas_engine_get_default (), plugin_dir_provider, NULL);
#endif

  dir = g_getenv ("CALLS_PLUGIN_DIR");
  if (dir && dir[0] != '\0') {
    g_autofree char *plugin_dir_provider = NULL;
    plugin_dir_provider = g_build_filename (dir, "provider", NULL);
    g_debug ("Adding %s to plugin search path", plugin_dir_provider);
    peas_engine_prepend_search_path (peas, plugin_dir_provider, NULL);
  }

  default_plugin_dir_provider = g_build_filename (PLUGIN_LIBDIR, "provider", NULL);
  g_debug ("Adding %s to plugin search path", default_plugin_dir_provider);
  peas_engine_add_search_path (peas, default_plugin_dir_provider, NULL);
  peas_engine_rescan_plugins (peas);

  g_test_add_func("/Calls/Plugins/load_plugins", test_calls_plugin_loading);

  return g_test_run();
}
