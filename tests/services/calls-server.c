/*
 * Copyright (C) 2023 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 */

#include "calls-dbus-config.h"

#include "calls-manager.h"
#include "calls-dbus-manager.h"
#include "calls-plugin-manager.h"

#include <glib-unix.h>


static void
on_bus_acquired (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  CallsDBusManager *manager = user_data;
  g_autoptr (GError) error = NULL;

  g_print ("Bus acquired: %s\n", name);

  g_assert_true (calls_dbus_manager_register (manager, connection, CALLS_DBUS_OBJECT_PATH, &error));
  g_assert_no_error (error);
}


static void
on_name_acquired (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
  g_print ("Name acquired: %s\n", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const char      *name,
              gpointer         user_data)
{
  g_print ("Name lost: %s\n", name);
}


int
main (int argc, char *argv[])
{
  g_autoptr (CallsManager) manager = NULL;
  g_autoptr (CallsPluginManager) plugin_manager = NULL;
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  g_autoptr (GError) error = NULL;
  CallsDBusManager *dbus_manager = NULL;
  guint bus_id;

  /* Setup environment */
  g_setenv ("GSETTINGS_SCHEMA_DIR", CALLS_BUILD_DIR_STR "/data", TRUE);
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("CALLS_PLUGIN_DIR", CALLS_BUILD_DIR_STR "/plugins", TRUE);

  g_print ("Starting up DBus service\n");

  plugin_manager = calls_plugin_manager_get_default ();
  manager = calls_manager_get_default ();
  dbus_manager = calls_dbus_manager_new ();
  g_assert_true (calls_plugin_manager_load_plugin (plugin_manager, "dummy", &error));
  g_assert_no_error (error);

  bus_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                           CALLS_DBUS_NAME,
                           G_BUS_NAME_OWNER_FLAGS_NONE,
                           on_bus_acquired,
                           on_name_acquired,
                           on_name_lost,
                           dbus_manager,
                           NULL);

  g_main_loop_run (loop);

  g_print ("Shutting down DBus service\n");

  g_assert_true (calls_plugin_manager_unload_plugin (plugin_manager, "dummy", &error));
  g_assert_no_error (error);

  /* The DBus manager unexports any objects it may still have.
   * Do this before releasing the DBus name ownership */
  g_assert_finalize_object (dbus_manager);

  g_bus_unown_name (bus_id);

  return 0;
}
