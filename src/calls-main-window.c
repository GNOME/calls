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
  GtkButton *back;
  GtkStack *call_stack;
  GtkScrolledWindow *call_scroll;
  GtkFlowBox *call_selector;
  GtkBox *dial_box;
  GtkExpander *new_call;
  GtkBox *dial_controls;
  GtkComboBox *origin;
  GtkSearchEntry *search;
  HdyDialer *dial_pad;

  GtkListStore *origin_store;
  GtkListStore *history_store;
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


static GtkWidget *
call_holders_create_widget_cb (CallsCallHolder *holder,
                               CallsMainWindow *self)
{
  return GTK_WIDGET (calls_call_holder_get_selector_item (holder));
}

static void
search_append_symbol (CallsMainWindow *self, gchar symbol)
{
  GtkEntryBuffer *buf = gtk_entry_get_buffer (GTK_ENTRY (self->search));
  guint len = gtk_entry_buffer_get_length (buf);

  gtk_entry_buffer_insert_text (buf, len, &symbol, 1);
}

static void
dial_pad_symbol_clicked_cb (CallsMainWindow *self, gchar symbol, HdyDialer *dialer)
{
  if (self->focus && !gtk_expander_get_expanded (self->new_call))
    {
      CallsCallData *data = calls_call_holder_get_data (self->focus);
      CallsCall *call = calls_call_data_get_call (data);

      calls_call_tone_start (call, symbol);
    }
  else
    {
      search_append_symbol (self, symbol);
    }
}


static void
dial_pad_deleted_cb (CallsMainWindow *self, HdyDialer *dialer)
{
  GtkEntryBuffer *buf = gtk_entry_get_buffer (GTK_ENTRY (self->search));
  guint len = gtk_entry_buffer_get_length (buf);

  gtk_entry_buffer_delete_text (buf, len - 1, 1);
}


static void
dial_pad_submitted_cb (CallsMainWindow *self, const gchar *unused, HdyDialer *dialer)
{
  GtkTreeIter iter;
  gboolean ok;
  CallsOrigin *origin;
  const gchar *number;

  g_return_if_fail (CALLS_IS_MAIN_WINDOW (self));

  if (gtk_widget_get_visible (GTK_WIDGET (self->new_call))
      && !gtk_expander_get_expanded (self->new_call))
    {
      return;
    }

  ok = gtk_combo_box_get_active_iter (self->origin, &iter);
  g_return_if_fail (ok);

  gtk_tree_model_get (GTK_TREE_MODEL (self->origin_store), &iter,
                      ORIGIN_STORE_COLUMN_ORIGIN, &origin,
                      -1);
  g_return_if_fail (CALLS_IS_ORIGIN (origin));

  number = gtk_entry_get_text (GTK_ENTRY (self->search));

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

  gtk_stack_set_visible_child
    (self->call_stack,
     GTK_WIDGET (calls_call_holder_get_display (holder)));
}


/* When we have an active call, we hide the dialpad action buttons and
 * put the dial_controls inside the new_call expander and use the
 * expanded state to determine whether key presses should be
 * considered dialing a new call or entering DTMF tones for the active
 * call.
 */
static void
show_new_call (CallsMainWindow *self)
{
  GtkWidget *dial_controls_widget = GTK_WIDGET (self->dial_controls);
  GObject *dial_controls_object = G_OBJECT (dial_controls_widget);

  hdy_dialer_set_show_action_buttons (self->dial_pad, FALSE);

  g_object_ref (dial_controls_object);
  gtk_container_remove (GTK_CONTAINER (self->dial_box), dial_controls_widget);

  gtk_container_add (GTK_CONTAINER (self->new_call), dial_controls_widget);
  g_object_unref (dial_controls_object);

  gtk_expander_set_expanded (self->new_call, FALSE);
  gtk_widget_show (GTK_WIDGET (self->new_call));

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}


static void
hide_new_call (CallsMainWindow *self)
{
  GtkWidget *dial_controls_widget = GTK_WIDGET (self->dial_controls);
  GObject *dial_controls_object = G_OBJECT (dial_controls_widget);

  gtk_widget_hide (GTK_WIDGET (self->new_call));

  g_object_ref (dial_controls_object);
  gtk_container_remove (GTK_CONTAINER (self->new_call), dial_controls_widget);

  gtk_box_pack_start (self->dial_box, dial_controls_widget, FALSE, TRUE, 0);
  g_object_unref (dial_controls_object);

  gtk_box_reorder_child (self->dial_box, dial_controls_widget, 0);

  hdy_dialer_set_show_action_buttons (self->dial_pad, TRUE);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}


static void
new_call_expanded_notify_cb (GtkExpander     *new_call,
                             GParamSpec      *param_spec,
                             CallsMainWindow *self)
{
  hdy_dialer_set_show_action_buttons (self->dial_pad,
                                      gtk_expander_get_expanded (new_call));
}


static void
back_clicked_cb (GtkButton       *back,
                 CallsMainWindow *self)
{
  gtk_stack_set_visible_child (self->call_stack, GTK_WIDGET (self->call_scroll));
  gtk_stack_set_visible_child (self->main_stack, GTK_WIDGET (self->call_stack));
}


static void
call_selector_child_activated_cb (GtkFlowBox      *box,
                                  GtkFlowBoxChild *child,
                                  CallsMainWindow *self)
{
  GtkWidget *widget = gtk_bin_get_child (GTK_BIN (child));
  CallsCallSelectorItem *item = CALLS_CALL_SELECTOR_ITEM (widget);
  CallsCallHolder *holder = calls_call_selector_item_get_holder (item);

  set_focus (self, holder);
}


/** Possibly show various call widgets */
static void
show_calls (CallsMainWindow *self, guint old_call_count)
{
  if (old_call_count == 0)
    {
      gtk_stack_add_titled (self->main_stack,
                            GTK_WIDGET (self->call_stack),
                            "call", "Call");
      show_new_call (self);
    }

  if (old_call_count > 0)
    {
      gtk_widget_show (GTK_WIDGET (self->back));
    }
}


static void
hide_calls (CallsMainWindow *self, guint call_count)
{
  if (call_count == 0)
    {
      hide_new_call (self);
      gtk_container_remove (GTK_CONTAINER (self->main_stack),
                            GTK_WIDGET (self->call_stack));
    }

  if (call_count <= 1)
    {
      gtk_widget_hide (GTK_WIDGET (self->back));
    }
}


static void
add_call (CallsMainWindow *self, CallsCall *call)
{
  CallsCallHolder *holder;
  CallsCallDisplay *display;

  g_signal_connect_swapped (call, "message",
                            G_CALLBACK (show_message), self);

  show_calls (self, g_list_model_get_n_items (G_LIST_MODEL (self->call_holders)));

  holder = calls_call_holder_new (call);

  display = calls_call_holder_get_display (holder);
  gtk_stack_add_named (self->call_stack, GTK_WIDGET (display),
                       calls_call_get_number (call));
  gtk_stack_set_visible_child (self->main_stack, GTK_WIDGET (self->call_stack));

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
  gtk_container_remove (GTK_CONTAINER (self->call_stack),
                        GTK_WIDGET (calls_call_holder_get_display (holder)));

  if (self->focus == holder)
    {
      set_focus (self, NULL);
    }

  hide_calls (self, n_items - 1);
}

static void
remove_call (CallsMainWindow *self, CallsCall *call, const gchar *reason)
{
  guint n_items, position;
  CallsCallHolder *holder;
  gboolean found;

  g_return_if_fail (CALLS_IS_MAIN_WINDOW (self));
  g_return_if_fail (CALLS_IS_CALL (call));

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
  const gint n_origins = gtk_tree_model_iter_n_children
    (GTK_TREE_MODEL (self->origin_store), NULL);
  GtkTreeIter iter;

  if (n_origins == 1)
    {
      /* We have more than one origin now so show the origin combo box */
      gtk_widget_show (GTK_WIDGET (self->origin));
    }

  gtk_list_store_append (self->origin_store, &iter);
  gtk_list_store_set (self->origin_store, &iter,
                      ORIGIN_STORE_COLUMN_NAME, calls_origin_get_name(origin),
                      ORIGIN_STORE_COLUMN_ORIGIN, G_OBJECT (origin),
                      -1);

  if (gtk_combo_box_get_active (self->origin) == -1)
    {
      /* We always want an item active */
      gtk_combo_box_set_active (self->origin, 0);
    }


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
update_origin (CallsMainWindow *self)
{
  if (gtk_tree_model_iter_n_children
      (GTK_TREE_MODEL (self->origin_store), NULL) < 2)
    {
      /* User has only one choice so hide the origin combo box */
      gtk_widget_hide (GTK_WIDGET (self->origin));
    }
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

  update_origin (self);
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

  update_origin (self);
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

  gtk_container_remove (GTK_CONTAINER (self->main_stack),
                        GTK_WIDGET (self->call_stack));

  gtk_flow_box_bind_model (self->call_selector,
                           G_LIST_MODEL (self->call_holders),
                           (GtkFlowBoxCreateWidgetFunc) call_holders_create_widget_cb,
                           NULL, NULL);

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

  CALLS_DISPOSE_OBJECT (self->call_holders);
  CALLS_DISPOSE_OBJECT (self->provider);

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


  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/main-window.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, info);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, info_label);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, back);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, call_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, call_scroll);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, call_selector);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, dial_box);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, new_call);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, dial_controls);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, origin);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, search);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, dial_pad);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, origin_store);
  gtk_widget_class_bind_template_child (widget_class, CallsMainWindow, history_store);
  gtk_widget_class_bind_template_callback (widget_class, info_response_cb);
  gtk_widget_class_bind_template_callback (widget_class, new_call_expanded_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, call_selector_child_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, back_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, dial_pad_submitted_cb);
  gtk_widget_class_bind_template_callback (widget_class, dial_pad_deleted_cb);
  gtk_widget_class_bind_template_callback (widget_class, dial_pad_symbol_clicked_cb);
}
