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

#include "calls-record-store.h"
#include "calls-call-record.h"
#include "calls-enumerate.h"
#include "calls-call.h"
#include "config.h"

#include <gom/gom.h>
#include <glib/gi18n.h>

#include <errno.h>


#define RECORD_STORE_FILENAME "records.db"
#define RECORD_STORE_VERSION  1


typedef enum
{
  STARTED,
  ANSWERED,
  ENDED
} CallsCallRecordState;


static CallsCallRecordState
state_to_record_state (CallsCallState call_state)
{
  switch (call_state)
    {
    case CALLS_CALL_STATE_DIALING:
    case CALLS_CALL_STATE_ALERTING:
    case CALLS_CALL_STATE_INCOMING:
    case CALLS_CALL_STATE_WAITING:
      return STARTED;

    case CALLS_CALL_STATE_ACTIVE:
    case CALLS_CALL_STATE_HELD:
      return ANSWERED;

    case CALLS_CALL_STATE_DISCONNECTED:
      return ENDED;
    }

  g_assert_not_reached ();
}


struct _CallsRecordStore
{
  GtkApplicationWindow parent_instance;

  gchar *filename;
  CallsProvider *provider;
  GomAdapter *adapter;
  GomRepository *repository;
};

G_DEFINE_TYPE (CallsRecordStore, calls_record_store, G_TYPE_LIST_STORE);


enum {
  PROP_0,
  PROP_PROVIDER,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


static void
load_calls_fetch_cb (GomResourceGroup *group,
                     GAsyncResult     *res,
                     CallsRecordStore *self)
{
  gboolean ok;
  GError *error = NULL;
  guint count, i;
  gpointer *records;

  ok = gom_resource_group_fetch_finish (group,
                                        res,
                                        &error);
  if (error)
    {
      g_debug ("Error fetching call records: %s",
               error->message);
      g_error_free (error);
      return;
    }
  g_assert (ok);

  count = gom_resource_group_get_count (group);
  g_debug ("Fetched %u call records from database `%s'",
           count, self->filename);
  records = g_new (gpointer, count);

  for (i = 0; i < count; ++i)
    {
      GomResource *resource;
      CallsCallRecord *record;
      GDateTime *end = NULL;

      resource = gom_resource_group_get_index (group, i);
      g_assert (resource != NULL);
      g_assert (CALLS_IS_CALL_RECORD (resource));
      record = CALLS_CALL_RECORD (resource);

      records[i] = record;

      g_object_get (G_OBJECT (record),
                    "end", &end,
                    NULL);
      if (end)
        {
          g_date_time_unref (end);
        }
    }

  g_list_store_splice (G_LIST_STORE (self),
                       0,
                       0,
                       records,
                       count);

  g_free (records);
  g_object_unref (group);
}


static void
load_calls_find_cb (GomRepository    *repository,
                    GAsyncResult     *res,
                    CallsRecordStore *self)
{
  GomResourceGroup *group;
  GError *error = NULL;
  guint count;

  group = gom_repository_find_finish (repository,
                                      res,
                                      &error);
  if (error)
    {
      g_debug ("Error finding call records in database `%s': %s",
               self->filename, error->message);
      g_error_free (error);
      return;
    }
  g_assert (group != NULL);

  count = gom_resource_group_get_count (group);
  if (count == 0)
    {
      g_debug ("No call records found in database `%s'",
               self->filename);
      return;
    }

  g_debug ("Found %u call records in database `%s', fetching",
           count, self->filename);
  gom_resource_group_fetch_async
    (group,
     0,
     count,
     (GAsyncReadyCallback)load_calls_fetch_cb,
     self);
}


static void
load_calls (CallsRecordStore *self)
{
  GomFilter *filter;
  GomSorting *sorting;

  filter = gom_filter_new_is_not_null
    (CALLS_TYPE_CALL_RECORD, "start");

  sorting = gom_sorting_new (CALLS_TYPE_CALL_RECORD,
                             "start",
                             GOM_SORTING_DESCENDING,
                             NULL);

  g_debug ("Finding records in call record database `%s'",
           self->filename);
  gom_repository_find_sorted_async (self->repository,
                                    CALLS_TYPE_CALL_RECORD,
                                    filter,
                                    sorting,
                                    (GAsyncReadyCallback)load_calls_find_cb,
                                    self);

  g_object_unref (G_OBJECT (filter));
}


static void
set_up_repo_migrate_cb (GomRepository *repo,
                        GAsyncResult *res,
                        CallsRecordStore *self)
{
  GError *error = NULL;
  gboolean ok;

  ok = gom_repository_automatic_migrate_finish (repo, res, &error);
  if (!ok)
    {
      if (error)
        {
          g_warning ("Error migrating call record database `%s': %s",
                     self->filename, error->message);
          g_error_free (error);
        }
      else
        {
          g_warning ("Unknown error migrating call record database `%s'",
                     self->filename);
        }

      g_clear_object (&self->repository);
      g_clear_object (&self->adapter);
    }
  else
    {
      g_debug ("Successfully migrated call record database `%s'",
               self->filename);
      load_calls (self);
    }
}


static void
set_up_repo (CallsRecordStore *self)
{
  GomRepository *repo;
  GList *types = NULL;

  if (self->repository)
    {
      g_warning ("Opened call record database `%s'"
                 " while repository exists",
                 self->filename);
      return;
    }

  repo = gom_repository_new (self->adapter);

  g_debug ("Attempting migration of call"
           " record database `%s'",
           self->filename);
  types = g_list_append (types, (gpointer)CALLS_TYPE_CALL_RECORD);
  gom_repository_automatic_migrate_async
    (repo,
     RECORD_STORE_VERSION,
     types,
     (GAsyncReadyCallback)set_up_repo_migrate_cb,
     self);

  self->repository = repo;
}


static void
close_adapter (CallsRecordStore *self)
{
  GError *error = NULL;
  gboolean ok;

  if (!self->adapter)
    {
      return;
    }

  ok = gom_adapter_close_sync(self->adapter, &error);
  if (!ok)
    {
      if (error)
        {
          g_warning ("Error closing call record database `%s': %s",
                     self->filename, error->message);
          g_error_free (error);
        }
      else
        {
          g_warning ("Unknown error closing call record database `%s'",
                     self->filename);
        }
    }

  g_clear_object (&self->adapter);
}


static void
open_repo_adapter_open_cb (GomAdapter *adapter,
                           GAsyncResult *res,
                           CallsRecordStore *self)
{
  GError *error = NULL;
  gboolean ok;

  ok = gom_adapter_open_finish (adapter, res, &error);
  if (!ok)
    {
      if (error)
        {
          g_warning ("Error opening call record database `%s': %s",
                     self->filename, error->message);
          g_error_free (error);
        }
      else
        {
          g_warning ("Unknown error opening call record database `%s'",
                     self->filename);
        }

      close_adapter (self);
    }
  else
    {
      g_debug ("Successfully opened call record database `%s'",
               self->filename);
      set_up_repo (self);
    }
}


static void
open_repo (CallsRecordStore *self)
{
  gchar *dir;
  gint err;
  gchar *uri;

  if (self->adapter)
    {
      return;
    }


  dir = g_path_get_dirname (self->filename);
  err = g_mkdir_with_parents (dir, 0755);
  if (err)
    {
      g_warning ("Could not create Calls data directory `%s': %s",
                 dir, g_strerror (errno));
    }
  g_free (dir);


  uri = g_strdup_printf ("file:%s", self->filename);
  g_debug ("Opening call record database using URI `%s'", uri);
  self->adapter = gom_adapter_new ();
  gom_adapter_open_async
    (self->adapter,
     uri,
     (GAsyncReadyCallback)open_repo_adapter_open_cb,
     self);

  g_free (uri);
}


struct CallsRecordCallData
{
  CallsRecordStore *self;
  CallsCall *call;
};


static void
record_call_save_cb (GomResource                *resource,
                     GAsyncResult               *res,
                     struct CallsRecordCallData *data)
{
  GObject * const call_obj = G_OBJECT (data->call);
  GError *error = NULL;
  gboolean ok;

  ok = gom_resource_save_finish (resource, res, &error);
  if (!ok)
    {
      if (error)
        {
          g_warning ("Error saving call record to database: %s",
                     error->message);
          g_error_free (error);
        }
      else
        {
          g_warning ("Unknown error saving call record to database");
        }

      g_object_set_data (call_obj, "calls-call-record", NULL);
    }
  else
    {
      g_debug ("Successfully saved new call record to database");
      g_list_store_insert (G_LIST_STORE (data->self),
                           0,
                           CALLS_CALL_RECORD (resource));
      g_object_set_data (call_obj, "calls-call-start", NULL);
    }

  g_object_unref (data->call);
  g_object_unref (data->self);
  g_free (data);
}


static void
record_call (CallsRecordStore *self,
             CallsCall        *call)
{
  GObject * const call_obj = G_OBJECT (call);
  GDateTime *start;
  CallsCallRecord *record;
  struct CallsRecordCallData *data;

  g_assert (g_object_get_data (call_obj, "calls-call-record") == NULL);

  start = g_object_get_data (call_obj, "calls-call-start");
  g_assert (start != NULL);

  record = g_object_new (CALLS_TYPE_CALL_RECORD,
                         "repository", self->repository,
                         "target", calls_call_get_number (call),
                         "inbound", calls_call_get_inbound (call),
                         "start", start,
                         NULL);

  g_object_set_data_full (call_obj, "calls-call-record",
                          record, g_object_unref);

  data = g_new (struct CallsRecordCallData, 1);
  g_object_ref (self);
  g_object_ref (call);
  data->self = self;
  data->call = call;

  gom_resource_save_async (GOM_RESOURCE (record),
                           (GAsyncReadyCallback)record_call_save_cb,
                           data);
}


static void
call_added_cb (CallsRecordStore *self,
               CallsCall        *call)
{
  GObject * const call_obj = G_OBJECT (call);
  GDateTime *start;

  g_assert (g_object_get_data (call_obj, "calls-call-start") == NULL);
  start = g_date_time_new_now_local ();
  g_object_set_data_full (call_obj, "calls-call-start",
                          start, (GDestroyNotify)g_date_time_unref);

  if (!self->repository)
    {
      open_repo (self);
      return;
    }

  record_call (self, call);
}


static void
update_cb (GomResource  *resource,
           GAsyncResult *res,
           gpointer     *unused)
{
  GError *error = NULL;
  gboolean ok;

  ok = gom_resource_save_finish (resource, res, &error);
  if (!ok)
    {
      if (error)
        {
          g_warning ("Error updating call record in database: %s",
                     error->message);
          g_error_free (error);
        }
      else
        {
          g_warning ("Unknown error updating call record in database");
        }
    }
  else
    {
      g_debug ("Successfully updated call record in database");
    }
}


static void
stamp_call (CallsCallRecord  *record,
            const gchar      *stamp_name)
{
  GObject *record_obj = G_OBJECT (record);
  GDateTime *stamp = NULL;

  /* Check the call has not already been stamped */
  g_object_get (record_obj,
                stamp_name, &stamp,
                NULL);
  if (stamp)
    {
      return;
    }


  g_debug ("Stamping call `%s'", stamp_name);
  stamp = g_date_time_new_now_local ();
  g_object_set (record_obj,
                stamp_name, stamp,
                NULL);
  g_date_time_unref (stamp);

  gom_resource_save_async (GOM_RESOURCE (record),
                           (GAsyncReadyCallback)update_cb,
                           NULL);
}


static void
call_removed_cb (CallsRecordStore *self,
                 CallsCall        *call,
                 const gchar      *reason)
{
  /* Stamp the call as ended if it hasn't already been done */
  CallsCallRecord *record =
    g_object_get_data (G_OBJECT (call), "calls-call-record");

  if (record)
    {
      stamp_call (record, "end");
    }
}


static void
state_changed_cb (CallsRecordStore *self,
                  CallsCallState    new_state,
                  CallsCallState    old_state,
                  CallsCall        *call)
{
  GObject *call_obj = G_OBJECT (call);
  CallsCallRecord *record =
    g_object_get_data (call_obj, "calls-call-record");
  CallsCallRecordState new_rec_state, old_rec_state;


  /* Check whether the call is recorded */
  if (!record)
    {
      /* Try to record the call again */
      if (g_object_get_data (call_obj, "calls-call-start") != NULL)
        {
          record_call (self, call);
        }
      else
        {
          g_warning ("Record store received state change"
                     " for non-started call");
        }
      return;
    }

  new_rec_state = state_to_record_state (new_state);
  old_rec_state = state_to_record_state (old_state);

  if (new_rec_state == old_rec_state)
    {
      return;
    }

  switch (old_rec_state)
    {
    case STARTED:
      switch (new_rec_state)
        {
        case ANSWERED:
          stamp_call (record, "answered");
          break;
        case ENDED:
          stamp_call (record, "end");
          break;
        default:
          g_assert_not_reached ();
          break;
        }
      break;

    case ANSWERED:
      switch (new_rec_state)
        {
        case ENDED:
          stamp_call (record, "end");
          break;
        default:
          g_assert_not_reached ();
          break;
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsRecordStore *self = CALLS_RECORD_STORE (object);

  switch (property_id) {
  case PROP_PROVIDER:
    g_set_object (&self->provider, CALLS_PROVIDER (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
set_up_provider (CallsRecordStore *self)
{
  CallsEnumerateParams *params;

  params = calls_enumerate_params_new (self);

  calls_enumerate_params_add
    (params, CALLS_TYPE_ORIGIN, "call-added", G_CALLBACK (call_added_cb));
  calls_enumerate_params_add
    (params, CALLS_TYPE_ORIGIN, "call-removed", G_CALLBACK (call_removed_cb));

  calls_enumerate_params_add
    (params, CALLS_TYPE_CALL, "state-changed", G_CALLBACK (state_changed_cb));

  calls_enumerate (self->provider, params);

  g_object_unref (params);
}


static void
constructed (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsRecordStore *self = CALLS_RECORD_STORE (object);

  open_repo (self);
  set_up_provider (self);

  parent_class->constructed (object);
}


static void
dispose (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsRecordStore *self = CALLS_RECORD_STORE (object);

  g_clear_object (&self->provider);

  g_list_store_remove_all (G_LIST_STORE (self));

  g_clear_object (&self->repository);
  close_adapter (self);

  parent_class->dispose (object);
}


static void
finalize (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsRecordStore *self = CALLS_RECORD_STORE (object);

  g_free (self->filename);

  parent_class->finalize (object);
}


static void
calls_record_store_class_init (CallsRecordStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  props[PROP_PROVIDER] =
    g_param_spec_object ("provider",
                         _("Provider"),
                         _("An object implementing low-level call-making functionality"),
                         CALLS_TYPE_PROVIDER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_record_store_init (CallsRecordStore *self)
{
  self->filename = g_build_filename (g_get_user_data_dir (),
                                     APP_DATA_NAME,
                                     RECORD_STORE_FILENAME,
                                     NULL);
}


CallsRecordStore *
calls_record_store_new (CallsProvider *provider)
{
  return g_object_new (CALLS_TYPE_RECORD_STORE,
                       "item-type", CALLS_TYPE_CALL_RECORD,
                       "provider", provider,
                       NULL);
}
