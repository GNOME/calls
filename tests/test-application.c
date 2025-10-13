/*
 * Copyright (C) 2023 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-application.h"

#include <glib/gstdio.h>
#include <glib.h>

#include "calls-plugin-manager.h"
#include "calls-settings.h"

static const char *expected_plugins = NULL; /* comma separated string of plugins, see on_idle_quit() */


#define calls_assert_in_dir(file, dir) G_STMT_START {                   \
  g_autofree char *__p = g_build_path ("/", dir, file, NULL);           \
  g_autoptr (GFile) __f = g_file_new_for_path (__p);                    \
  if (!g_file_query_exists (__f, NULL)) {                               \
    g_autofree char *__msg =                                            \
      g_strdup_printf ("File %s does not exist", __p);                  \
    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, __msg); \
  }                                                                     \
} G_STMT_END


#define calls_assert_not_in_dir(file, dir) G_STMT_START {               \
  g_autofree char *__p = g_build_path ("/", dir, file, NULL);           \
  g_autoptr (GFile) __f = g_file_new_for_path (__p);                    \
  if (g_file_query_exists (__f, NULL)) {                                \
    g_autofree char *__msg =                                            \
      g_strdup_printf ("File %s must not exist", __p);                  \
    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, __msg); \
  }                                                                     \
} G_STMT_END


static gboolean
on_idle_quit (gpointer user_data)
{
  GApplication *app = user_data;
  CallsPluginManager *plugins = calls_plugin_manager_get_default ();
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) expected = NULL;
  uint n_plugins;

  if (expected_plugins) {
    expected = g_strsplit (expected_plugins, ",", -1);
  } else {
    CallsSettings *settings = calls_settings_get_default ();
    expected = calls_settings_get_autoload_plugins (settings);
  }

  n_plugins = g_strv_length (expected);
  /* we always should have *some* plugin, either the defaults, or explicitly loaded ones */
  g_assert_cmpuint (n_plugins, >, 0);

  for (uint i = 0; i < n_plugins; i++) {
    g_assert_true (calls_plugin_manager_has_plugin (plugins, expected[i]));
  }

  g_application_quit (app);

  g_assert_true (calls_plugin_manager_has_any_plugins (plugins));
  g_assert_true (calls_plugin_manager_unload_all_plugins (plugins, &error));
  g_assert_no_error (error);
  g_assert_false (calls_plugin_manager_has_any_plugins (plugins));

  return G_SOURCE_REMOVE;
}


static void
test_application_shutdown_daemon (void)
{
  CallsApplication *app = calls_application_new ();
  char *argv[] = { "test", "--gapplication-service", NULL };
  int status;

  expected_plugins = NULL;
  g_idle_add (on_idle_quit, app);

  status = g_application_run (G_APPLICATION (app), G_N_ELEMENTS (argv), argv);
  g_assert_cmpint (status, ==, 0);

  g_assert_finalize_object (app);
}

static void
test_application_shutdown_no_daemon (void)
{
  CallsApplication *app = calls_application_new ();
  char *argv[] = { "test", NULL };
  int status;

  expected_plugins = NULL;
  g_idle_add (on_idle_quit, app);

  status = g_application_run (G_APPLICATION (app), G_N_ELEMENTS (argv), argv);
  g_assert_cmpint (status, ==, 0);

  g_assert_finalize_object (app);
}


static void
test_application_shutdown_no_daemon_dummy (void)
{
  CallsApplication *app = calls_application_new ();
  char *argv[] = { "test", "-p", "dummy", NULL };
  int status;

  expected_plugins = "dummy";
  g_idle_add (on_idle_quit, app);

  status = g_application_run (G_APPLICATION (app), G_N_ELEMENTS (argv), argv);
  g_assert_cmpint (status, ==, 0);

  g_assert_finalize_object (app);
}


static void
test_application_shutdown_delayed (void)
{
  CallsApplication *app = calls_application_new ();
  char *argv[] = { "test", "-p", "dummy", NULL };
  int status;

  expected_plugins = "dummy";
  g_timeout_add_seconds (5, on_idle_quit, app);

  status = g_application_run (G_APPLICATION (app), G_N_ELEMENTS (argv), argv);
  g_assert_cmpint (status, ==, 0);

  g_assert_finalize_object (app);
}


static gboolean
on_kill_application (gpointer user_data)
{
  gint pid = GPOINTER_TO_INT (user_data);

  kill (pid, SIGTERM);

  return G_SOURCE_REMOVE;
}

static void
test_application_shutdown_sigterm (void)
{
  CallsApplication *app = calls_application_new ();
  int status;
  int pid = getpid ();

  g_idle_add (on_kill_application, GINT_TO_POINTER (pid));

  status = g_application_run (G_APPLICATION (app), 0, NULL);
  g_assert_cmpint (status, ==, 0);

  g_assert_finalize_object (app);
}


int
main (int   argc,
      char *argv[])
{
  g_autofree char *rec_dir = NULL;
  const char *tmp = g_get_tmp_dir ();
  int status;

  rec_dir = g_build_filename (tmp, "calls-XXXXXX", NULL);
  g_assert_nonnull (g_mkdtemp (rec_dir));

  g_print ("Setting 'CALLS_RECORD_DIR' to '%s'\n", rec_dir);
  g_setenv ("CALLS_RECORD_DIR", rec_dir, TRUE);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/application/shutdown_no_daemon", (GTestFunc) test_application_shutdown_no_daemon);
  g_test_add_func ("/Calls/application/shutdown_no_daemon_dummy", (GTestFunc) test_application_shutdown_no_daemon_dummy);
  g_test_add_func ("/Calls/application/shutdown_delayed", (GTestFunc) test_application_shutdown_delayed);
  g_test_add_func ("/Calls/application/shutdown_sigterm", (GTestFunc) test_application_shutdown_sigterm);
  /* Last test so we don't need to bother if --gpplication-service keeps us alive a bit longer */
  g_test_add_func ("/Calls/application/shutdown_daemon", (GTestFunc) test_application_shutdown_daemon);

  status = g_test_run ();

  /* Check that sqlite db is there but no stale write ahead logs */
  calls_assert_in_dir ("records.db", rec_dir);
  calls_assert_not_in_dir ("records.db-wal", rec_dir);
  calls_assert_not_in_dir ("records.db-shm", rec_dir);

  g_rmdir (rec_dir);

  return status;
}
