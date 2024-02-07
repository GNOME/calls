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

#include <adwaita.h>

struct _CallsContactsBox {
  AdwBin            parent_instance;

  GtkWidget        *child;

  GtkWidget        *search_entry;
  GtkWidget        *contacts_frame;
  GtkWidget        *contacts_listbox;
  GtkWidget        *placeholder_empty;

  GListStore       *contacts_list;
  GtkCustomSorter  *contacts_sorter;
  FolksSimpleQuery *search_query;
  GtkCustomFilter  *search_filter;
};

G_DEFINE_TYPE (CallsContactsBox, calls_contacts_box, ADW_TYPE_BIN);

static void
search_changed_cb (CallsContactsBox *self,
                   GtkEntry         *entry)
{
  const gchar *search_text;

  search_text = gtk_editable_get_text (GTK_EDITABLE (entry));

  folks_simple_query_set_query_string (self->search_query, search_text);

  gtk_filter_changed (GTK_FILTER (self->search_filter), GTK_FILTER_CHANGE_DIFFERENT);
}

static gboolean
contacts_filter_func (FolksIndividual *item,
                      CallsContactsBox *self)
{
  return folks_query_is_match (FOLKS_QUERY (self->search_query), item) > 0;
}


static void
adjust_style (CallsContactsBox *self, GtkWidget *widget)
{
  g_return_if_fail (CALLS_IS_CONTACTS_BOX (self));

  if (gtk_widget_get_mapped (widget)) {
    gtk_widget_set_vexpand (self->contacts_frame, TRUE);
    gtk_widget_add_css_class (self->contacts_listbox, "no-background");
  } else {
    gtk_widget_set_vexpand (self->contacts_frame, FALSE);
    gtk_widget_remove_css_class (self->contacts_listbox, "no-background");
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

  gtk_sorter_changed (GTK_SORTER (self->contacts_sorter), GTK_SORTER_CHANGE_DIFFERENT);
}

static void
contacts_provider_added (CallsContactsBox *self,
                         FolksIndividual  *individual)
{
  g_list_store_append (G_LIST_STORE (self->contacts_list), individual);

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
  guint position;
  while (g_list_store_find (G_LIST_STORE (self->contacts_list), individual, &position)) {
    g_list_store_remove(G_LIST_STORE (self->contacts_list), position);
  }
}

static gint
contacts_sort_func (FolksIndividual *a,
                    FolksIndividual *b,
                    gpointer          unused)
{
  const char *name_a = folks_individual_get_display_name (a);
  const char *name_b = folks_individual_get_display_name (b);
  gboolean fav_a;
  gboolean fav_b;

  g_object_get (G_OBJECT (a), "is-favourite", &fav_a, NULL);
  g_object_get (G_OBJECT (b), "is-favourite", &fav_b, NULL);

  if (fav_a == fav_b)
    return g_strcmp0 (name_a, name_b);

  return fav_a ? -1 : 1;
}

static void
on_main_window_closed (CallsContactsBox *self)
{
  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
}

static void
calls_contacts_box_dispose (GObject *object)
{
  CallsContactsBox *self = CALLS_CONTACTS_BOX (object);

  GtkWidget *child = self->child;

  g_clear_pointer (&child, gtk_widget_unparent);
}


static void
calls_contacts_box_class_init (CallsContactsBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = calls_contacts_box_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/contacts-box.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsContactsBox, child);
  gtk_widget_class_bind_template_child (widget_class, CallsContactsBox, contacts_listbox);
  gtk_widget_class_bind_template_child (widget_class, CallsContactsBox, contacts_frame);
  gtk_widget_class_bind_template_child (widget_class, CallsContactsBox, search_entry);
  gtk_widget_class_bind_template_child (widget_class, CallsContactsBox, placeholder_empty);
}


static void
calls_contacts_box_init (CallsContactsBox *self)
{
  CallsContactsProvider *contacts_provider;
  GtkSortListModel *sorted_contacts;
  GtkFilterListModel *filtered_contacts;

  g_autoptr (GeeCollection) individuals = NULL;
  gchar* query_fields[] = { "alias",
                            "full-name",
                            "nickname",
                            "structured-name",
                            "phone-numbers" };

  gtk_widget_init_template (GTK_WIDGET (self));

  self->contacts_list = g_list_store_new(FOLKS_TYPE_INDIVIDUAL);

  self->contacts_sorter = gtk_custom_sorter_new ((GCompareDataFunc) contacts_sort_func, NULL, NULL);
  sorted_contacts = gtk_sort_list_model_new (G_LIST_MODEL (g_object_ref (self->contacts_list)), GTK_SORTER (g_object_ref (self->contacts_sorter)));

  self->search_query = folks_simple_query_new ("", query_fields, G_N_ELEMENTS (query_fields));
  self->search_filter = gtk_custom_filter_new ((GtkCustomFilterFunc) contacts_filter_func, self, NULL);
  filtered_contacts = gtk_filter_list_model_new (G_LIST_MODEL (sorted_contacts), GTK_FILTER (g_object_ref (self->search_filter)));

  gtk_list_box_bind_model (GTK_LIST_BOX (self->contacts_listbox),
                           G_LIST_MODEL (filtered_contacts),
                           (GtkListBoxCreateWidgetFunc) calls_contacts_row_new,
                           NULL,
                           NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->contacts_listbox),
                                (GtkListBoxUpdateHeaderFunc) header_cb,
                                NULL,
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

  g_signal_connect_swapped (g_application_get_default (),
                            "main-window-closed",
                            G_CALLBACK (on_main_window_closed),
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
