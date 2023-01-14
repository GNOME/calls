/*
 * Copyright (C) 2023 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 */

#include "calls-dbus-config.h"

#include "calls-call-dbus.h"

#include <gtk/gtk.h>


/* returns the PID of the process owning the given name on the session bus, or -1 if not found */
static int
find_pid_by_bus_name (const char *name)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GDBusProxy) proxy =
    g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   NULL,
                                   "org.freedesktop.DBus",
                                   "/org/freesktop/DBus",
                                   "org.freedesktop.DBus",
                                   NULL,
                                   &error);
  g_autoptr (GVariant) ret = NULL;
  guint32 pid;

  g_assert_nonnull (proxy);
  g_assert_no_error (error);

  ret = g_dbus_proxy_call_sync (proxy,
                                "GetConnectionUnixProcessID",
                                g_variant_new ("(s)", name),
                                G_DBUS_CALL_FLAGS_NONE,
                                5000,
                                NULL,
                                &error);

  g_assert_no_error (error);

  g_variant_get (ret, "(u)", &pid);

  return pid;
}


typedef struct {
  GTestDBus          *dbus;
  GDBusObjectManager *calls_manager;
  GMainLoop          *loop;
  guint32             pid_server;
} DBusTestFixture;


static void
fixture_setup (DBusTestFixture *fixture, gconstpointer unused)
{
  g_autoptr (GError) error = NULL;

  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->dbus = g_test_dbus_new (G_TEST_DBUS_NONE);

  g_test_dbus_add_service_dir (fixture->dbus, CALLS_BUILD_DIR_STR "/tests/services");
  g_test_dbus_up (fixture->dbus);

  fixture->calls_manager =
    calls_dbus_object_manager_client_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                       CALLS_DBUS_NAME,
                                                       CALLS_DBUS_OBJECT_PATH,
                                                       NULL,
                                                       &error);

  if (fixture->calls_manager == NULL)
    g_error ("Error getting object manager client: %s", error->message);

  fixture->pid_server = find_pid_by_bus_name (CALLS_DBUS_NAME);
}


static void
fixture_teardown (DBusTestFixture *fixture, gconstpointer unused)
{
  g_assert_finalize_object (fixture->calls_manager);

  g_test_dbus_down (fixture->dbus);
  g_clear_object (&fixture->dbus);

  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}


static void
test_no_calls (DBusTestFixture *fixture, gconstpointer unused)
{
  GList *objects;

  g_assert_cmpint (fixture->pid_server, >, 0);

  objects = g_dbus_object_manager_get_objects (fixture->calls_manager);

  g_assert_cmpint (g_list_length (objects), ==, 0);
  g_clear_list (&objects, g_object_unref);
}


int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add ("/Calls/DBus/no_calls", DBusTestFixture, NULL,
              fixture_setup, test_no_calls, fixture_teardown);

  g_test_run ();
}
