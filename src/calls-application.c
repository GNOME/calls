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

#include "config.h"
#include "calls-new-call-header-bar.h"
#include "calls-history-header-bar.h"
#include "calls-history-box.h"
#include "calls-new-call-box.h"
#include "calls-encryption-indicator.h"
#include "calls-ringer.h"
#include "calls-call-window.h"
#include "calls-main-window.h"
#include "calls-application.h"

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include <libpeas/peas.h>
#include <glib/gi18n.h>

/**
 * SECTION: calls-application
 * @title: CallsApplication
 * @short_description: Base Application class
 * @include: "calls-application.h"
 */

#define DEFAULT_PROVIDER_PLUGIN "mm"

struct _CallsApplication
{
  GtkApplication parent_instance;

  GString       *provider_name;
  CallsProvider *provider;
  CallsRinger   *ringer;
};

G_DEFINE_TYPE (CallsApplication, calls_application, GTK_TYPE_APPLICATION)


static gint
handle_local_options (GApplication *application,
                      GVariantDict *options)
{
  gboolean ok;
  g_autoptr(GError) error = NULL;
  const gchar *name;

  g_debug ("Registering application");
  ok = g_application_register (application, NULL, &error);
  if (!ok)
    {
      g_error ("Error registering application: %s",
               error->message);
    }

  ok = g_variant_dict_lookup (options, "provider", "&s", &name);
  if (ok)
    {
      g_action_group_activate_action (G_ACTION_GROUP (application),
                                      "set-provider-name",
                                      g_variant_new_string (name));
    }

  return -1; // Continue processing signal
}


static void
set_provider_name_action (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  CallsApplication *self = CALLS_APPLICATION (user_data);
  const gchar *name;

  name = g_variant_get_string (parameter, NULL);
  g_return_if_fail (name != NULL);

  if (self->provider)
    {
      g_warning ("Cannot set provider name to `%s'"
                 " because provider is already created",
                 name);
      return;
    }

  g_string_assign (self->provider_name, name);

  g_debug ("Provider name set to `%s'",
           self->provider_name->str);
}


static const GActionEntry actions[] =
{
  { "set-provider-name", set_provider_name_action, "s" },
};


static void
startup (GApplication *application)
{
  G_APPLICATION_CLASS (calls_application_parent_class)->startup (application);

  g_set_prgname (APP_ID);
  g_set_application_name (_("Calls"));

  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   application);
}


static void
load_provider_plugin (CallsApplication *self)
{
  const gchar * const name = self->provider_name->str;
  PeasEngine *plugins;
  PeasPluginInfo *info;
  PeasExtension *extension;

  g_assert (self->provider == NULL);

  // Add Calls search path and rescan
  plugins = peas_engine_get_default ();
  peas_engine_add_search_path (plugins, PLUGIN_LIBDIR, PLUGIN_LIBDIR);
  g_debug ("Scanning for plugins in `%s'", PLUGIN_LIBDIR);

  // Find the plugin
  info = peas_engine_get_plugin_info (plugins, name);
  if (!info)
    {
      g_critical ("Could not find plugin `%s'", name);
      return;
    }

  // Possibly load the plugin
  if (!peas_plugin_info_is_loaded (info))
    {
      g_autoptr(GError) error = NULL;

      peas_engine_load_plugin (plugins, info);

      if (!peas_plugin_info_is_available (info, &error))
        {
          if (error)
            {
              g_critical ("Error loading plugin `%s': %s",
                          name, error->message);
            }
          else
            {
              g_critical ("Could not load plugin `%s'", name);
            }

          return;
        }

      g_debug ("Loaded plugin `%s'", name);
    }

  // Check the plugin provides CallsProvider
  if (!peas_engine_provides_extension
      (plugins, info, CALLS_TYPE_PROVIDER))
    {
      g_critical ("Plugin `%s' does not have a provider extension",
                  name);
      return;
    }

  // Get the extension
  extension = peas_engine_create_extensionv
    (plugins, info, CALLS_TYPE_PROVIDER, 0, NULL);
  if (!extension)
    {
      g_critical ("Could not create provider from plugin `%s'",
                  name);
      return;
    }

  g_debug ("Created provider from plugin `%s'", name);
  self->provider = CALLS_PROVIDER (extension);
}


static void
activate (GApplication *application)
{
  GtkApplication *gtk_app;
  GtkWindow *window;

  g_assert (GTK_IS_APPLICATION (application));
  gtk_app = GTK_APPLICATION (application);

  window = gtk_application_get_active_window (gtk_app);

  if (window == NULL)
    {
      CallsApplication *self = CALLS_APPLICATION (application);

      // Later we will make provider loading/unloaded a dynamic
      // process but that will have far-reaching consequences and is
      // of no use immediately so for now, we just load one provider
      // at startup.  We can't put this in the actual startup() method
      // though, because we need to be able to set the provider name
      // from the command line and we use actions to do that, which
      // depend on the application already being started up.
      if (!self->provider)
        {
          load_provider_plugin (self);
          if (!self->provider)
            {
              g_application_quit (application);
              return;
            }

          self->ringer = calls_ringer_new (self->provider);
          g_assert (self->ringer != NULL);
        }

      /*
       * We don't track the memory created.  Ideally, we might have to.
       * But we assume that the application is closed by closing the
       * window.  In that case, GTK+ frees the resources right.
       */
      window = GTK_WINDOW (calls_main_window_new (gtk_app, self->provider));
      calls_call_window_new (gtk_app, self->provider);
    }

  gtk_window_present (window);
}


static void
constructed (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (GTK_TYPE_APPLICATION);
  CallsApplication *self = CALLS_APPLICATION (object);
  GSimpleActionGroup *action_group;

  action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (action_group),
                                   actions, G_N_ELEMENTS (actions), self);
  g_object_unref (action_group);

  parent_class->constructed (object);
}


static void
dispose (GObject *object)
{
  CallsApplication *self = (CallsApplication *)object;

  g_clear_object (&self->ringer);
  g_clear_object (&self->provider);

  G_OBJECT_CLASS (calls_application_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsApplication *self = (CallsApplication *)object;

  g_string_free (self->provider_name, TRUE);

  G_OBJECT_CLASS (calls_application_parent_class)->finalize (object);
}


static void
calls_application_class_init (CallsApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  application_class->handle_local_options = handle_local_options;
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
  const GOptionEntry options[] = {
    {
      "provider", 'p', G_OPTION_FLAG_NONE,
      G_OPTION_ARG_STRING, NULL,
      _("The name of the plugin to use for the call Provider"),
      _("PLUGIN")
    },
    {
      NULL
    }
  };

  g_application_add_main_option_entries (G_APPLICATION (self), options);

  self->provider_name = g_string_new (DEFAULT_PROVIDER_PLUGIN);
}


CallsApplication *
calls_application_new (void)
{
  return g_object_new (CALLS_TYPE_APPLICATION,
                       "application-id", APP_ID,
                       "flags", G_APPLICATION_FLAGS_NONE,
                       NULL);
}
