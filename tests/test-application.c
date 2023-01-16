/*
 * Copyright (C) 2023 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-application.h"

#include <glib/gstdio.h>
#include <gtk/gtk.h>

static gboolean
on_idle_quit (gpointer user_data)
{
  GApplication *app = user_data;

  g_application_quit (app);

  return G_SOURCE_REMOVE;
}


static void
test_application_shutdown_daemon (void)
{
  CallsApplication *app = calls_application_new ();
  char *argv[] = { "test", "--daemon", NULL };
  int status;

  g_idle_add (on_idle_quit, app);

  status = g_application_run (G_APPLICATION (app), 3, argv);
  g_assert_cmpint (status, ==, 0);

  g_assert_finalize_object (app);
}


static void
test_application_shutdown_no_daemon (void)
{
  CallsApplication *app = calls_application_new ();
  int status;

  g_idle_add (on_idle_quit, app);

  status = g_application_run (G_APPLICATION (app), 0, NULL);
  g_assert_cmpint (status, ==, 0);

  g_assert_finalize_object (app);
}


static void
test_application_shutdown_delayed (void)
{
  CallsApplication *app = calls_application_new ();
  int status;

  g_timeout_add_seconds (5, on_idle_quit, app);

  status = g_application_run (G_APPLICATION (app), 0, NULL);
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


static gboolean
dir_contains (const char *dir,
              const char *file,
              gboolean    only_file)
{
  g_autoptr (GDir) tmp_dir = g_dir_open (dir, 0, NULL);
  const char *tmp_file;
  gboolean found = FALSE;
  uint n = 0;

  while ((tmp_file = g_dir_read_name (tmp_dir))) {
    if (g_strcmp0 (tmp_file, file) == 0)
      found = TRUE;
    n++;
  }

  if (only_file)
    return found && n == 1;

  return found;
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

  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/application/shutdown_daemon", (GTestFunc) test_application_shutdown_daemon);
  g_test_add_func ("/Calls/application/shutdown_no_daemon", (GTestFunc) test_application_shutdown_no_daemon);
  g_test_add_func ("/Calls/application/shutdown_delayed", (GTestFunc) test_application_shutdown_delayed);
  g_test_add_func ("/Calls/application/shutdown_sigterm", (GTestFunc) test_application_shutdown_sigterm);

  status = g_test_run ();

  /* assert there is no dangling write-ahead-log */
  g_assert_true (dir_contains (rec_dir, "records.db", TRUE));

  g_rmdir (rec_dir);

  return status;
}
