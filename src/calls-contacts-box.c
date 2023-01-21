/*
 * Copyright (C) 2020 Purism SPC
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
 * Author: Julian Sparber <julian@sparber.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "calls-manager.h"
#include "calls-contacts-provider.h"
#include "calls-contacts-box.h"
#include "calls-contacts-row.h"

#include <glib/gi18n.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

struct _CallsContactsBox {
  GtkBin            parent_instance;

  GtkWidget        *search_entry;
  GtkWidget        *contacts_frame;
  GtkWidget        *contacts_listbox;
  GtkWidget        *placeholder_empty;

  FolksSimpleQuery *search_query;
};

G_DEFINE_TYPE (CallsContactsBox, calls_contacts_box, GTK_TYPE_BIN);

static void
search_changed_cb (CallsContactsBox *self,
                   GtkEntry         *entry)
{
  const gchar *search_text;

  search_text = gtk_entry_get_text (entry);

  folks_simple_query_set_query_string (self->search_query, search_text);

  gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->contacts_listbox));
}

static gboolean
contacts_filter_func (CallsContactsRow *row,
                      CallsContactsBox *self)
{
  FolksIndividual *item = calls_contacts_row_get_item (row);

  return folks_query_is_match (FOLKS_QUERY (self->search_query), item) > 0;
}


static void
adjust_style (CallsContactsBox *self, GtkWidget *widget)
{
  g_return_if_fail (CALLS_IS_CONTACTS_BOX (self));

  if (gtk_widget_get_mapped (widget)) {
    gtk_frame_set_shadow_type (GTK_FRAME (self->contacts_frame), GTK_SHADOW_NONE);
    gtk_widget_set_vexpand (self->contacts_frame, TRUE);
    gtk_style_context_add_class (gtk_widget_get_style_context (self->contacts_listbox),
                                 "no-background");
  } else {
    gtk_frame_set_shadow_type (GTK_FRAME (self->contacts_frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_set_vexpand (self->contacts_frame, FALSE);
    gtk_style_context_remove_class (gtk_widget_get_style_context (self->contacts_listbox),
                                    "no-background");
  }
}

static void
header_cb (GtkListBoxRow *row,
           GtkListBoxRow *before)
{
  if (!before) {
    gtk_list_box_row_set_header (row, NULL);
    return;
  }


  if (!gtk_list_box_row_get_header (row))
    gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
}

static void
on_favourite_changed (CallsContactsBox *self)
{
  g_assert (CALLS_IS_CONTACTS_BOX (self));

  gtk_list_box_invalidate_sort (GTK_LIST_BOX (self->contacts_listbox));
}

static void
contacts_provider_added (CallsContactsBox *self,
                         FolksIndividual  *individual)
{
  GtkWidget *row;

  row = calls_contacts_row_new (individual);

  gtk_container_add (GTK_CONTAINER (self->contacts_listbox), row);

  g_signal_connect_object (individual,
                           "notify::is-favourite",
                           G_CALLBACK (on_favourite_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
contacts_provider_removed (CallsContactsBox *self,
                           FolksIndividual  *individual)
{
  g_autoptr (GList) list = gtk_container_get_children (GTK_CONTAINER (self->contacts_listbox));
  GList *l;

  for (l = list; l != NULL; l = l->next) {
    CallsContactsRow *row = CALLS_CONTACTS_ROW (l->data);
    if (calls_contacts_row_get_item (row) == individual)
      gtk_container_remove (GTK_CONTAINER (self->contacts_listbox), GTK_WIDGET (row));
  }
}

static gint
contacts_sort_func (CallsContactsRow *a,
                    CallsContactsRow *b,
                    gpointer          unused)
{
  FolksIndividual *individual_a = calls_contacts_row_get_item (a);
  FolksIndividual *individual_b = calls_contacts_row_get_item (b);
  const char *name_a = folks_individual_get_display_name (individual_a);
  const char *name_b = folks_individual_get_display_name (individual_b);
  gboolean fav_a;
  gboolean fav_b;

  g_object_get (G_OBJECT (individual_a), "is-favourite", &fav_a, NULL);
  g_object_get (G_OBJECT (individual_b), "is-favourite", &fav_b, NULL);

  if (fav_a == fav_b)
    return g_strcmp0 (name_a, name_b);

  return fav_a ? -1 : 1;
}


static void
calls_contacts_box_class_init (CallsContactsBoxClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/contacts-box.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsContactsBox, contacts_listbox);
  gtk_widget_class_bind_template_child (widget_class, CallsContactsBox, contacts_frame);
  gtk_widget_class_bind_template_child (widget_class, CallsContactsBox, search_entry);
  gtk_widget_class_bind_template_child (widget_class, CallsContactsBox, placeholder_empty);
}


static void
calls_contacts_box_init (CallsContactsBox *self)
{
  CallsContactsProvider *contacts_provider;

  g_autoptr (GeeCollection) individuals = NULL;
  gchar* query_fields[] = { "alias",
                            "full-name",
                            "nickname",
                            "structured-name",
                            "phone-numbers" };

  gtk_widget_init_template (GTK_WIDGET (self));

  self->search_query = folks_simple_query_new ("", query_fields, G_N_ELEMENTS (query_fields));

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->contacts_listbox),
                                (GtkListBoxUpdateHeaderFunc) header_cb,
                                NULL,
                                NULL);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->contacts_listbox),
                              (GtkListBoxSortFunc) contacts_sort_func,
                              NULL,
                              NULL);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (self->contacts_listbox),
                                (GtkListBoxFilterFunc) contacts_filter_func,
                                self,
                                NULL);

  g_signal_connect_swapped (self->placeholder_empty, "map", G_CALLBACK (adjust_style), self);
  g_signal_connect_swapped (self->placeholder_empty, "unmap", G_CALLBACK (adjust_style), self);

  contacts_provider = calls_manager_get_contacts_provider (calls_manager_get_default ());
  individuals = calls_contacts_provider_get_individuals (contacts_provider);

  g_signal_connect_swapped (contacts_provider,
                            "added",
                            G_CALLBACK (contacts_provider_added),
                            self);

  g_signal_connect_swapped (contacts_provider,
                            "removed",
                            G_CALLBACK (contacts_provider_removed),
                            self);

  g_signal_connect_swapped (self->search_entry,
                            "search-changed",
                            G_CALLBACK (search_changed_cb),
                            self);

  if (!gee_collection_get_is_empty (individuals))
    calls_contacts_provider_consume_iter_on_idle (gee_iterable_iterator (GEE_ITERABLE (individuals)),
                                                  (IdleCallback) contacts_provider_added,
                                                  self);
}


GtkWidget *
calls_contacts_box_new (void)
{
  return g_object_new (CALLS_TYPE_CONTACTS_BOX, NULL);
}
