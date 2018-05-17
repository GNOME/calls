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

#include "util.h"

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
