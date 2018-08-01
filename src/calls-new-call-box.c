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

#include "calls-origin.h"

#include <glib/gi18n.h>
#define HANDY_USE_UNSTABLE_API
#include <handy.h>


struct _CallsNewCallBox
{
  GtkBox parent_instance;

  GtkComboBox *origin_box;
  GtkSearchEntry *number_entry;
  HdyDialer *dial_pad;

  gulong origin_store_row_deleted_id;
  gulong origin_store_row_inserted_id;
};

G_DEFINE_TYPE (CallsNewCallBox, calls_new_call_box, GTK_TYPE_BOX);


enum {
  PROP_0,
  PROP_ORIGIN_STORE,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


enum {
  SIGNAL_SUBMITTED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];


enum {
  ORIGIN_STORE_COLUMN_NAME,
  ORIGIN_STORE_COLUMN_ORIGIN
};


static void
dial_pad_symbol_clicked_cb (CallsNewCallBox *self,
                            gchar            symbol,
                            HdyDialer       *dialer)
{
  GtkEntryBuffer *buf = gtk_entry_get_buffer (GTK_ENTRY (self->number_entry));
  guint len = gtk_entry_buffer_get_length (buf);

  gtk_entry_buffer_insert_text (buf, len, &symbol, 1);
}


static void
dial_pad_deleted_cb (CallsNewCallBox *self,
                     HdyDialer       *dialer)
{
  GtkEntryBuffer *buf = gtk_entry_get_buffer (GTK_ENTRY (self->number_entry));
  guint len = gtk_entry_buffer_get_length (buf);

  gtk_entry_buffer_delete_text (buf, len - 1, 1);
}


static void
dial_clicked_cb (CallsNewCallBox *self,
                 const gchar     *unused,
                 GtkButton       *button)
{
  GtkTreeIter iter;
  GtkTreeModel *origin_store;
  CallsOrigin *origin;
  const gchar *number;

  gtk_combo_box_get_active_iter (self->origin_box, &iter);
  if (!gtk_combo_box_get_active_iter (self->origin_box, &iter))
    {
      g_debug ("Can't submit call with no origin.");

      return;
    }

  origin_store = gtk_combo_box_get_model (self->origin_box);
  gtk_tree_model_get (origin_store, &iter,
                      ORIGIN_STORE_COLUMN_ORIGIN, &origin,
                      -1);
  g_assert (CALLS_IS_ORIGIN (origin));

  number = gtk_entry_get_text (GTK_ENTRY (self->number_entry));

  g_signal_emit (self, signals[SIGNAL_SUBMITTED], 0, origin, number);
}


void
update_origin_box (CallsNewCallBox *self)
{
  GtkTreeModel *origin_store = gtk_combo_box_get_model (self->origin_box);
  GtkTreeIter iter;

  if (origin_store == NULL ||
      !gtk_tree_model_get_iter_first (origin_store, &iter))
    {
      gtk_widget_hide (GTK_WIDGET (self->origin_box));

      return;
    }

  /* We know there is a model and it's not empty. */

  if (!gtk_tree_model_iter_next (origin_store, &iter))
    {
      gtk_combo_box_set_active (self->origin_box, 0);
      gtk_widget_hide (GTK_WIDGET (self->origin_box));

      return;
    }

  /* We know there are multiple origins in the model. */

  if (gtk_combo_box_get_active (self->origin_box) < 0)
    {
      gtk_combo_box_set_active (self->origin_box, 0);
    }

  /* We know there are multiple origins and one is selected. */

  gtk_widget_show (GTK_WIDGET (self->origin_box));
}


static void
calls_new_call_box_set_origin_store (CallsNewCallBox *self,
                                     GtkListStore    *origin_store)
{
  g_return_if_fail (CALLS_IS_NEW_CALL_BOX (self));
  g_return_if_fail (origin_store == NULL || GTK_IS_LIST_STORE (origin_store));

  if (self->origin_store_row_deleted_id != 0)
    {
      g_signal_handler_disconnect (gtk_combo_box_get_model (self->origin_box),
                                   self->origin_store_row_deleted_id);
      g_signal_handler_disconnect (gtk_combo_box_get_model (self->origin_box),
                                   self->origin_store_row_inserted_id);
    }

  gtk_combo_box_set_model (self->origin_box, GTK_TREE_MODEL (origin_store));

  if (origin_store != NULL)
    {
      self->origin_store_row_deleted_id = g_signal_connect_swapped (origin_store, "row-deleted", G_CALLBACK (update_origin_box), self);
      self->origin_store_row_inserted_id = g_signal_connect_swapped (origin_store, "row-inserted", G_CALLBACK (update_origin_box), self);
    }
  else
    {
      self->origin_store_row_deleted_id = 0;
      self->origin_store_row_inserted_id = 0;
    }
}


static void
calls_new_call_box_init (CallsNewCallBox *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  update_origin_box (self);
}


static void
calls_new_call_box_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  CallsNewCallBox *self = CALLS_NEW_CALL_BOX (object);

  switch (property_id)
    {
      case PROP_ORIGIN_STORE:
        calls_new_call_box_set_origin_store (self, g_value_get_object (value));
        g_object_notify_by_pspec (object, pspec);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}


static void
calls_new_call_box_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  CallsNewCallBox *self = CALLS_NEW_CALL_BOX (object);

  switch (property_id)
    {
      case PROP_ORIGIN_STORE:
        g_value_set_object (value, gtk_combo_box_get_model (self->origin_box));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}


static void
calls_new_call_box_class_init (CallsNewCallBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = calls_new_call_box_set_property;
  object_class->get_property = calls_new_call_box_get_property;

  props[PROP_ORIGIN_STORE] =
    g_param_spec_object ("origin-store",
                         _("Origin store"),
                         _("The storage for origins"),
                         GTK_TYPE_LIST_STORE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  /**
   *  CallsNewCallBox::submitted:
   * @self: The # CallsNewCallBox instance.
   * @origin: The origin of the call.
   * @number: The number at the time of activation.
   *
   * This signal is emitted when the dialer's 'dial' button is activated.
   * Connect to this signal to perform to get notified when the user
   * wants to submit the dialed number for a given call origin.
   */
  signals[SIGNAL_SUBMITTED] =
    g_signal_new ("submitted",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  CALLS_TYPE_ORIGIN,
                  G_TYPE_STRING);

  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/new-call-box.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, origin_box);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, number_entry);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, dial_pad);
  gtk_widget_class_bind_template_callback (widget_class, dial_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, dial_pad_deleted_cb);
  gtk_widget_class_bind_template_callback (widget_class, dial_pad_symbol_clicked_cb);
}
