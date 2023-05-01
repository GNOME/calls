/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "calls-settings.h"

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include <glib.h>
#include <gio/gio.h>

static void
test_default (void)
{
  g_autoptr (GSettings) gsettings = g_settings_new ("org.gnome.Calls");
  g_autoptr (CallsSettings) calls_settings = NULL;

  g_assert_null (g_settings_get_user_value (gsettings, "autoload-plugins"));
  g_assert_null (g_settings_get_user_value (gsettings, "preferred-audio-codecs"));

  /* we're testing if CallsSettings has the sideeffect of writing to the settings */
  calls_settings = calls_settings_get_default ();

  g_assert_null (g_settings_get_user_value (gsettings, "autoload-plugins"));
  g_assert_null (g_settings_get_user_value (gsettings, "preferred-audio-codecs"));
}


int
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/Settings/default", test_default);

  return g_test_run ();
}
