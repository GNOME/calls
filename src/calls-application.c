/* calls-application.c
 *
 * Copyright (C) 2018 Purism SPC
 * Copyright (C) 2018 Mohammed Sadiq <sadiq@sadiqpk.org>
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
 * Authors:
 *      Bob Ham <bob.ham@puri.sm>
 *      Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include "config.h"
#include "calls-new-call-header-bar.h"
#include "calls-history-header-bar.h"
#include "calls-history-box.h"
#include "calls-new-call-box.h"
#include "calls-encryption-indicator.h"
#include "calls-mm-provider.h"
#include "calls-call-window.h"
#include "calls-main-window.h"
#include "calls-application.h"

/**
 * SECTION: calls-application
 * @title: CallsApplication
 * @short_description: Base Application class
 * @include: "calls-application.h"
 */

struct _CallsApplication
{
  GtkApplication parent_instance;

  GDBusConnection *connection;
  CallsProvider *provider;
};

G_DEFINE_TYPE (CallsApplication, calls_application, GTK_TYPE_APPLICATION)


static void
finalize (GObject *object)
{
  CallsApplication *self = (CallsApplication *)object;

  g_clear_object (&self->provider);
  g_clear_object (&self->connection);

  G_OBJECT_CLASS (calls_application_parent_class)->finalize (object);
}

static void
startup (GApplication *application)
{
  CallsApplication *self = (CallsApplication *)application;
  g_autoptr(GError) error = NULL;

  G_APPLICATION_CLASS (calls_application_parent_class)->startup (application);

  self->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

  if (!self->connection)
    {
      g_error ("Error creating D-Bus connection: %s", error->message);
    }

  self->provider = CALLS_PROVIDER (calls_mm_provider_new (self->connection));
  g_assert (self->provider != NULL);

  g_set_prgname (APP_ID);
  g_set_application_name (_("Calls"));
}

static void
activate (GApplication *application)
{
  CallsApplication *self = (CallsApplication *)application;
  GtkApplication *app = (GtkApplication *)application;
  GtkWindow *window;

  g_assert (GTK_IS_APPLICATION (app));

  window = gtk_application_get_active_window (app);

  if (window == NULL)
    {
      /*
       * We don't track the memory created.  Ideally, we might have to.
       * But we assume that the application is closed by closing the
       * window.  In that case, GTK+ frees the resources right.
       */
      window = GTK_WINDOW (calls_main_window_new (app, self->provider));
      calls_call_window_new (app, self->provider);
    }

  gtk_window_present (window);
}

static void
calls_application_class_init (CallsApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = finalize;

  application_class->startup = startup;
  application_class->activate = activate;

  g_type_ensure (CALLS_TYPE_ENCRYPTION_INDICATOR);
  g_type_ensure (CALLS_TYPE_HISTORY_BOX);
  g_type_ensure (CALLS_TYPE_HISTORY_HEADER_BAR);
  g_type_ensure (CALLS_TYPE_NEW_CALL_BOX);
  g_type_ensure (CALLS_TYPE_NEW_CALL_HEADER_BAR);
  g_type_ensure (HDY_TYPE_DIALER);
}

static void
calls_application_init (CallsApplication *self)
{
}

CallsApplication *
calls_application_new (void)
{
  return g_object_new (CALLS_TYPE_APPLICATION,
                       "application-id", APP_ID,
                       "flags", G_APPLICATION_FLAGS_NONE,
                       NULL);
}
