/*
 * Copyright (C) 2018 Purism SPC
 *
 * This file is part of Calls.
 *
 * Calls is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Calls is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Calls.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Bob Ham <bob.ham@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include <gtk/gtk.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include "calls-encryption-indicator.h"
#include "calls-main-window.h"
#include "calls-mm-provider.h"

static void
show_window (GtkApplication *app)
{
  GError *error = NULL;
  GDBusConnection *connection;
  CallsProvider *provider;
  CallsMainWindow *main_window;

  CALLS_TYPE_ENCRYPTION_INDICATOR;
  HDY_TYPE_DIALER;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!connection)
    {
      g_error ("Error creating D-Bus connection: %s",
               error->message);
      return;
    }

  provider = CALLS_PROVIDER (calls_mm_provider_new (connection));
  g_assert (provider != NULL);

  main_window = calls_main_window_new (app, provider);

  gtk_window_set_title (GTK_WINDOW (main_window), "Calls");

  gtk_widget_show_all (GTK_WIDGET (main_window));
}

int
main (int    argc,
      char **argv)
{
  GtkApplication *app;
  int status;

  app = gtk_application_new ("sm.puri.Calls", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (show_window), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}
