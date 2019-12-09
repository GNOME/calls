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
 * Author: Bob Ham <bob.ham@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "util.h"


void
calls_object_unref (gpointer object)
{
  if (object)
    {
      g_object_unref (object);
    }
}


typedef struct
{
  gpointer needle;
  guint needle_column;
  GtkTreeIter *iter;
  gboolean found;
} ListStoreFindData;

static gboolean
list_store_find_foreach_cb (GtkTreeModel *model,
                            GtkTreePath  *path,
                            GtkTreeIter  *iter,
                            gpointer      data)
{
  ListStoreFindData *find_data = data;
  gpointer value;

  gtk_tree_model_get (model, iter, find_data->needle_column,
                      &value, -1);

  if (value == find_data->needle)
    {
      *find_data->iter = *iter;
      return (find_data->found = TRUE);
    }

  return FALSE;
}

gboolean
calls_list_store_find (GtkListStore *store,
                       gpointer needle,
                       gint needle_column,
                       GtkTreeIter *iter)
{
  ListStoreFindData find_data
    = { needle, needle_column, iter, FALSE };

  gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                          list_store_find_foreach_cb,
                          &find_data);

  return find_data.found;
}


void
calls_entry_append (GtkEntry *entry,
                    gchar     character)
{
  const gchar str[] = {character, '\0'};
  GtkEntryBuffer *buf;
  guint len;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  buf = gtk_entry_get_buffer (entry);
  len = gtk_entry_buffer_get_length (buf);

  gtk_entry_buffer_insert_text (buf, len, str, 1);
}


gboolean
calls_date_time_is_same_day (GDateTime *a,
                             GDateTime *b)
{
#define eq(member)                              \
  (g_date_time_get_##member (a) ==              \
   g_date_time_get_##member (b))

  return
    eq (year)
    &&
    eq (month)
    &&
    eq (day_of_month);

#undef eq
}


gboolean
calls_date_time_is_yesterday (GDateTime *now,
                              GDateTime *t)
{
  GDateTime *yesterday;
  gboolean same_day;

  yesterday = g_date_time_add_days (now, -1);

  same_day = calls_date_time_is_same_day (yesterday, t);

  g_date_time_unref (yesterday);

  return same_day;
}


gboolean
calls_date_time_is_same_year (GDateTime *a,
                              GDateTime *b)
{
  return
    g_date_time_get_year (a) ==
    g_date_time_get_year (b);
}
