/*
 * Copyright (C) 2023 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 */

#include "calls-dbus-config.h"

#include "calls-call-dbus.h"
#include "calls-emergency-call-dbus.h"
#include "cui-call.h"

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

  g_assert_no_error (error);
  g_assert_nonnull (proxy);

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
  CallsDBusEmergencyCalls *ec_manager;
  GMainLoop          *loop;
  guint32             pid_server;
  int                 test_phase;
  const char         *object_path;
  CallsDBusCallsCall *call;
} DBusTestFixture;


static void
fixture_setup (DBusTestFixture *fixture, gconstpointer unused)
{
  g_autoptr (GError) error = NULL;

  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  fixture->test_phase = 0;

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

  fixture->ec_manager =
    calls_dbus_emergency_calls_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
						       G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
						       CALLS_DBUS_NAME,
						       CALLS_DBUS_OBJECT_PATH,
						       NULL,
						       &error);
  g_assert_no_error (error);

  fixture->pid_server = find_pid_by_bus_name (CALLS_DBUS_NAME);
}


static void
fixture_teardown (DBusTestFixture *fixture, gconstpointer unused)
{
  g_assert_finalize_object (fixture->calls_manager);
  g_assert_finalize_object (fixture->ec_manager);

  g_test_dbus_down (fixture->dbus);
  g_clear_object (&fixture->dbus);

  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}


static void
on_call_state_changed (GObject    *object,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  g_autoptr (GError) error = NULL;
  CallsDBusCallsCall *call = CALLS_DBUS_CALLS_CALL (object);
  DBusTestFixture *fixture = user_data;
  const char *object_path = g_dbus_object_get_object_path (G_DBUS_OBJECT (object));

  g_assert_cmpstr (object_path, ==, fixture->object_path);

  switch (fixture->test_phase) {
  case 0:
    g_assert_cmpint (calls_dbus_calls_call_get_state (call), ==, CUI_CALL_STATE_ACTIVE);
    g_assert_true (calls_dbus_calls_call_call_hangup_sync (call, NULL, &error));
    g_assert_no_error (error);
    break;

  case 1:
    g_assert_cmpint (calls_dbus_calls_call_get_state (call), ==, CUI_CALL_STATE_DISCONNECTED);
    break;

  default:
    g_assert_not_reached ();
  }

  fixture->test_phase++;
}

static void
on_call_added (GDBusObjectManager *manager,
               GDBusObject        *object,
               gpointer            user_data)
{
  g_autoptr (GError) error = NULL;
  CallsDBusCallsCall *call;
  DBusTestFixture *fixture = user_data;

  fixture->object_path = g_dbus_object_get_object_path (object);

  call = calls_dbus_calls_call_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       CALLS_DBUS_NAME,
                                                       fixture->object_path,
                                                       NULL,
                                                       &error);

  g_assert_nonnull (call);
  g_assert_no_error (error);

  fixture->call = call;

  g_assert_cmpint (calls_dbus_calls_call_get_state (call), ==, CUI_CALL_STATE_INCOMING);

  g_signal_connect (call, "notify::state", G_CALLBACK (on_call_state_changed), user_data);

  g_assert_true (calls_dbus_calls_call_call_accept_sync (call, NULL, &error));
  g_assert_no_error (error);
}


static void
on_call_removed (GDBusObjectManager *manager,
                 GDBusObject        *object,
                 gpointer            user_data)
{
  DBusTestFixture *fixture = user_data;

  g_assert_cmpstr (g_dbus_object_get_object_path (object), ==, fixture->object_path);
  g_clear_object (&fixture->call);

  g_main_loop_quit (fixture->loop);
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


static void
test_interact_call (DBusTestFixture *fixture, gconstpointer unused)
{
  GList *objects;

  g_assert_cmpint (fixture->pid_server, >, 0);

  objects = g_dbus_object_manager_get_objects (fixture->calls_manager);

  g_assert_cmpint (g_list_length (objects), ==, 0);
  g_clear_list (&objects, g_object_unref);

  g_signal_connect (fixture->calls_manager, "object-added", G_CALLBACK (on_call_added), fixture);
  g_signal_connect (fixture->calls_manager, "object-added", G_CALLBACK (on_call_removed), fixture);

  /* Sending USR1 will trigger an incoming call on the dummy provider running on the server*/
  kill (fixture->pid_server, SIGUSR1);

  g_main_loop_run (fixture->loop);
}


static void
test_emergency_calls_numbers (DBusTestFixture *fixture, gconstpointer unused)
{
  gboolean success;
  gsize count;
  GVariantIter iter;
  const char *id = NULL;
  const char *name = NULL;
  gint32 source;
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) contacts = NULL;

  success =
    calls_dbus_emergency_calls_call_get_emergency_contacts_sync (fixture->ec_manager,
								 &contacts,
								 NULL,
								 &error);
  g_assert_no_error (error);
  g_assert_true (success);

  count = g_variant_n_children (contacts);
  g_assert_cmpint (count, ==, 2);

#define CONTACT_FORMAT "(&s&si@a{sv})"
  g_variant_iter_init (&iter, contacts);

  g_variant_iter_next (&iter, CONTACT_FORMAT, &id, &name, &source, NULL);
  g_assert_cmpstr (id, ==, "123");
  g_assert_cmpstr (name, ==, "123");
  g_assert_cmpint (source, ==, 0);
  g_variant_iter_next (&iter, CONTACT_FORMAT, &id, &name, &source, NULL);
  g_assert_cmpstr (id, ==, "456");
  g_assert_cmpstr (name, ==, "456");
  g_assert_cmpint (source, ==, 0);
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/Calls/DBus/no_calls", DBusTestFixture, NULL,
              fixture_setup, test_no_calls, fixture_teardown);
  g_test_add ("/Calls/DBus/call_interaction", DBusTestFixture, NULL,
              fixture_setup, test_interact_call, fixture_teardown);
  g_test_add ("/Calls/DBus/emergency-calls/numbers", DBusTestFixture, NULL,
              fixture_setup, test_emergency_calls_numbers, fixture_teardown);

  g_test_run ();
}
