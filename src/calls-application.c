/* calls-application.c
 *
 * Copyright (C) 2018, 2019 Purism SPC
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

#define G_LOG_DOMAIN "CallsApplication"

#include "config.h"

#include "calls-application.h"
#include "calls-call-window.h"
#include "calls-dbus-manager.h"
#include "calls-history-box.h"
#include "calls-log.h"
#include "calls-main-window.h"
#include "calls-manager.h"
#include "calls-message-source.h"
#include "calls-new-call-box.h"
#include "calls-notifier.h"
#include "calls-record-store.h"
#include "calls-ringer.h"
#include "version.h"

#include <call-ui.h>
#include <glib/gi18n.h>
#include <handy.h>
#include <libcallaudio.h>

/**
 * SECTION: calls-application
 * @title: CallsApplication
 * @short_description: Base Application class
 * @include: "calls-application.h"
 */

#define DEFAULT_PROVIDER_PLUGIN "mm"

struct _CallsApplication {
  GtkApplication    parent_instance;

  gboolean          daemon;
  CallsRinger      *ringer;
  CallsNotifier    *notifier;
  CallsRecordStore *record_store;
  CallsMainWindow  *main_window;
  CallsCallWindow  *call_window;
  CallsDBusManager *dbus_manager;

  char             *uri;
};

G_DEFINE_TYPE (CallsApplication, calls_application, GTK_TYPE_APPLICATION);


static void start_proper (CallsApplication *self);


static gboolean
cmd_verbose_cb (const char *option_name,
                const char *value,
                gpointer    data,
                GError    **error)
{
  calls_log_increase_verbosity ();

  return TRUE;
}

static gboolean
calls_application_dbus_register (GApplication    *application,
                                 GDBusConnection *connection,
                                 const gchar     *object_path,
                                 GError         **error)
{
  CallsApplication *self = CALLS_APPLICATION (application);

  G_APPLICATION_CLASS (calls_application_parent_class)->dbus_register (application,
                                                                       connection,
                                                                       object_path,
                                                                       error);

  self->dbus_manager = calls_dbus_manager_new ();
  return calls_dbus_manager_register (self->dbus_manager, connection, object_path, error);
}


static void
calls_application_dbus_unregister (GApplication    *application,
                                   GDBusConnection *connection,
                                   const gchar     *object_path)
{
  CallsApplication *self = CALLS_APPLICATION (application);

  g_clear_object (&self->dbus_manager);

  G_APPLICATION_CLASS (calls_application_parent_class)->dbus_unregister (application,
                                                                         connection,
                                                                         object_path);
}


static void
set_provider_names_action (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  CallsManager *manager;
  g_autofree const char **names = NULL;
  g_autofree const char **loaded = NULL;
  gsize length;
  guint length_loaded;

  names = g_variant_get_strv (parameter, &length);
  g_return_if_fail (names && *names);

  manager = calls_manager_get_default ();
  loaded = calls_manager_get_provider_names (manager, &length_loaded);

  /* remove unwanted providers */
  for (guint i = 0; i < length_loaded; i++) {
    g_autofree char *provider = g_strdup (loaded[i]);
    if (!g_strv_contains (names, provider))
      calls_manager_remove_provider (manager, provider);
  }

  for (guint i = 0; i < length; i++) {
    const char *name = names[i];

    if (calls_manager_has_provider (manager, name))
      continue;

    g_debug ("Loading provider `%s'", name);
    calls_manager_add_provider (manager, name);
  }
}


static void
set_default_providers_action (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  CallsManager *manager = calls_manager_get_default ();
  CallsSettings *settings = calls_manager_get_settings (manager);

  g_auto (GStrv) plugins = NULL;
  /**
   * Only add default providers when there are none added yet,
   * This makes sure we're not resetting explicitly set providers
   */
  if (calls_manager_has_any_provider (manager))
    return;

  plugins = calls_settings_get_autoload_plugins (settings);
  for (guint i = 0; plugins[i] != NULL; i++) {
    calls_manager_add_provider (manager, plugins[i]);
  }
}


static void
set_daemon_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  CallsApplication *self = CALLS_APPLICATION (user_data);
  gboolean daemon = g_variant_get_boolean (parameter);

  self->daemon = daemon;

  g_debug ("Application %smarked as daemon",
           daemon ? "" : "not ");
}


#define DIALLING    "0-9*#+ABCD"
#define SIGNALLING  ",TP!W@X"
#define VISUAL      "[:space:]\\-.()t/"
#define REJECT_RE   "[^" DIALLING SIGNALLING VISUAL "]"
#define VISUAL_RE   "[" VISUAL "]"

static gboolean
check_dial_number (const char *number)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GRegex) reject = g_regex_new (REJECT_RE, 0, 0, &error);
  gboolean matches;

  if (!reject) {
    g_warning ("Could not compile regex for"
               " dial number checking: %s",
               error->message);
    return FALSE;
  }

  matches = g_regex_match (reject, number, 0, NULL);

  return !matches;
}


static char *
extract_dial_string (const char *number)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GRegex) replace_visual = g_regex_new (VISUAL_RE, 0, 0, &error);
  char *dial_string;

  if (!replace_visual) {
    g_warning ("Could not compile regex for"
               " dial number extracting: %s",
               error->message);
    return NULL;
  }

  dial_string = g_regex_replace_literal
                  (replace_visual, number, -1, 0, "", 0, &error);

  if (!dial_string) {
    g_warning ("Error replacing visual separators"
               " in dial number: %s",
               error->message);
    return NULL;
  }

  return dial_string;
}


static void
call_number (CallsApplication *self,
             const char       *number)
{
  g_autofree char *dial_string = NULL;
  gboolean number_ok;

  number_ok = check_dial_number (number);
  if (!number_ok) {
    g_warning ("Dial number `%s' is not a valid dial string",
               number);
    return;
  }

  dial_string = extract_dial_string (number);
  if (!dial_string) {
    return;
  }

  g_debug ("Dialing dial string `%s' extracted from number `%s'",
           dial_string, number);

  start_proper (self);

  calls_main_window_dial (self->main_window,
                          dial_string);

}


static void
open_sip_uri (CallsApplication *self,
              const char       *uri)
{
  g_auto (GStrv) tokens = NULL;
  g_assert (uri);

  tokens = g_strsplit (uri, "///", 2);

  if (g_strv_length (tokens) == 2) {
    /* Remove "///" from "sip:///user@host" */
    g_autofree char *dial_string = g_strconcat (tokens[0], tokens[1], NULL);

    calls_main_window_dial (self->main_window, dial_string);
  } else {
    /* Dial the uri as it is */
    calls_main_window_dial (self->main_window, uri);
  }
}


static void
open_tel_uri (CallsApplication *self,
              const char       *uri)
{
  const char *number = NULL;
  g_autofree char* uri_str = g_uri_unescape_string (uri, NULL);

  g_debug ("Opening tel URI `%s'", uri);

  number = &uri_str[4]; // tel:NUMBER
  if (!number || !*number) {
    g_autofree char *msg =
      g_strdup_printf (_("Tried dialing invalid tel URI `%s'"), uri);

    calls_message_source_emit_message (CALLS_MESSAGE_SOURCE (calls_manager_get_default ()),
                                       msg,
                                       GTK_MESSAGE_WARNING);
    g_warning ("Ignoring invalid tel URI `%s'", uri);
    return;
  }

  call_number (self, number);
}


static void
dial_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  CallsApplication *self = CALLS_APPLICATION (user_data);
  const char *number;

  number = g_variant_get_string (parameter, NULL);
  g_return_if_fail (number != NULL);

  if (g_str_has_prefix (number, "sip:") ||
      g_str_has_prefix (number, "sips:"))
    open_sip_uri (self, number);
  else if (g_str_has_prefix (number, "tel:"))
    open_tel_uri (self, number);
  else
    call_number (self, number);
}


static void
copy_number (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  const char *number = g_variant_get_string (parameter, NULL);
  GtkClipboard *clipboard =
    gtk_clipboard_get_default (gdk_display_get_default ());

  gtk_clipboard_set_text (clipboard, number, -1);

  g_debug ("Copied `%s' to clipboard", number);
}


static void
show_accounts (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  CallsApplication *app = CALLS_APPLICATION (user_data);

  calls_main_window_show_accounts_overview (app->main_window);
}

static void
manager_state_changed_cb (GApplication *application)
{
  GAction* dial_action = g_action_map_lookup_action (G_ACTION_MAP (application), "dial");
  CallsManagerFlags state_flags = calls_manager_get_state_flags (calls_manager_get_default ());
  gboolean enabled = (state_flags & CALLS_MANAGER_FLAGS_HAS_CELLULAR_MODEM) ||
                     (state_flags & CALLS_MANAGER_FLAGS_HAS_VOIP_ACCOUNT);

  g_simple_action_set_enabled (G_SIMPLE_ACTION (dial_action), enabled);
}

static const GActionEntry actions[] =
{
  { "set-provider-names", set_provider_names_action, "as" },
  { "set-default-providers", set_default_providers_action, NULL },
  { "set-daemon", set_daemon_action, "b" },
  { "dial", dial_action, "s" },
  { "copy-number", copy_number, "s" },
  /* TODO About dialog { "about", show_about, NULL}, */
  { "accounts", show_accounts, NULL },
};


static int
calls_application_handle_local_options (GApplication *application,
                                        GVariantDict *options)
{
  guint verbosity = calls_log_get_verbosity ();

  if (g_variant_dict_contains (options, "version")) {
    g_print ("%s %s\n", APP_DATA_NAME, *VCS_TAG ? VCS_TAG : PACKAGE_VERSION);

    return 0;
  }

  /* Propagate verbosity changes to the main instance */
  if (verbosity > 0) {
    g_variant_dict_insert_value (options, "verbosity", g_variant_new_uint32 (verbosity));
    g_print ("Increasing verbosity to %u\n", verbosity);
  }


  return -1;
}


static void
startup (GApplication *application)
{
  g_autoptr (GtkCssProvider) provider = NULL;
  g_autoptr (GError) error = NULL;
#if HDY_CHECK_VERSION (1, 5, 0)
  HdyStyleManager *style_manager;
#endif

  G_APPLICATION_CLASS (calls_application_parent_class)->startup (application);

  hdy_init ();

#if HDY_CHECK_VERSION (1, 5, 0)
  style_manager = hdy_style_manager_get_default ();

  hdy_style_manager_set_color_scheme (style_manager, HDY_COLOR_SCHEME_PREFER_LIGHT);
#endif

  if (!call_audio_init (&error))
    g_warning ("Failed to init libcallaudio: %s", error->message);

  cui_init (TRUE);

  g_set_prgname (APP_ID);
  g_set_application_name (_("Calls"));

  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   application);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "notify::state",
                            G_CALLBACK (manager_state_changed_cb),
                            application);

  manager_state_changed_cb (application);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/Calls/style.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}


static int
calls_application_command_line (GApplication            *application,
                                GApplicationCommandLine *command_line)
{
  CallsApplication *self = CALLS_APPLICATION (application);
  GVariantDict *options;
  const char *arg;

  g_autoptr (GVariant) providers = NULL;
  g_auto (GStrv) arguments = NULL;
  gint argc;
  guint verbosity;

  options = g_application_command_line_get_options_dict (command_line);

  providers = g_variant_dict_lookup_value (options, "provider", G_VARIANT_TYPE_STRING_ARRAY);
  if (providers) {
    g_action_group_activate_action (G_ACTION_GROUP (application),
                                    "set-provider-names",
                                    providers);
  } else {
    g_action_group_activate_action (G_ACTION_GROUP (application),
                                    "set-default-providers",
                                    NULL);
  }

  g_action_group_activate_action (G_ACTION_GROUP (application),
                                  "set-daemon",
                                  g_variant_new_boolean (g_variant_dict_contains (options, "daemon")));

  if (g_variant_dict_lookup (options, "dial", "&s", &arg))
    g_action_group_activate_action (G_ACTION_GROUP (application),
                                    "dial", g_variant_new_string (arg));

  /* TODO make this a comma separated string of "CATEGORY:level" pairs */
  if (g_variant_dict_lookup (options, "verbosity", "u", &verbosity)) {
    gint delta = calls_log_set_verbosity (verbosity);
    guint level = calls_log_get_verbosity ();
    if (delta != 0)
      g_print ("%s verbosity by %d to %u\n",
               delta > 0 ? "Increased" : "Decreased",
               delta,
               level);
  }

  arguments = g_application_command_line_get_arguments (command_line, &argc);

  /* Keep only the first URI, if there are many */
  for (guint i = 0; i < argc; i++)
    if (g_str_has_prefix (arguments[i], "tel:") ||
        g_str_has_prefix (arguments[i], "sip:") ||
        g_str_has_prefix (arguments[i], "sips:")) {
      g_free (self->uri);
      self->uri = g_strdup (arguments[i]);
      break;
    }

  g_application_activate (application);

  return 0;
}

static void
app_shutdown (GApplication *application)
{
  cui_uninit ();

  G_APPLICATION_CLASS (calls_application_parent_class)->shutdown (application);
}


static void
notify_window_visible_cb (GtkWidget        *window,
                          GParamSpec       *pspec,
                          CallsApplication *application)
{
  CallsManager *manager = calls_manager_get_default ();

  g_return_if_fail (CALLS_IS_APPLICATION (application));
  g_return_if_fail (CALLS_IS_CALL_WINDOW (window));

  /* The UI is being closed, hang up active calls */
  if (!gtk_widget_is_visible (window))
    calls_manager_hang_up_all_calls (manager);
}


static void
start_proper (CallsApplication *self)
{
  GtkApplication *gtk_app;

  if (self->main_window) {
    return;
  }

  gtk_app = GTK_APPLICATION (self);

  self->ringer = calls_ringer_new ();
  g_assert (self->ringer != NULL);

  self->record_store = calls_record_store_new ();
  g_assert (self->record_store != NULL);

  self->notifier = calls_notifier_new ();
  g_assert (CALLS_IS_NOTIFIER (self->notifier));

  self->main_window = calls_main_window_new
                        (gtk_app,
                        G_LIST_MODEL (self->record_store));
  g_assert (self->main_window != NULL);

  self->call_window = calls_call_window_new (gtk_app);
  g_assert (self->call_window != NULL);

  g_signal_connect (self->call_window,
                    "notify::visible",
                    G_CALLBACK (notify_window_visible_cb),
                    self);
}


static void
activate (GApplication *application)
{
  CallsApplication *self = CALLS_APPLICATION (application);
  gboolean present;

  g_debug ("Activated");

  start_proper (self);
  present = !self->daemon;

  if (present || self->uri) {
    gtk_window_present (GTK_WINDOW (self->main_window));
  }

  if (self->uri) {
    if (g_str_has_prefix (self->uri, "tel:"))
      open_tel_uri (self, self->uri);

    else if (g_str_has_prefix (self->uri, "sip:") ||
             g_str_has_prefix (self->uri, "sips:"))
      open_sip_uri (self, self->uri);
  }

  g_clear_pointer (&self->uri, g_free);
}


static void
app_open (GApplication *application,
          GFile       **files,
          gint          n_files,
          const char   *hint)
{
  CallsApplication *self = CALLS_APPLICATION (application);

  g_assert (n_files > 0);

  if (n_files > 1)
    g_warning ("Calls can handle only one call a time. %u items provided", n_files);

  if (g_file_has_uri_scheme (files[0], "tel") ||
      g_file_has_uri_scheme (files[0], "sip") ||
      g_file_has_uri_scheme (files[0], "sips")) {

    g_free (self->uri);
    self->uri = g_file_get_uri (files[0]);
    g_debug ("Opening %s", self->uri);

    g_application_activate (application);
  } else {
    g_autofree char *msg = NULL;
    g_autofree char *uri = NULL;

    uri = g_file_get_parse_name (files[0]);
    g_warning ("Don't know how to"
               " open file `%s', ignoring",
               uri);

    msg = g_strdup_printf (_("Don't know how to open `%s'"), uri);

    calls_message_source_emit_message (CALLS_MESSAGE_SOURCE (calls_manager_get_default ()),
                                       msg,
                                       GTK_MESSAGE_WARNING);
  }
}


static void
finalize (GObject *object)
{
  CallsApplication *self = (CallsApplication *) object;

  g_clear_object (&self->call_window);
  g_clear_object (&self->main_window);
  g_clear_object (&self->record_store);
  g_clear_object (&self->ringer);
  g_clear_object (&self->notifier);
  g_free (self->uri);

  G_OBJECT_CLASS (calls_application_parent_class)->finalize (object);
}


static void
calls_application_class_init (CallsApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = finalize;

  application_class->handle_local_options = calls_application_handle_local_options;
  application_class->startup = startup;
  application_class->command_line = calls_application_command_line;
  application_class->shutdown = app_shutdown;
  application_class->activate = activate;
  application_class->open = app_open;
  application_class->dbus_register = calls_application_dbus_register;
  application_class->dbus_unregister = calls_application_dbus_unregister;

  g_type_ensure (CALLS_TYPE_HISTORY_BOX);
  g_type_ensure (CALLS_TYPE_NEW_CALL_BOX);
}


static void
calls_application_init (CallsApplication *self)
{
  const GOptionEntry options[] = {
    {
      "provider", 'p', G_OPTION_FLAG_NONE,
      G_OPTION_ARG_STRING_ARRAY, NULL,
      _("The name of the plugin to use as a call provider"),
      _("PLUGIN")
    },
    {
      "daemon", 'd', G_OPTION_FLAG_NONE,
      G_OPTION_ARG_NONE, NULL,
      _("Whether to present the main window on startup"),
      NULL
    },
    {
      "dial", 'l', G_OPTION_FLAG_NONE,
      G_OPTION_ARG_STRING, NULL,
      _("Dial a telephone number"),
      _("NUMBER")
    },
    {
      "verbose", 'v', G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_CALLBACK, cmd_verbose_cb,
      _("Enable verbose debug messages"),
      NULL
    },
    {
      "version", 0, G_OPTION_FLAG_NONE,
      G_OPTION_ARG_NONE, NULL,
      _("Print current version"),
      NULL
    },
    {
      NULL
    }
  };

  g_application_add_main_option_entries (G_APPLICATION (self), options);
}


CallsApplication *
calls_application_new (void)
{
  return g_object_new (CALLS_TYPE_APPLICATION,
                       "application-id", APP_ID,
                       "flags", G_APPLICATION_HANDLES_OPEN | G_APPLICATION_HANDLES_COMMAND_LINE,
                       "register-session", TRUE,
                       NULL);
}
