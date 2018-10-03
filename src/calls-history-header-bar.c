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

#include "calls-history-header-bar.h"

struct _CallsHistoryHeaderBar
{
  GtkHeaderBar parent_instance;
};

G_DEFINE_TYPE (CallsHistoryHeaderBar, calls_history_header_bar, GTK_TYPE_HEADER_BAR);


static void
calls_history_header_bar_init (CallsHistoryHeaderBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


static void
calls_history_header_bar_class_init (CallsHistoryHeaderBarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/history-header-bar.ui");
}

