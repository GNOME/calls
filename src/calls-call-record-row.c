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
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib.h>

#include <sys/time.h>
#include <errno.h>


struct _CallsCallRecordRow
{
  GtkOverlay parent_instance;

  GtkImage *avatar;
  GtkImage *type;
  GtkLabel *target;
  GtkLabel *time;

  CallsCallRecord *record;
  gulong answered_notify_handler_id;
  gulong end_notify_handler_id;
  guint date_change_timeout;

  CallsNewCallBox *new_call;
};

G_DEFINE_TYPE (CallsCallRecordRow, calls_call_record_row, GTK_TYPE_BOX);


enum {
  PROP_0,
  PROP_RECORD,
  PROP_NEW_CALL,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
redial_clicked_cb (CallsCallRecordRow *self)
{
  gchar *target;

  g_object_get (self->record,
                "target", &target,
                NULL);
  g_assert (target != NULL);

  calls_new_call_box_dial (self->new_call, target);
  g_free (target);
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
update_time (CallsCallRecordRow *self,
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

  update_time (self, end, &final);
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
update (CallsCallRecordRow *self,
        gboolean            inbound,
        GDateTime          *answered,
        GDateTime          *end)
{
  gboolean missed = FALSE;
  gchar *type_icon_name;

  if (end)
    {
      gboolean time_final;

      update_time (self, end, &time_final);

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
notify_cb (CallsCallRecordRow *self,
           GParamSpec *pspec,
           CallsCallRecord *record)
{
  gboolean inbound;
  GDateTime *answered;
  GDateTime *end;

  g_object_get (G_OBJECT (self->record),
                "inbound", &inbound,
                "answered", &answered,
                "end", &end,
                NULL);

  update (self, inbound, answered, end);

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

  case PROP_NEW_CALL:
    g_set_object (&self->new_call,
                  CALLS_NEW_CALL_BOX (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
constructed (GObject *object)
{
  GObjectClass *obj_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsCallRecordRow *self = CALLS_CALL_RECORD_ROW (object);
  gchar *target;
  gboolean inbound;
  GDateTime *answered;
  GDateTime *end;

  g_object_get (G_OBJECT (self->record),
                "target", &target,
                "inbound", &inbound,
                "answered", &answered,
                "end", &end,
                NULL);

  gtk_label_set_text (self->target, target);
  g_free (target);

  if (!end)
    {
      self->end_notify_handler_id =
        g_signal_connect_swapped (self->record,
                                  "notify::end",
                                  G_CALLBACK (notify_cb),
                                  self);

      if (!answered)
        {
          self->answered_notify_handler_id =
            g_signal_connect_swapped (self->record,
                                      "notify::answered",
                                      G_CALLBACK (notify_cb),
                                      self);
        }
    }

  update (self, inbound, answered, end);
  calls_date_time_unref (answered);
  calls_date_time_unref (end);

  obj_class->constructed (object);
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
  GObjectClass *obj_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsCallRecordRow *self = CALLS_CALL_RECORD_ROW (object);

  g_clear_object (&self->new_call);

  calls_clear_source (&self->date_change_timeout);
  calls_clear_signal (self->record, &self->answered_notify_handler_id);
  calls_clear_signal (self->record, &self->end_notify_handler_id);
  g_clear_object (&self->record);

  obj_class->dispose (object);
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

  props[PROP_RECORD] =
    g_param_spec_object ("record",
                         _("Record"),
                         _("The call record for this row"),
                         CALLS_TYPE_CALL_RECORD,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  props[PROP_NEW_CALL] =
    g_param_spec_object ("new-call",
                         _("New call"),
                         _("The UI box for making calls"),
                         CALLS_TYPE_NEW_CALL_BOX,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/call-record-row.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, avatar);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, type);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, target);
  gtk_widget_class_bind_template_child (widget_class, CallsCallRecordRow, time);
  gtk_widget_class_bind_template_callback (widget_class, redial_clicked_cb);
}


static void
calls_call_record_row_init (CallsCallRecordRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


CallsCallRecordRow *
calls_call_record_row_new (CallsCallRecord *record,
                           CallsNewCallBox *new_call)
{
  return g_object_new (CALLS_TYPE_CALL_RECORD_ROW,
                       "record", record,
                       "new-call", new_call,
                       NULL);
}


CallsCallRecord *
calls_call_record_row_get_record (CallsCallRecordRow *self)
{
  return self->record;
}
