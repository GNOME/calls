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

#include "calls-history-box.h"
#include "calls-origin.h"
#include "calls-call-holder.h"
#include "calls-call-selector-item.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>


struct _CallsHistoryBox
{
  GtkStack parent_instance;

  GtkListStore *history_store;
};

G_DEFINE_TYPE (CallsHistoryBox, calls_history_box, GTK_TYPE_STACK)


static void
calls_history_box_init (CallsHistoryBox *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


static void
calls_history_box_class_init (CallsHistoryBoxClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);


  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/history-box.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsHistoryBox, history_store);
}
