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

#define G_LOG_DOMAIN "CallsNewCallBox"

#include "calls-account.h"
#include "calls-main-window.h"
#include "calls-manager.h"
#include "calls-new-call-box.h"
#include "calls-settings.h"
#include "calls-ussd.h"
#include "calls-util.h"

#include <adwaita.h>
#include <call-ui.h>
#include <glib/gi18n.h>

enum {
  PROP_0,
  PROP_NUMERIC_INPUT_ONLY,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

struct _CallsNewCallBox {
  AdwBin        parent_instance;

  GtkWidget    *child;

  GtkListBox   *origin_list_box;
  AdwComboRow  *origin_list;
  CuiDialpad   *dialpad;
  GtkEntry     *address_entry;
  AdwActionRow *result;
  GtkButton    *dial_result;

  GList        *dial_queue;

  gboolean      numeric_input_only;
};

G_DEFINE_TYPE (CallsNewCallBox, calls_new_call_box, ADW_TYPE_BIN);


static CallsOrigin *
get_selected_origin (CallsNewCallBox *self)
{
  g_autoptr (CallsOrigin) origin = NULL;
  GListModel *model = adw_combo_row_get_model (self->origin_list);
  gint index = -1;

  if (model)
    index = adw_combo_row_get_selected (self->origin_list);

  if (model && index >= 0)
    origin = g_list_model_get_item (model, index);

  return origin;
}


static CallsOrigin *
get_origin (CallsNewCallBox *self,
            const char      *target)
{
  CallsManager *manager = calls_manager_get_default ();
  CallsSettings *settings = calls_manager_get_settings (manager);

  GListModel *model;
  gboolean auto_use_def_origin =
    calls_settings_get_use_default_origins (settings);

  if (auto_use_def_origin) {
    guint n_items;

    model = calls_manager_get_suitable_origins (calls_manager_get_default (),
                                                target);
    n_items = g_list_model_get_n_items (model);

    if (n_items == 0)
      return NULL;

    for (guint i = 0; i < n_items; i++) {
      g_autoptr (CallsOrigin) origin = g_list_model_get_item (model, i);
      g_autofree char *origin_name = NULL;

      if (CALLS_IS_ACCOUNT (origin) &&
          calls_account_get_state (CALLS_ACCOUNT (origin)) != CALLS_ACCOUNT_STATE_ONLINE)
        continue;

      origin_name = calls_origin_get_name (origin);
      g_debug ("Using origin '%s' for call to '%s'",
               origin_name, target);

      return g_steal_pointer (&origin);
    }
  } else {
    return get_selected_origin (self);
  }

  return NULL;
}


static void
address_activate_cb (CallsNewCallBox *self)
{
  CallsOrigin *origin = get_selected_origin (self);
  const char *address = gtk_editable_get_text (GTK_EDITABLE (self->address_entry));

  if (origin && !STR_IS_NULL_OR_EMPTY (address))
    calls_origin_dial (origin, address);
}


static void
address_changed_cb (CallsNewCallBox *self)
{
  const char *address = gtk_editable_get_text (GTK_EDITABLE (self->address_entry));

  gtk_widget_set_visible (GTK_WIDGET (self->result),
                          !STR_IS_NULL_OR_EMPTY (address));
}


static void
set_numeric (CallsNewCallBox *self,
             gboolean         enable)
{
  if (enable == self->numeric_input_only)
    return;

  g_debug ("Numeric input %sabled", enable ? "en" : "dis");

  self->numeric_input_only = enable;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NUMERIC_INPUT_ONLY]);
}


static void
notify_selected_index_cb (CallsNewCallBox *self)
{
  CallsOrigin *origin = get_selected_origin (self);
  gboolean numeric_input = TRUE;

  if (origin)
    numeric_input = calls_origin_supports_protocol (origin, "tel");

  set_numeric (self, numeric_input);
}


static void
ussd_send_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  CallsNewCallBox *self;
  CallsUssd *ussd = (CallsUssd *) object;

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
dialpad_dialed_cb (CuiDialpad      *dialpad,
                   const char      *number,
                   CallsNewCallBox *self)
{
  GtkRoot *root;

  g_assert (CALLS_IS_NEW_CALL_BOX (self));

  root = gtk_widget_get_root (GTK_WIDGET (self));

  if (CALLS_IS_MAIN_WINDOW (root))
    calls_main_window_dial (CALLS_MAIN_WINDOW (root), number);
  else
    calls_new_call_box_dial (self, number);
}


static void
dial_result_clicked_cb (CallsNewCallBox *self)
{
  CallsOrigin *origin = get_selected_origin (self);
  const char *address = gtk_editable_get_text (GTK_EDITABLE (self->address_entry));

  if (origin && address && *address != '\0')
    calls_origin_dial (origin, address);
  else
    g_warning ("No suitable origin found. How was this even clicked?");
}


static void
dial_queued_cb (gchar           *target,
                CallsNewCallBox *self)
{
  CallsOrigin *origin = NULL;

  g_debug ("Try dialing queued target `%s'", target);

  origin = get_origin (self,
                       target);
  if (origin) {
    calls_origin_dial (origin, target);
    self->dial_queue = g_list_remove (self->dial_queue, target);
  } else {
    g_debug ("No suitable origin found");
  }
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
  if (!self->dial_queue)
    return;

  g_debug ("Try dialing %u queued targets",
           g_list_length (self->dial_queue));

  g_list_foreach (self->dial_queue,
                  (GFunc) dial_queued_cb,
                  self);
}


static void
origin_count_changed_cb (CallsNewCallBox *self)
{
  GListModel *origins;
  guint n_items = 0;

  g_assert (CALLS_IS_NEW_CALL_BOX (self));

  origins = calls_manager_get_origins (calls_manager_get_default ());
  n_items = g_list_model_get_n_items (origins);

  gtk_widget_set_visible (GTK_WIDGET (self->origin_list_box), n_items > 1);
  gtk_widget_set_sensitive (GTK_WIDGET (self->dialpad), n_items > 0);

  if (n_items)
    dial_queued (self);

  notify_selected_index_cb (self);
}


static void
calls_new_call_box_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  CallsNewCallBox *self = CALLS_NEW_CALL_BOX (object);

  switch (property_id) {
  case PROP_NUMERIC_INPUT_ONLY:
    g_value_set_boolean (value, self->numeric_input_only);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
on_main_window_closed (CallsNewCallBox *self)
{
  cui_dialpad_set_number (self->dialpad, "");
  gtk_editable_set_text (GTK_EDITABLE (self->address_entry), "");
}

static void
calls_new_call_box_init (CallsNewCallBox *self)
{
  GListModel *origins;
  GtkExpression *get_origin_name;

  gtk_widget_init_template (GTK_WIDGET (self));

  origins = calls_manager_get_origins (calls_manager_get_default ());
  adw_combo_row_set_model (self->origin_list, origins);
  get_origin_name = gtk_property_expression_new (CALLS_TYPE_ORIGIN,
                                                 NULL, "name");
  adw_combo_row_set_expression(self->origin_list, get_origin_name);

  g_signal_connect_object (origins,
                           "items-changed",
                           G_CALLBACK (origin_count_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_swapped (g_application_get_default (),
                            "main-window-closed",
                            G_CALLBACK (on_main_window_closed),
                            self);

  origin_count_changed_cb (self);
}


static void
calls_new_call_box_dispose (GObject *object)
{
  CallsNewCallBox *self = CALLS_NEW_CALL_BOX (object);

  GtkWidget *child = self->child;

  clear_dial_queue (self);

  g_clear_pointer (&child, gtk_widget_unparent);

  G_OBJECT_CLASS (calls_new_call_box_parent_class)->dispose (object);
}


static void
calls_new_call_box_class_init (CallsNewCallBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = calls_new_call_box_get_property;
  object_class->dispose = calls_new_call_box_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/new-call-box.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, child);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, origin_list_box);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, origin_list);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, dialpad);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, address_entry);
  gtk_widget_class_bind_template_callback (widget_class, address_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, address_changed_cb);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, result);
  gtk_widget_class_bind_template_callback (widget_class, dialpad_dialed_cb);
  gtk_widget_class_bind_template_callback (widget_class, dial_result_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, notify_selected_index_cb);

  props[PROP_NUMERIC_INPUT_ONLY] =
    g_param_spec_boolean ("numeric-input-only",
                          "Numeric input only",
                          "Whether only numeric input is allowed (for the selected origin)",
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
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
  g_autoptr (CallsOrigin) origin = NULL;

  g_return_if_fail (CALLS_IS_NEW_CALL_BOX (self));
  g_return_if_fail (target != NULL);

  origin = get_origin (self, target);
  if (!origin) {
    // Queue for dialing when an origin appears
    g_debug ("Can't submit call with no origin, queuing for later");
    self->dial_queue = g_list_append (self->dial_queue,
                                      g_strdup (target));
    return;
  }

  calls_origin_dial (origin, target);
}

void
calls_new_call_box_send_ussd_async (CallsNewCallBox    *self,
                                    const char         *target,
                                    GCancellable       *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer            user_data)
{
  g_autoptr (CallsOrigin) origin = NULL;
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (CALLS_IS_NEW_CALL_BOX (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (target && *target);

  origin = get_origin (self, target);

  task = g_task_new (self, cancellable, callback, user_data);

  if (!CALLS_IS_USSD (origin)) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "No origin with USSD available");
    return;
  }

  if (!calls_number_is_ussd (target)) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "%s is not a valid USSD code", target);
    return;
  }

  calls_ussd_initiate_async (CALLS_USSD (origin), target, cancellable,
                             ussd_send_cb, g_steal_pointer (&task));
}

char *
calls_new_call_box_send_ussd_finish (CallsNewCallBox *self,
                                     GAsyncResult    *result,
                                     GError         **error)
{
  g_return_val_if_fail (CALLS_IS_NEW_CALL_BOX (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
