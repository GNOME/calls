/*
 * Copyright (C) 2018, 2019 Purism SPC
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

#include "calls-history-box.h"
#include "calls-call-record.h"
#include "calls-call-record-row.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>


struct _CallsHistoryBox {
  GtkStack    parent_instance;

  GtkListBox *history;

  GListModel *model;
  gulong      model_changed_handler_id;

};

G_DEFINE_TYPE (CallsHistoryBox, calls_history_box, GTK_TYPE_STACK);


enum {
  PROP_0,
  PROP_MODEL,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
update (CallsHistoryBox *self)
{
  gchar *child_name;

  if (g_list_model_get_n_items (self->model) == 0) {
    child_name = "empty";
  } else {
    child_name = "history";

    /* Transition should only ever be from empty to non-empty */
    if (self->model_changed_handler_id != 0)
      calls_clear_signal (self->model,
                          &self->model_changed_handler_id);
  }

  gtk_stack_set_visible_child_name (GTK_STACK (self), child_name);
}


static void
delete_call_cb (CallsCallRecord *record,
                CallsHistoryBox *self)
{
  guint position;
  guint id;
  gboolean ok;

  g_return_if_fail (CALLS_IS_CALL_RECORD (record));

  ok = calls_find_in_store (self->model,
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

  update (self);
}


static GtkWidget *
create_row_cb (CallsCallRecord *record,
               CallsHistoryBox *self)
{
  GtkWidget *row_widget;

  row_widget = GTK_WIDGET (calls_call_record_row_new (record));

  g_signal_connect (record,
                    "call-delete",
                    G_CALLBACK (delete_call_cb),
                    self);
  return row_widget;
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

  self->model_changed_handler_id =
    g_signal_connect_swapped
      (self->model, "items-changed", G_CALLBACK (update), self);
  g_assert (self->model_changed_handler_id != 0);

  gtk_list_box_bind_model (self->history,
                           self->model,
                           (GtkListBoxCreateWidgetFunc) create_row_cb,
                           self,
                           NULL);

  update (self);

  G_OBJECT_CLASS (calls_history_box_parent_class)->constructed (object);
}


static void
dispose (GObject *object)
{
  CallsHistoryBox *self = CALLS_HISTORY_BOX (object);

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
  gtk_widget_class_bind_template_child (widget_class, CallsHistoryBox, history);
}


static void
calls_history_box_init (CallsHistoryBox *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


CallsHistoryBox *
calls_history_box_new (GListModel *model)
{
  return g_object_new (CALLS_TYPE_HISTORY_BOX,
                       "model", model,
                       NULL);
}
