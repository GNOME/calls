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

#include "calls-main-window.h"
#include "calls-origin.h"
#include "calls-call-holder.h"
#include "calls-call-selector-item.h"
#include "calls-new-call-box.h"
#include "config.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>


struct _CallsMainWindow
{
  GtkApplicationWindow parent_instance;

  CallsProvider *provider;
  GListStore *call_holders;
  CallsCallHolder *focus;

  GtkInfoBar *info;
  GtkLabel *info_label;

  GtkStack *main_stack;

  GtkListStore *origin_store;
};

enum {
  ORIGIN_STORE_COLUMN_NAME,
  ORIGIN_STORE_COLUMN_ORIGIN
};

G_DEFINE_TYPE (CallsMainWindow, calls_main_window, GTK_TYPE_APPLICATION_WINDOW);

enum {
  PROP_0,
  PROP_PROVIDER,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


enum {
  SIGNAL_CALL_ADDED,
  SIGNAL_CALL_REMOVED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals [SIGNAL_LAST_SIGNAL];


static void
about_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  CallsMainWindow *self = user_data;

  const gchar *version = NULL;

  static const gchar *authors[] = {
    "Adrien Plazas <kekun.plazas@laposte.net>",
    "Bob Ham <rah@settrans.net>",
    "Guido Günther <agx@sigxcpu.org>",
    NULL
  };

  static const gchar *artists[] = {
    "Tobias Bernard <tbernard@gnome.org>",
    NULL
  };

  static const gchar *documenters[] = {
    "Heather Ellsworth <heather.ellsworth@puri.sm>",
    NULL
  };

  version = g_str_equal (VCS_TAG, "") ? PACKAGE_VERSION:
                                        PACKAGE_VERSION "-" VCS_TAG;

  gtk_show_about_dialog (GTK_WINDOW (self),
                         "artists", artists,
                         "authors", authors,
                         "copyright", "Copyright © 2018 Purism",
                         "documenters", documenters,
                         "license-type", GTK_LICENSE_GPL_3_0,
                         "logo-icon-name", APP_ID,
                         "program-name", _("Calls"),
                         "translator-credits", _("translator-credits"),
                         "version", version,
                         "website", PACKAGE_URL,
                         NULL);
}


static const GActionEntry window_entries [] =
{
  { "about", about_action },
};


CallsMainWindow *
calls_main_window_new (GtkApplication *application, CallsProvider *provider)
{
  return g_object_new (CALLS_TYPE_MAIN_WINDOW,
                       "application", application,
                       "provider", provider,
                       NULL);
}


static void
show_message (CallsMainWindow *self, const gchar *text, GtkMessageType type)
{
  gtk_info_bar_set_message_type (self->info, type);
  gtk_label_set_text (self->info_label, text);
  gtk_widget_show (GTK_WIDGET (self->info));
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}


static void
info_response_cb (GtkInfoBar      *infobar,
                  gint             response_id,
                  CallsMainWindow *self)
{
  gtk_widget_hide (GTK_WIDGET (self->info));
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}


static void
new_call_submitted_cb (CallsMainWindow *self,
                       CallsOrigin     *origin,
                       const gchar     *number,
                       CallsNewCallBox *new_call_box)
{
  g_return_if_fail (CALLS_IS_MAIN_WINDOW (self));

  calls_origin_dial (origin, number);
}


typedef gboolean (*FindCallHolderFunc) (CallsCallHolder *holder,
                                        gpointer user_data);


static gboolean
find_call_holder_by_call (CallsCallHolder *holder,
                          gpointer user_data)
{
  CallsCallData *data = calls_call_holder_get_data (holder);

  return calls_call_data_get_call (data) == user_data;
}


/** Search through the list of call holders, returning the total
    number of items in the list, the position of the holder within the
    list and a pointer to the holder itself. */
static gboolean
find_call_holder (CallsMainWindow *self,
                  guint *n_itemsp,
                  guint *positionp,
                  CallsCallHolder **holderp,
                  FindCallHolderFunc predicate,
                  gpointer user_data)
{
  GListModel * const model = G_LIST_MODEL (self->call_holders);
  const guint n_items = g_list_model_get_n_items (model);
  guint position = 0;
  CallsCallHolder *holder;

  for (position = 0; position < n_items; ++position)
    {
      holder = CALLS_CALL_HOLDER (g_list_model_get_item (model, position));

      if (predicate (holder, user_data))
        {
#define out(var)                                \
          if (var##p)                           \
            {                                   \
              *var##p = var ;                   \
            }

          out (n_items);
          out (position);
          out (holder);

#undef out
          
          return TRUE;
        }
    }

  return FALSE;
}


static void
set_focus (CallsMainWindow *self, CallsCallHolder *holder)
{
  if (!holder)
    {
      holder = g_list_model_get_item (G_LIST_MODEL (self->call_holders), 0);

      if (!holder)
        {
          /* No calls */
          self->focus = NULL;
          return;
        }
    }

  self->focus = holder;
}


static void
add_call (CallsMainWindow *self, CallsCall *call)
{
  CallsCallHolder *holder;

  g_signal_emit (self, signals[SIGNAL_CALL_ADDED], 0, call);

  g_signal_connect_swapped (call, "message",
                            G_CALLBACK (show_message), self);

  holder = calls_call_holder_new (call);

  g_list_store_append (self->call_holders, holder);

  set_focus (self, holder);

}


static void
remove_call_holder (CallsMainWindow *self,
                    guint n_items,
                    guint position,
                    CallsCallHolder *holder)
{
  g_list_store_remove (self->call_holders, position);

  if (self->focus == holder)
    {
      set_focus (self, NULL);
    }
}

static void
remove_call (CallsMainWindow *self, CallsCall *call, const gchar *reason)
{
  guint n_items, position;
  CallsCallHolder *holder;
  gboolean found;

  g_return_if_fail (CALLS_IS_MAIN_WINDOW (self));
  g_return_if_fail (CALLS_IS_CALL (call));

  g_signal_emit (self, signals[SIGNAL_CALL_REMOVED], 0, call, reason);

  found = find_call_holder (self, &n_items, &position, &holder,
                            find_call_holder_by_call, call);
  g_return_if_fail (found);

  remove_call_holder (self, n_items, position, holder);

  if (!reason)
    {
      reason = "Call ended for unknown reason";
    }
  show_message(self, reason, GTK_MESSAGE_INFO);
}


static void
remove_calls (CallsMainWindow *self)
{
  GListModel * model = G_LIST_MODEL (self->call_holders);
  guint n_items = g_list_model_get_n_items (model);
  gpointer *item;
  CallsCallHolder *holder;

  while ( (item = g_list_model_get_item (model, 0)) )
    {
      holder = CALLS_CALL_HOLDER (item);
      remove_call_holder (self, n_items--, 0, holder);
    }
}


static void
add_origin_calls (CallsMainWindow *self, CallsOrigin *origin)
{
  GList *calls, *node;

  calls = calls_origin_get_calls (origin);

  for (node = calls; node; node = node->next)
    {
      add_call (self, CALLS_CALL (node->data));
    }

  g_list_free (calls);
}


static void
add_origin (CallsMainWindow *self, CallsOrigin *origin)
{
  GtkTreeIter iter;

  gtk_list_store_append (self->origin_store, &iter);
  gtk_list_store_set (self->origin_store, &iter,
                      ORIGIN_STORE_COLUMN_NAME, calls_origin_get_name(origin),
                      ORIGIN_STORE_COLUMN_ORIGIN, G_OBJECT (origin),
                      -1);


  g_signal_connect_swapped (origin, "message",
                            G_CALLBACK (show_message), self);
  g_signal_connect_swapped (origin, "call-added",
                            G_CALLBACK (add_call), self);
  g_signal_connect_swapped (origin, "call-removed",
                            G_CALLBACK (remove_call), self);

  add_origin_calls(self, origin);
}


static void
dump_list_store (GtkListStore *store)
{
  GtkTreeIter iter;
  GtkTreeModel *model = GTK_TREE_MODEL (store);
  gboolean ok;

  ok = gtk_tree_model_get_iter_first (model, &iter);
  if (!ok)
    {
      return;
    }

  g_debug ("List store:");
  do
    {
      gchararray name;
      gtk_tree_model_get (model, &iter,
                          ORIGIN_STORE_COLUMN_NAME, &name,
                          -1);
      g_debug (" name: `%s'", name);
    }
  while (gtk_tree_model_iter_next (model, &iter));
}


static void
remove_origin (CallsMainWindow *self, CallsOrigin *origin)
{
  GtkTreeIter iter;
  gboolean ok;

  ok = calls_list_store_find (self->origin_store, origin,
                              ORIGIN_STORE_COLUMN_ORIGIN, &iter);
  g_return_if_fail (ok);

  gtk_list_store_remove (self->origin_store, &iter);
}


static void
remove_origins (CallsMainWindow *self)
{
  GtkTreeModel *model = GTK_TREE_MODEL (self->origin_store);
  GtkTreeIter iter;

  while (gtk_tree_model_get_iter_first (model, &iter))
    {
      gtk_list_store_remove (self->origin_store, &iter);
    }
}


static void
add_provider_origins (CallsMainWindow *self, CallsProvider *provider)
{
  GList *origins, *node;

  origins = calls_provider_get_origins (provider);

  for (node = origins; node; node = node->next)
    {
      add_origin (self, CALLS_ORIGIN (node->data));
    }

  g_list_free (origins);

  dump_list_store (self->origin_store);
}


static void
set_provider (CallsMainWindow *self, CallsProvider *provider)
{
  g_signal_connect_swapped (provider, "message",
                            G_CALLBACK (show_message), self);
  g_signal_connect_swapped (provider, "origin-added",
                            G_CALLBACK (add_origin), self);
  g_signal_connect_swapped (provider, "origin-removed",
                            G_CALLBACK (remove_origin), self);

  self->provider = provider;
  g_object_ref (G_OBJECT (provider));

  add_provider_origins (self, provider);
}

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);
  GObject *val_obj;

  switch (property_id) {
  case PROP_PROVIDER:
    val_obj = g_value_get_object (value);
    if (val_obj == NULL)
      {
        g_warning("Null provider");
        self->provider = NULL;
      }
    else
      {
        g_return_if_fail (CALLS_IS_PROVIDER (val_obj));
        set_provider (self, CALLS_PROVIDER (val_obj));
      }
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
constructed (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (GTK_TYPE_APPLICATION_WINDOW);
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);
  GSimpleActionGroup *simple_action_group;

  /* Add actions */
  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   window_entries,
                                   G_N_ELEMENTS (window_entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "win",
                                  G_ACTION_GROUP (simple_action_group));
  g_object_unref (simple_action_group);

  parent_class->constructed (object);
}


static void
calls_main_window_init (CallsMainWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->call_holders = g_list_store_new (CALLS_TYPE_CALL_HOLDER);
}


static void
dispose (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (GTK_TYPE_APPLICATION_WINDOW);
  CallsMainWindow *self = CALLS_MAIN_WINDOW (object);

  if (self->call_holders)
    {
      remove_calls (self);
    }

  if (self->origin_store)
  {
    remove_origins (self);
  }

  g_clear_object (&self->call_holders);
  g_clear_object (&self->provider);

  parent_class->dispose (object);
}


static void
calls_main_window_class_init (CallsMainWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;

  props[PROP_PROVIDER] =
    g_param_spec_object ("provider",
                         _("Provider"),
                         _("An object implementing low-level call-making functionality"),
                         CALLS_TYPE_PROVIDER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  signals[SIGNAL_CALL_ADDED] =
    g_signal_new ("call-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  CALLS_TYPE_CALL);

  signals[SIGNAL_CALL_REMOVED] =
    g_signal_new ("call-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  CALLS_TYPE_CALL,
                  G_TYPE_STRING);


  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/main-window.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, info);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, info_label);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, origin_store);
  gtk_widget_class_bind_template_callback (widget_class, info_response_cb);
  gtk_widget_class_bind_template_callback (widget_class, new_call_submitted_cb);
}
