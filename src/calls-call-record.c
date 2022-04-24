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

#include "calls-call-record.h"

#include <glib/gi18n.h>


struct _CallsCallRecord {
  GomResource parent_instance;
  guint       id;
  char       *target;
  gboolean    inbound;
  GDateTime  *start;
  GDateTime  *answered;
  GDateTime  *end;
  char       *protocol;
  gboolean    complete;
};

G_DEFINE_TYPE (CallsCallRecord, calls_call_record, GOM_TYPE_RESOURCE)


enum {
  PROP_0,
  PROP_ID,
  PROP_TARGET,
  PROP_INBOUND,
  PROP_START,
  PROP_ANSWERED,
  PROP_END,
  PROP_PROTOCOL,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

enum {
  SIGNAL_CALL_DELETE,
  SIGNAL_LAST_SIGNAL,
};
static guint signals[SIGNAL_LAST_SIGNAL];


static void
get_property (GObject    *object,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
  CallsCallRecord *self = CALLS_CALL_RECORD (object);

  switch (property_id) {

  case PROP_ID:
    g_value_set_uint (value, self->id);
    break;

  case PROP_TARGET:
    g_value_set_string (value, self->target);
    break;

  case PROP_INBOUND:
    g_value_set_boolean (value, self->inbound);
    break;

  case PROP_START:
    g_value_set_boxed (value, self->start);
    break;

  case PROP_ANSWERED:
    g_value_set_boxed (value, self->answered);
    break;

  case PROP_END:
    g_value_set_boxed (value, self->end);
    break;

  case PROP_PROTOCOL:
    g_value_set_string (value, self->protocol);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
set_date_time (GDateTime   **stamp_ptr,
               const GValue *value)
{
  gpointer new_stamp = g_value_get_boxed (value);

  g_clear_pointer (stamp_ptr, g_date_time_unref);

  if (new_stamp)
    *stamp_ptr = g_date_time_ref (new_stamp);
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsCallRecord *self = CALLS_CALL_RECORD (object);

  switch (property_id) {
  case PROP_ID:
    self->id = g_value_get_uint (value);
    break;

  case PROP_TARGET:
    g_free (self->target);
    self->target = g_value_dup_string (value);
    break;

  case PROP_INBOUND:
    self->inbound = g_value_get_boolean (value);
    break;

  case PROP_START:
    set_date_time (&self->start, value);
    break;

  case PROP_ANSWERED:
    set_date_time (&self->answered, value);
    break;

  case PROP_END:
    set_date_time (&self->end, value);
    break;

  case PROP_PROTOCOL:
    g_free (self->protocol);
    self->protocol = g_value_dup_string (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
finalize (GObject *object)
{
  CallsCallRecord *self = CALLS_CALL_RECORD (object);

  g_clear_pointer (&self->end, g_date_time_unref);
  g_clear_pointer (&self->answered, g_date_time_unref);
  g_clear_pointer (&self->start, g_date_time_unref);
  g_free (self->target);
  g_free (self->protocol);

  G_OBJECT_CLASS (calls_call_record_parent_class)->finalize (object);
}


static void
calls_call_record_class_init (CallsCallRecordClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomResourceClass *resource_class = GOM_RESOURCE_CLASS (klass);

  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  signals[SIGNAL_CALL_DELETE] =
    g_signal_new ("call-delete",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0, NULL);

  gom_resource_class_set_table (resource_class, "calls");

  /*
   * NB: ANY ADDITIONS TO THIS LIST REQUIRE AN INCREASE IN
   * RECORD_STORE_VERSION IN calls-record-store.c AND THE USE OF
   *
   *   gom_resource_class_set_property_new_in_version
   *   (resource_class, "property", 2);
   *
   * HERE.
   */

  props[PROP_ID] =
    g_param_spec_uint ("id",
                       "ID",
                       "The row ID",
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_ID, props[PROP_ID]);
  gom_resource_class_set_primary_key (resource_class, "id");

  props[PROP_TARGET] =
    g_param_spec_string ("target",
                         "Target",
                         "The PTSN phone number or other address of the call",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_TARGET, props[PROP_TARGET]);

  props[PROP_INBOUND] =
    g_param_spec_boolean ("inbound",
                          "Inbound",
                          "Whether the call was an inbound call",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_INBOUND, props[PROP_INBOUND]);

  props[PROP_START] =
    g_param_spec_boxed ("start",
                        "Start",
                        "Time stamp of the start of the call",
                        G_TYPE_DATE_TIME,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_START, props[PROP_START]);

  props[PROP_ANSWERED] =
    g_param_spec_boxed ("answered",
                        "Answered",
                        "Time stamp of when the call was answered",
                        G_TYPE_DATE_TIME,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_ANSWERED, props[PROP_ANSWERED]);

  props[PROP_END] =
    g_param_spec_boxed ("end",
                        "End",
                        "Time stamp of the end of the call",
                        G_TYPE_DATE_TIME,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_END, props[PROP_END]);

  props[PROP_PROTOCOL] =
    g_param_spec_string ("protocol",
                         "Protocol",
                         "The URI protocol for this call",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_PROTOCOL, props[PROP_PROTOCOL]);

  gom_resource_class_set_property_new_in_version (resource_class, "protocol", 2);


  /*
   * NB: ANY ADDITIONS TO THIS LIST REQUIRE AN INCREASE IN
   * RECORD_STORE_VERSION IN calls-record-store.c AND THE USE OF
   *
   *   gom_resource_class_set_property_new_in_version
   *   (resource_class, "property", 2);
   *
   * HERE.
   */

}


static void
calls_call_record_init (CallsCallRecord *self)
{
}
