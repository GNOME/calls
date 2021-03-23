/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-provider.h"

#include <gtk/gtk.h>
#include <libpeas/peas.h>

static CallsProvider *
get_plugin (const gchar *name)
{
  /* This is pretty much a copy of load_plugin () from calls-manager.c */
  g_autoptr (GError) error = NULL;
  PeasEngine *plugins;
  PeasPluginInfo *info;
  PeasExtension *extension;

  // Add Calls search path and rescan
  plugins = peas_engine_get_default ();

  // Find the plugin
  info = peas_engine_get_plugin_info (plugins, name);
  if (!info) {
      g_debug ("Could not find plugin `%s'", name);
      return NULL;
    }

  // Possibly load the plugin
  if (!peas_plugin_info_is_loaded (info)) {
    peas_engine_load_plugin (plugins, info);

    if (!peas_plugin_info_is_available (info, &error)) {
      g_debug ("Error loading plugin `%s': %s", name, error->message);
      return NULL;
    }

    g_debug ("Loaded plugin `%s'", name);
  }

  // Check the plugin provides CallsProvider
  if (!peas_engine_provides_extension (plugins, info, CALLS_TYPE_PROVIDER)) {
    g_debug ("Plugin `%s' does not have a provider extension", name);
    return NULL;
  }

  // Get the extension
  extension = peas_engine_create_extensionv (plugins, info, CALLS_TYPE_PROVIDER, 0, NULL);
  if (!extension) {
    g_debug ("Could not create provider from plugin `%s'", name);
    return NULL;
  }

  g_debug ("Created provider from plugin `%s'", name);
  return CALLS_PROVIDER (extension);
}

static void
test_calls_plugin_loading ()
{
  g_autoptr (CallsProvider) dummy_provider = NULL;
  g_autoptr (CallsProvider) mm_provider = NULL;
  g_autoptr (CallsProvider) ofono_provider = NULL;
  g_autoptr (CallsProvider) not_a_provider = NULL;

  dummy_provider = get_plugin ("dummy");
  g_assert_nonnull (dummy_provider);

  mm_provider = get_plugin ("mm");
  g_assert_nonnull (mm_provider);

  ofono_provider = get_plugin ("ofono");
  g_assert_nonnull (ofono_provider);

  not_a_provider = get_plugin ("not-a-valid-provider-plugin");
  g_assert_null (not_a_provider);
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

  g_test_add_func("/Calls/Plugins/load_plugins", test_calls_plugin_loading);

  return g_test_run();
}
