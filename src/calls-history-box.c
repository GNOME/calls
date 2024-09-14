/*
 * Copyright (C) 2018, 2019, 2022 Purism SPC
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
 *         Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
#define G_LOG_DOMAIN "CallsHistoryBox"

#include "calls-history-box.h"
#include "calls-call-record.h"
#include "calls-call-record-row.h"
#include "calls-util.h"

#include <glib/gi18n.h>
#include <glib-object.h>

#define CALLS_HISTORY_SIZE_INITIAL 75
#define CALLS_HISTORY_SIZE_INCREMENTS 50
#define CALLS_HISTORY_RESET_SIZE_POSITION_THRESHOLD 500
#define CALLS_HISTORY_INCREASE_N_PAGES_THRESHOLD 2

struct _CallsHistoryBox {
  AdwBin             parent_instance;

  GtkStack          *stack;
  GtkListBox        *history;
  GtkScrolledWindow *scrolled_window;
  GtkAdjustment     *scroll_adjustment;

  GListModel        *model;
  GtkSliceListModel *slice_model;

  gsize              n_items;

  gulong             model_changed_handler_id;

};

G_DEFINE_TYPE (CallsHistoryBox, calls_history_box, ADW_TYPE_BIN);


enum {
  PROP_0,
  PROP_MODEL,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
on_model_changed (GListModel      *model,
                  guint            position,
                  guint            removed,
                  guint            added,
                  CallsHistoryBox *self)
{
  gchar *child_name;

  self->n_items = self->n_items + added - removed;
  if (self->n_items == 0)
    child_name = "empty";
  else
    child_name = "history";

  gtk_stack_set_visible_child_name (self->stack, child_name);
}


static void
delete_call_cb (CallsCallRecord *record,
                CallsHistoryBox *self)
{
  guint position;
  guint id;
  gboolean ok;

  g_return_if_fail (CALLS_IS_CALL_RECORD (record));

  ok = calls_find_in_model (self->model,
                            record,
                            &position);

  g_object_get (G_OBJECT (record),
                "id",
                &id,
                NULL);

  if (!ok) {
    g_warning ("Could not find record with id %u in model",
               id);
    return;
  }

  g_list_store_remove ((GListStore *) self->model, position);
}


static GtkWidget *
create_row_cb (CallsCallRecord *record,
               CallsHistoryBox *self)
{
  GtkWidget *row_widget;

  row_widget = GTK_WIDGET (calls_call_record_row_new (record));

  g_debug ("Created new row [%p] for record [%p]",
           row_widget, record);

  g_signal_connect (record,
                    "call-delete",
                    G_CALLBACK (delete_call_cb),
                    self);
  return row_widget;
}


static void
on_adjustment_position_changed (GtkAdjustment   *adjustment,
                                CallsHistoryBox *self)
{
  double position;
  double upper_limit;
  double page_size;
  guint old_size;

  g_assert (CALLS_IS_HISTORY_BOX (self));

  position = gtk_adjustment_get_value (adjustment);
  page_size = gtk_adjustment_get_page_size (adjustment);
  old_size = gtk_slice_list_model_get_size (self->slice_model);

  if (position < CALLS_HISTORY_RESET_SIZE_POSITION_THRESHOLD) {
    if (old_size == CALLS_HISTORY_SIZE_INITIAL)
      return;

    g_debug ("Resetting to initial size: %u",
             CALLS_HISTORY_SIZE_INITIAL);
    gtk_slice_list_model_set_size (self->slice_model, CALLS_HISTORY_SIZE_INITIAL);
    return;
  }

  upper_limit = gtk_adjustment_get_upper (adjustment);

  if (position > upper_limit - CALLS_HISTORY_INCREASE_N_PAGES_THRESHOLD * page_size) {
    guint new_size = old_size + CALLS_HISTORY_SIZE_INCREMENTS;

    new_size = MIN (new_size, self->n_items);

    if (old_size == new_size)
      return;

    g_debug ("Increasing history slice from %u to %u",
             old_size, new_size);
    gtk_slice_list_model_set_size (self->slice_model, new_size);
  }
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsHistoryBox *self = CALLS_HISTORY_BOX (object);

  switch (property_id) {
  case PROP_MODEL:
    g_set_object (&self->model,
                  G_LIST_MODEL (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
constructed (GObject *object)
{
  CallsHistoryBox *self = CALLS_HISTORY_BOX (object);

  g_assert (self->model != NULL);

  G_OBJECT_CLASS (calls_history_box_parent_class)->constructed (object);

  self->slice_model = gtk_slice_list_model_new (g_object_ref (self->model),
                                                0,
                                                CALLS_HISTORY_SIZE_INITIAL);

  self->model_changed_handler_id =
    g_signal_connect (self->model,
                      "items-changed",
                      G_CALLBACK (on_model_changed),
                      self);
  g_assert (self->model_changed_handler_id != 0);

  gtk_list_box_bind_model (self->history,
                           G_LIST_MODEL (self->slice_model),
                           (GtkListBoxCreateWidgetFunc) create_row_cb,
                           self,
                           NULL);

  on_model_changed (self->model, 0, 0, g_list_model_get_n_items (self->model), self);
}


static void
dispose (GObject *object)
{
  CallsHistoryBox *self = CALLS_HISTORY_BOX (object);

  g_clear_signal_handler (&self->model_changed_handler_id, self->model);
  g_clear_object (&self->slice_model);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (calls_history_box_parent_class)->dispose (object);
}


static void
calls_history_box_class_init (CallsHistoryBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;

  props[PROP_MODEL] =
    g_param_spec_object ("model",
                         "model",
                         "The data store containing call records",
                         G_TYPE_LIST_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/history-box.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsHistoryBox, stack);
  gtk_widget_class_bind_template_child (widget_class, CallsHistoryBox, history);
  gtk_widget_class_bind_template_child (widget_class, CallsHistoryBox, scrolled_window);
}


static void
calls_history_box_init (CallsHistoryBox *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->scroll_adjustment = gtk_scrolled_window_get_vadjustment (self->scrolled_window);

  g_signal_connect (self->scroll_adjustment,
                    "value-changed",
                    G_CALLBACK (on_adjustment_position_changed),
                    self);
}


CallsHistoryBox *
calls_history_box_new (GListModel *model)
{
  return g_object_new (CALLS_TYPE_HISTORY_BOX,
                       "model", model,
                       NULL);
}
