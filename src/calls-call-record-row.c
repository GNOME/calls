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
#include "calls-contacts.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib.h>
#include <handy.h>

#include <sys/time.h>
#include <errno.h>


#define ANONYMOUS_CALLER _("Anonymous caller")


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


static gboolean
string_to_variant (GBinding *binding,
                   const GValue *from_value,
                   GValue *to_value)
{
  const gchar *target = g_value_get_string (from_value);
  GVariant *variant = g_variant_new_string (target);

  g_value_take_variant (to_value, variant);
  return TRUE;
}


static void
nice_time (GDateTime *t,
           gchar **nice,
           gboolean *final)
{
  GDateTime *now = g_date_time_new_now_local ();
  const gboolean today =
    calls_date_time_is_same_day (now, t);
  const gboolean yesterday =
    (!today && calls_date_time_is_yesterday (now, t));

  g_assert (nice != NULL);
  g_assert (final != NULL);

  if (today || yesterday)
    {
      gchar *n = g_date_time_format (t, "%R");

      if (yesterday)
        {
          gchar *s;
          s = g_strdup_printf (_("%s\nyesterday"), n);
          g_free (n);
          n = s;
        }

      *nice = n;
      *final = FALSE;
    }
  else if (calls_date_time_is_same_year (now, t))
    {
      *nice = g_date_time_format (t, "%b %-d");
      *final = FALSE;
    }
  else
    {
      *nice = g_date_time_format (t, "%Y");
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
    {
      self->date_change_timeout = 0;
    }
  else
    {
      setup_date_change_timeout (self);
    }

  return FALSE;
}


static void
update_time (CallsCallRecordRow *self,
             gboolean            inbound,
             GDateTime          *answered,
             GDateTime          *end)
{
  gboolean missed = FALSE;
  gchar *type_icon_name;

  if (end)
    {
      gboolean time_final;

      update_time_text (self, end, &time_final);

      if (!time_final && !self->date_change_timeout)
        {
          setup_date_change_timeout (self);
        }

      if (!answered)
        {
          missed = TRUE;
        }
    }

  type_icon_name = g_strdup_printf
    ("call-arrow-%s%s-symbolic",
     inbound ? "incoming" : "outgoing",
     missed  ? "-missed"  : "");
  gtk_image_set_from_icon_name (self->type, type_icon_name,
                                GTK_ICON_SIZE_MENU);

  g_free (type_icon_name);
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

  if (answered)
    {
      g_date_time_unref (answered);
      calls_clear_signal (record, &self->answered_notify_handler_id);
    }
  if (end)
    {
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
  if (!end)
    {
      self->end_notify_handler_id =
        g_signal_connect_swapped (self->record,
                                  "notify::end",
                                  G_CALLBACK (notify_time_cb),
                                  self);

      if (!answered)
        {
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
contact_name_cb (CallsCallRecordRow *self)
{
  const gchar *name = NULL;

  if (self->contact)
    {
      name = calls_best_match_get_name (self->contact);
    }

  if (name)
    {
      gtk_label_set_text (self->target, name);
    }
  else
    {
      g_autofree gchar *target = NULL;

      g_object_get (G_OBJECT (self->record),
                    "target", &target,
                    NULL);

      if (!g_strcmp0 (target, ""))
        {
          gtk_label_set_text (self->target, ANONYMOUS_CALLER);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (self->button), NULL);
        }
      else
        {
          gtk_label_set_text (self->target, target);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (self->button), "app.dial");
        }
    }
}

static void
avatar_text_changed_cb (HdyAvatar *avatar)
{
  const gchar *text = hdy_avatar_get_text (avatar);
  gboolean show_initials = TRUE;

  if (strchr("#*+", *text)
      || g_ascii_isdigit (*text)
      || !g_strcmp0 (text, ANONYMOUS_CALLER))
    {
      show_initials = FALSE;
    }

  hdy_avatar_set_show_initials (avatar, show_initials);
}


static void
setup_contact (CallsCallRecordRow *self)
{
  g_autofree gchar *target = NULL;
  g_autoptr(GError) error = NULL;
  EPhoneNumber *phone_number;

  // Get the target number
  g_object_get (G_OBJECT (self->record),
                "target", &target,
                NULL);
  g_assert (target != NULL);

  if (!target[0])
    return;

  // Parse it
  phone_number = e_phone_number_from_string
    (target, NULL, &error);
  if (!phone_number)
    {
      g_warning ("Error parsing phone number `%s': %s",
                 target, error->message);
      return;
    }

  // Look up the best match object
  self->contact = calls_contacts_lookup_phone_number
    (calls_contacts_get_default (), phone_number);
  g_assert (self->contact != NULL);
  g_object_ref (self->contact);
  e_phone_number_free (phone_number);

  g_signal_connect_swapped (self->contact,
                            "notify::name",
                            G_CALLBACK (contact_name_cb),
                            self);
}


static void
context_menu (GtkWidget *self,
              GdkEvent  *event)
{
  gtk_popover_popup (CALLS_CALL_RECORD_ROW (self)->popover);
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
  if (gdk_event_triggers_context_menu ((GdkEvent *) event))
    {
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

  g_object_get (G_OBJECT (self->record),
                "inbound", &inbound,
                "answered", &answered,
                "end", &end,
                NULL);

  g_object_bind_property_full (self->record,
                               "target",
                               self->button,
                               "action-target",
                               G_BINDING_SYNC_CREATE,
                               (GBindingTransformFunc) string_to_variant,
                               NULL,
                               NULL,
                               NULL);

  setup_time (self, inbound, answered, end);
  calls_date_time_unref (answered);
  calls_date_time_unref (end);

  setup_contact (self);
  contact_name_cb (self);

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


  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/call-record-row.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, avatar);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, type);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, target);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, time);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, button);

  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, event_box);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, popover);
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


static GActionEntry entries[] =
{
 { "delete-call", delete_call_activated, NULL, NULL, NULL},
};


static void
calls_call_record_row_init (CallsCallRecordRow *self)
{
  GAction *act;
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->avatar,
                    "notify::text",
                    G_CALLBACK (avatar_text_changed_cb),
                    NULL);

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

  gtk_popover_bind_model (self->popover,
                          G_MENU_MODEL (self->context_menu),
                          "row-history");
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
