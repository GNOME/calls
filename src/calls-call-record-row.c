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

#include "calls-call-record-row.h"
#include "calls-best-match.h"
#include "calls-contacts-provider.h"
#include "calls-manager.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib.h>
#include <handy.h>

#include <sys/time.h>
#include <errno.h>


struct _CallsCallRecordRow
{
  GtkListBoxRow parent_instance;

  GtkWidget *avatar;
  GtkImage *type;
  GtkLabel *target;
  GtkLabel *time;
  GtkButton *button;
  GtkPopover *popover;
  GtkGesture *gesture;
  GtkEventBox *event_box;

  GMenu *context_menu;

  GActionMap *action_map;

  CallsCallRecord *record;
  gulong answered_notify_handler_id;
  gulong end_notify_handler_id;
  guint date_change_timeout;

  CallsBestMatch *contact;
};

G_DEFINE_TYPE (CallsCallRecordRow, calls_call_record_row, GTK_TYPE_LIST_BOX_ROW)


enum {
  PROP_0,
  PROP_RECORD,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
nice_time (GDateTime *t,
           gchar **nice,
           gboolean *final)
{
  GDateTime *now = g_date_time_new_now_local ();
  g_autoptr (GTimeZone) local_tz = g_time_zone_new_local ();
  g_autoptr (GDateTime) t_local_tz = g_date_time_to_timezone (t, local_tz);
  const gboolean today =
    calls_date_time_is_same_day (now, t_local_tz);
  const gboolean yesterday =
    (!today && calls_date_time_is_yesterday (now, t_local_tz));

  g_assert (nice != NULL);
  g_assert (final != NULL);

  if (today || yesterday) {
    gchar *n = g_date_time_format (t_local_tz, "%R");

    if (yesterday) {
      gchar *s;
      s = g_strdup_printf (_("%s\nyesterday"), n);
      g_free (n);
      n = s;
    }

    *nice = n;
    *final = FALSE;
  } else if (calls_date_time_is_same_year (now, t)) {
    *nice = g_date_time_format (t_local_tz, "%b %-d");
    *final = FALSE;
  } else {
    *nice = g_date_time_format (t_local_tz, "%Y");
    *final = TRUE;
  }

  g_date_time_unref (now);
}


static void
update_time_text (CallsCallRecordRow *self,
                  GDateTime          *end,
                  gboolean           *final)
{
  gchar *nice;
  nice_time (end, &nice, final);
  gtk_label_set_text (self->time, nice);
  g_free (nice);
}


static gboolean date_change_cb (CallsCallRecordRow *self);


static void
setup_date_change_timeout (CallsCallRecordRow *self)
{
  GDateTime *gnow, *gnextday, *gtomorrow;
  struct timeval now, tomorrow, delta;
  int err;
  guint interval;

  // Get the time now
  gnow = g_date_time_new_now_local ();

  // Get the next day
  gnextday = g_date_time_add_days (gnow, 1);
  g_date_time_unref (gnow);

  // Get the start of the next day
  gtomorrow =
    g_date_time_new (g_date_time_get_timezone (gnextday),
                     g_date_time_get_year (gnextday),
                     g_date_time_get_month (gnextday),
                     g_date_time_get_day_of_month (gnextday),
                     0,
                     0,
                     0.0);
  g_date_time_unref (gnextday);

  // Convert to a timeval
  tomorrow.tv_sec  = g_date_time_to_unix (gtomorrow);
  tomorrow.tv_usec = 0;
  g_date_time_unref (gtomorrow);

  // Get the precise time now
  err = gettimeofday (&now, NULL);
  if (err == -1)
    {
      g_warning ("Error getting time to set date change timeout: %s",
                 g_strerror (errno));
      return;
    }

  // Find how long from now until the start of the next day
  timersub (&tomorrow, &now, &delta);

  // Convert to milliseconds
  interval =
    (delta.tv_sec  * 1000)
    +
    (delta.tv_usec / 1000);

  // Add the timeout
  self->date_change_timeout =
    g_timeout_add (interval,
                   (GSourceFunc)date_change_cb,
                   self);
}


static gboolean
date_change_cb (CallsCallRecordRow *self)
{
  GDateTime *end;
  gboolean final;

  g_object_get (G_OBJECT (self->record),
                "end", &end,
                NULL);
  g_assert (end != NULL);

  update_time_text (self, end, &final);
  g_date_time_unref (end);

  if (final)
    self->date_change_timeout = 0;
  else
    setup_date_change_timeout (self);

  return FALSE;
}


static void
update_time (CallsCallRecordRow *self,
             gboolean            inbound,
             GDateTime          *answered,
             GDateTime          *end)
{
  if (end) {
    gboolean time_final;

    update_time_text (self, end, &time_final);

    if (!time_final && !self->date_change_timeout)
      setup_date_change_timeout (self);
  }

  gtk_image_set_from_icon_name (self->type,
                                get_call_icon_symbolic_name (inbound, !answered),
                                GTK_ICON_SIZE_MENU);
}


static void
notify_time_cb (CallsCallRecordRow *self,
                GParamSpec         *pspec,
                CallsCallRecord    *record)
{
  gboolean inbound;
  GDateTime *answered;
  GDateTime *end;

  g_object_get (G_OBJECT (self->record),
                "inbound", &inbound,
                "answered", &answered,
                "end", &end,
                NULL);

  update_time (self, inbound, answered, end);

  if (answered) {
    g_date_time_unref (answered);
    calls_clear_signal (record, &self->answered_notify_handler_id);
  }
  if (end) {
    g_date_time_unref (end);
    calls_clear_signal (record, &self->end_notify_handler_id);
  }
}


static void
setup_time (CallsCallRecordRow *self,
            gboolean            inbound,
            GDateTime          *answered,
            GDateTime          *end)
{
  if (!end) {
    self->end_notify_handler_id =
      g_signal_connect_swapped (self->record,
                                "notify::end",
                                G_CALLBACK (notify_time_cb),
                                self);

    if (!answered) {
      self->answered_notify_handler_id =
        g_signal_connect_swapped (self->record,
                                  "notify::answered",
                                  G_CALLBACK (notify_time_cb),
                                  self);
    }
  }

  update_time (self, inbound, answered, end);
}


static void
on_notify_can_add_contacts (CallsCallRecordRow *self)
{
  CallsContactsProvider *contacts_provider;
  GAction *action_new_contact = g_action_map_lookup_action (self->action_map, "new-contact");

  contacts_provider = calls_manager_get_contacts_provider (calls_manager_get_default ());

  if (!calls_contacts_provider_get_can_add_contacts (contacts_provider))
    return;

  g_object_bind_property (self->contact, "has-individual",
                          action_new_contact, "enabled",
                          G_BINDING_SYNC_CREATE |
                          G_BINDING_INVERT_BOOLEAN);
  g_signal_handlers_disconnect_by_data(contacts_provider, self);
}


static void
setup_contact (CallsCallRecordRow *self)
{
  GAction *action_copy = g_action_map_lookup_action (self->action_map, "copy-number");
  GAction *action_new_contact = g_action_map_lookup_action (self->action_map, "new-contact");
  g_autofree gchar *target = NULL;
  CallsContactsProvider *contacts_provider;

  contacts_provider = calls_manager_get_contacts_provider (calls_manager_get_default ());

  if (calls_contacts_provider_get_can_add_contacts (contacts_provider)) {
    on_notify_can_add_contacts (self);
  } else {
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action_new_contact), FALSE);
    g_signal_connect_swapped (contacts_provider,
                              "notify::can-add-contacts",
                              G_CALLBACK (on_notify_can_add_contacts),
                              self);
  }



  // Get the target number
  g_object_get (G_OBJECT (self->record),
                "target", &target,
                NULL);

  if (!target || target[0] == '\0') {
    gtk_actionable_set_action_name (GTK_ACTIONABLE (self->button), NULL);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action_copy), FALSE);
  } else {
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action_copy), TRUE);
  }

  // Look up the best match object
  self->contact = calls_contacts_provider_lookup_id (contacts_provider, target);

  if (!self->contact) {
    gtk_label_set_text (self->target, calls_best_match_get_primary_info (NULL));
    return;
  }

  g_object_bind_property (self->contact, "primary-info",
                          self->target, "label",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (self->contact, "has-individual",
                          self->avatar, "show-initials",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (self->contact, "avatar",
                          self->avatar, "loadable-icon",
                          G_BINDING_SYNC_CREATE);

}


static void
context_menu (GtkWidget *widget,
              GdkEvent  *event)
{
  CallsCallRecordRow *self;

  g_assert (CALLS_IS_CALL_RECORD_ROW (widget));

  self = CALLS_CALL_RECORD_ROW (widget);

  if (!self->popover) {
    self->popover = GTK_POPOVER (gtk_popover_new (widget));
    gtk_popover_bind_model (self->popover,
                            G_MENU_MODEL (self->context_menu),
                            "row-history");
    }

  gtk_popover_popup (self->popover);
}


static gboolean
calls_call_record_row_popup_menu (GtkWidget *self)
{
  context_menu (self, NULL);
  return TRUE;
}


static void
long_pressed (GtkGestureLongPress *gesture,
              gdouble              x,
              gdouble              y,
              GtkWidget           *self)
{
  context_menu (self, NULL);
}


static gboolean
calls_call_record_row_button_press_event (GtkWidget      *self,
                                          GdkEventButton *event)
{
  if (gdk_event_triggers_context_menu ((GdkEvent *) event)) {
    context_menu (self, (GdkEvent *) event);
    return TRUE;
  }

  return GTK_WIDGET_CLASS (calls_call_record_row_parent_class)->button_press_event (self, event);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsCallRecordRow *self = CALLS_CALL_RECORD_ROW (object);

  switch (property_id) {
  case PROP_RECORD:
    g_set_object (&self->record,
                  CALLS_CALL_RECORD (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
constructed (GObject *object)
{
  CallsCallRecordRow *self = CALLS_CALL_RECORD_ROW (object);
  gboolean inbound;
  GDateTime *answered;
  GDateTime *end;
  g_autofree char *target_name = NULL;
  g_autofree char *protocol = NULL;
  g_autofree char *action_name = NULL;
  g_autofree char *target = NULL;

  g_object_get (self->record,
                "inbound", &inbound,
                "answered", &answered,
                "end", &end,
                "protocol", &protocol,
                "target", &target,
                NULL);

  /* Fall back to "app.dial-tel" action if no protocol was given */
  if (protocol)
    action_name = g_strdup_printf ("app.dial-%s", protocol);
  else
    action_name = g_strdup ("app.dial-tel");

  gtk_actionable_set_action_name (GTK_ACTIONABLE (self->button), action_name);

  /* TODO add origin ID to action target */
  if (target && *target)
    gtk_actionable_set_action_target (GTK_ACTIONABLE (self->button),
                                      "(ss)", target, "");
  else
    ;

  setup_time (self, inbound, answered, end);
  calls_date_time_unref (answered);
  calls_date_time_unref (end);

  setup_contact (self);

  G_OBJECT_CLASS (calls_call_record_row_parent_class)->constructed (object);
}


static void
get_property (GObject      *object,
              guint         property_id,
              GValue       *value,
              GParamSpec   *pspec)
{
  CallsCallRecordRow *self = CALLS_CALL_RECORD_ROW (object);

  switch (property_id) {
  case PROP_RECORD:
    g_value_set_object (value, self->record);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
dispose (GObject *object)
{
  CallsCallRecordRow *self = CALLS_CALL_RECORD_ROW (object);

  g_clear_object (&self->contact);
  g_clear_object (&self->action_map);
  g_clear_object (&self->gesture);

  calls_clear_source (&self->date_change_timeout);
  calls_clear_signal (self->record, &self->answered_notify_handler_id);
  calls_clear_signal (self->record, &self->end_notify_handler_id);
  g_clear_object (&self->record);

  G_OBJECT_CLASS (calls_call_record_row_parent_class)->dispose (object);
}


static void
calls_call_record_row_class_init (CallsCallRecordRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->get_property = get_property;
  object_class->dispose = dispose;

  widget_class->popup_menu = calls_call_record_row_popup_menu;
  widget_class->button_press_event = calls_call_record_row_button_press_event;

  props[PROP_RECORD] =
    g_param_spec_object ("record",
                         "Record",
                         "The call record for this row",
                         CALLS_TYPE_CALL_RECORD,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/call-record-row.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, avatar);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, type);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, target);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, time);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, button);

  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, event_box);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, context_menu);
}


static void
delete_call_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       data)
{
  GtkWidget *self = GTK_WIDGET (data);
  g_signal_emit_by_name (CALLS_CALL_RECORD_ROW (self)->record, "call-delete");
}


static void
copy_number_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       data)
{
  CallsCallRecordRow *self = CALLS_CALL_RECORD_ROW (data);
  g_autofree gchar *target = NULL;

  g_object_get (G_OBJECT (self->record),
                "target", &target,
                NULL);

  g_return_if_fail (target);

  g_action_group_activate_action (G_ACTION_GROUP (g_application_get_default ()),
                                  "copy-number",
                                  g_variant_new_string (target));
}


static void
new_contact_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       data)
{
  CallsCallRecordRow *self = CALLS_CALL_RECORD_ROW (data);
  CallsContactsProvider *contacts_provider;

  contacts_provider = calls_manager_get_contacts_provider (calls_manager_get_default ());

  calls_contacts_provider_add_new_contact (contacts_provider,
                                           calls_best_match_get_phone_number (self->contact));
}


static GActionEntry entries[] =
{
 { "delete-call", delete_call_activated, NULL, NULL, NULL},
 { "copy-number", copy_number_activated, NULL, NULL, NULL},
 { "new-contact", new_contact_activated, NULL, NULL, NULL},
};


static void
calls_call_record_row_init (CallsCallRecordRow *self)
{
  GAction *act;
  gtk_widget_init_template (GTK_WIDGET (self));

  self->action_map = G_ACTION_MAP (g_simple_action_group_new ());
  g_action_map_add_action_entries (self->action_map,
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "row-history",
                                  G_ACTION_GROUP (self->action_map));

  act = g_action_map_lookup_action (self->action_map, "delete-call");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (act), TRUE);

  self->gesture = gtk_gesture_long_press_new (GTK_WIDGET (self->event_box));
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->gesture), TRUE);
  g_signal_connect (self->gesture, "pressed", G_CALLBACK (long_pressed), self);
}


CallsCallRecordRow *
calls_call_record_row_new (CallsCallRecord *record)
{
  return g_object_new (CALLS_TYPE_CALL_RECORD_ROW,
                       "record", record,
                       NULL);
}


CallsCallRecord *
calls_call_record_row_get_record (CallsCallRecordRow *self)
{
  return self->record;
}
