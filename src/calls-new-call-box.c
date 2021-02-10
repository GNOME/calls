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
 * Author: Adrien Plazas <adrien.plazas@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "calls-new-call-box.h"
#include "calls-ussd.h"
#include "calls-main-window.h"
#include "calls-manager.h"

#include <glib/gi18n.h>
#include <handy.h>


struct _CallsNewCallBox
{
  GtkBox parent_instance;

  GtkListStore *origin_store;
  GtkComboBox *origin_box;
  GtkButton *backspace;
  HdyKeypad *keypad;
  GtkButton *dial;
  GtkGestureLongPress *long_press_back_gesture;

  GList *dial_queue;
};

G_DEFINE_TYPE (CallsNewCallBox, calls_new_call_box, GTK_TYPE_BOX);


enum {
  ORIGIN_STORE_COLUMN_NAME,
  ORIGIN_STORE_COLUMN_ORIGIN
};


static CallsOrigin *
get_origin (CallsNewCallBox *self)
{
  GtkTreeIter iter;
  gboolean ok;
  CallsOrigin *origin;

  ok = gtk_combo_box_get_active_iter (self->origin_box, &iter);
  if (!ok)
    {
      return NULL;
    }

  gtk_tree_model_get (GTK_TREE_MODEL (self->origin_store),
                      &iter,
                      ORIGIN_STORE_COLUMN_ORIGIN, &origin,
                      -1);
  g_assert (CALLS_IS_ORIGIN (origin));

  return origin;
}


static void
long_press_back_cb (CallsNewCallBox *self)
{
  GtkEntry *entry = hdy_keypad_get_entry (self->keypad);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
}

static void
backspace_clicked_cb (CallsNewCallBox *self)
{
  GtkEntry *entry = hdy_keypad_get_entry (self->keypad);
  g_signal_emit_by_name (entry, "backspace", NULL);
}

static void
ussd_send_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  CallsNewCallBox *self;
  CallsUssd *ussd = (CallsUssd *)object;
  g_autoptr (GTask) task = user_data;
  GError *error = NULL;
  char *response;

  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);

  g_assert (CALLS_IS_NEW_CALL_BOX (self));
  g_assert (CALLS_IS_USSD (ussd));

  response = calls_ussd_initiate_finish (ussd, result, &error);
  g_task_set_task_data (task, g_object_ref (ussd), g_object_unref);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, response, g_free);
}

static void
dial_clicked_cb (CallsNewCallBox *self)
{
  GtkEntry *entry = hdy_keypad_get_entry (self->keypad);
  GtkWidget *window;
  const char *text;

  window = gtk_widget_get_toplevel (GTK_WIDGET (self));
  text = gtk_entry_get_text (entry);

  if (CALLS_IS_MAIN_WINDOW (window))
    calls_main_window_dial (CALLS_MAIN_WINDOW (window), text);
  else
    calls_new_call_box_dial (self, text);
}


static void
dial_queued_cb (gchar       *target,
                CallsOrigin *origin)
{
  g_debug ("Dialing queued target `%s'", target);
  calls_origin_dial (origin, target);
}


static void
clear_dial_queue (CallsNewCallBox *self)
{
  g_list_free_full (self->dial_queue, g_free);
  self->dial_queue = NULL;
}


static void
dial_queued (CallsNewCallBox *self)
{
  CallsOrigin *origin;

  if (!self->dial_queue)
    {
      return;
    }

  g_debug ("Dialing %u queued targets",
           g_list_length (self->dial_queue));

  origin = get_origin (self);
  g_assert (origin != NULL);

  g_list_foreach (self->dial_queue,
                  (GFunc)dial_queued_cb,
                  origin);
  g_object_unref (origin);

  clear_dial_queue (self);
}


void
update_origin_box (CallsNewCallBox *self)
{
  GtkTreeModel *origin_store = GTK_TREE_MODEL (self->origin_store);
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_first (origin_store, &iter))
    {
      gtk_widget_hide (GTK_WIDGET (self->origin_box));
      gtk_widget_set_sensitive (GTK_WIDGET (self->dial), FALSE);
      return;
    }

  /* We know there is at least one origin. */

  gtk_widget_set_sensitive (GTK_WIDGET (self->dial), TRUE);

  if (!gtk_tree_model_iter_next (origin_store, &iter))
    {
      gtk_combo_box_set_active (self->origin_box, 0);
      gtk_widget_hide (GTK_WIDGET (self->origin_box));
    }
  else
    {
      /* We know there are multiple origins. */

      if (gtk_combo_box_get_active (self->origin_box) < 0)
        {
          gtk_combo_box_set_active (self->origin_box, 0);
        }

      /* We know there are multiple origins and one is selected. */

      gtk_widget_show (GTK_WIDGET (self->origin_box));
    }

  dial_queued (self);
}


static void
add_origin (CallsNewCallBox *self, CallsOrigin *origin)
{
  GtkTreeIter iter;

  gtk_list_store_append (self->origin_store, &iter);
  gtk_list_store_set (self->origin_store, &iter,
                      ORIGIN_STORE_COLUMN_NAME, calls_origin_get_name (origin),
                      ORIGIN_STORE_COLUMN_ORIGIN, G_OBJECT (origin),
                      -1);

  update_origin_box (self);
}


static void
remove_origin (CallsNewCallBox *self, CallsOrigin *origin)
{
  GtkTreeIter iter;
  gboolean ok;

  ok = calls_list_store_find (self->origin_store, origin,
                              ORIGIN_STORE_COLUMN_ORIGIN, &iter);
  g_return_if_fail (ok);

  gtk_list_store_remove (self->origin_store, &iter);

  update_origin_box (self);
}


static void
remove_origins (CallsNewCallBox *self)
{
  GtkTreeModel *model = GTK_TREE_MODEL (self->origin_store);
  GtkTreeIter iter;

  while (gtk_tree_model_get_iter_first (model, &iter))
    {
      gtk_list_store_remove (self->origin_store, &iter);
    }
}


static void
calls_new_call_box_init (CallsNewCallBox *self)
{
  g_autoptr (GList) origins = NULL;
  GList *o;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_swapped (calls_manager_get_default (),
                            "origin-add",
                            G_CALLBACK (add_origin),
                            self);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "origin-remove",
                            G_CALLBACK (remove_origin),
                            self);

  origins = calls_manager_get_origins (calls_manager_get_default ());
  for (o = origins; o != NULL; o = o->next) {
    add_origin (self, o->data);
  }

  update_origin_box (self);
}


static void
dispose (GObject *object)
{
  CallsNewCallBox *self = CALLS_NEW_CALL_BOX (object);

  clear_dial_queue (self);

  if (self->origin_store)
    {
      remove_origins (self);
    }

  if (self->long_press_back_gesture != NULL)
    g_object_unref (self->long_press_back_gesture);

  G_OBJECT_CLASS (calls_new_call_box_parent_class)->dispose (object);
}


static void
calls_new_call_box_class_init (CallsNewCallBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/new-call-box.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, origin_store);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, origin_box);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, backspace);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, long_press_back_gesture);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, keypad);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, dial);
  gtk_widget_class_bind_template_callback (widget_class, dial_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, backspace_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, long_press_back_cb);
}


CallsNewCallBox *
calls_new_call_box_new (void)
{
  return g_object_new (CALLS_TYPE_NEW_CALL_BOX, NULL);
}


void
calls_new_call_box_dial (CallsNewCallBox *self,
                         const gchar     *target)
{
  CallsOrigin *origin;

  g_return_if_fail (CALLS_IS_NEW_CALL_BOX (self));
  g_return_if_fail (target != NULL);

  origin = get_origin (self);
  if (!origin)
    {
      // Queue for dialing when an origin appears
      g_debug ("Can't submit call with no origin, queuing for later");
      self->dial_queue = g_list_append (self->dial_queue,
                                        g_strdup (target));
      return;
    }

  calls_origin_dial (origin, target);
  g_object_unref (origin);
}

void
calls_new_call_box_send_ussd_async (CallsNewCallBox     *self,
                                    const char          *target,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr (CallsOrigin) origin = NULL;
  g_autoptr (GTask) task = NULL;
  GtkEntry *entry;

  g_return_if_fail (CALLS_IS_NEW_CALL_BOX (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (target && *target);

  origin = get_origin (self);

  task = g_task_new (self, cancellable, callback, user_data);

  if (!CALLS_IS_USSD (origin))
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "No origin with USSD available");
      return;
    }

  if (!calls_number_is_ussd (target))
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "%s is not a valid USSD code", target);
      return;
    }

  calls_ussd_initiate_async (CALLS_USSD (origin), target, cancellable,
                             ussd_send_cb, g_steal_pointer (&task));

  entry = hdy_keypad_get_entry (self->keypad);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
}

char *
calls_new_call_box_send_ussd_finish (CallsNewCallBox *self,
                                     GAsyncResult    *result,
                                     GError          **error)
{
  g_return_val_if_fail (CALLS_IS_NEW_CALL_BOX (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
