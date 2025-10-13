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

#define G_LOG_DOMAIN "CallsRecordStore"

#include "calls-config.h"

#include "calls-call-record.h"
#include "calls-manager.h"
#include "calls-ui-call-data.h"
#include "calls-record-store.h"

#include <gom/gom.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <errno.h>


#define RECORD_STORE_FILENAME "records.db"
#define RECORD_STORE_VERSION  2

enum {
  SIGNAL_DB_DONE,
  N_SIGNALS
};
static guint signals[N_SIGNALS];

typedef enum {
  STARTED,
  ANSWERED,
  ENDED
} CallsCallRecordState;


static CallsCallRecordState
state_to_record_state (CuiCallState call_state)
{
  switch (call_state) {
  case CUI_CALL_STATE_CALLING:
  case CUI_CALL_STATE_INCOMING:
    return STARTED;

  case CUI_CALL_STATE_ACTIVE:
  case CUI_CALL_STATE_HELD:
    return ANSWERED;

  case CUI_CALL_STATE_DISCONNECTED:
    return ENDED;

  default:
    g_assert_not_reached ();
  }
}


static const char *
record_state_to_string (CallsCallRecordState state)
{
  switch (state) {
  case STARTED:
    return "started";
  case ANSWERED:
    return "answered";
  case ENDED:
    return "ended";
  default:
    return "unknown";
  }
}


struct _CallsRecordStore {
  GObject              parent_instance;

  gchar               *filename;
  GomAdapter          *adapter;
  GomRepository       *repository;
  int                  busy;

  GListStore          *list_store;
};

G_DEFINE_TYPE (CallsRecordStore, calls_record_store, G_TYPE_OBJECT);


static void
delete_record_cb (GomResource      *resource,
                  GAsyncResult     *res,
                  CallsRecordStore *self)
{
  g_autoptr (GError) error = NULL;
  gboolean ok;
  guint id;

  ok = gom_resource_delete_finish (resource,
                                   res,
                                   &error);

  g_object_get (G_OBJECT (resource),
                "id",
                &id,
                NULL);

  if (!ok) {
    if (error) {
      g_warning ("Error deleting call record with id %u from database %s",
                 id, error->message);
      return;

    } else {
      g_warning ("Unknown error deleting call record with id %u from database",
                 id);
    }

  } else {
    g_debug ("Successfully deleted call record with id %u from database",
             id);
  }
}


static void
delete_call_cb (CallsCallRecord  *record,
                CallsRecordStore *self)
{
  gom_resource_delete_async (GOM_RESOURCE (record),
                             (GAsyncReadyCallback) delete_record_cb,
                             self);
}


static void
load_calls_fetch_cb (GomResourceGroup *group,
                     GAsyncResult     *res,
                     CallsRecordStore *self)
{
  g_autoptr (GError) error = NULL;
  gboolean ok;
  guint count, i;
  gpointer *records;

  ok = gom_resource_group_fetch_finish (group,
                                        res,
                                        &error);
  if (error) {
    g_debug ("Error fetching call records: %s",
             error->message);
    goto exit;
    return;
  }
  g_assert (ok);

  count = gom_resource_group_get_count (group);
  g_debug ("Fetched %u call records from database `%s'",
           count, self->filename);
  records = g_new (gpointer, count);

  for (i = 0; i < count; ++i) {
    GomResource *resource;
    CallsCallRecord *record;

    resource = gom_resource_group_get_index (group, i);
    g_assert (resource != NULL);
    g_assert (CALLS_IS_CALL_RECORD (resource));
    record = CALLS_CALL_RECORD (resource);

    records[i] = record;

    g_signal_connect (record,
                      "call-delete",
                      G_CALLBACK (delete_call_cb),
                      self);
  }

  g_list_store_splice (G_LIST_STORE (self->list_store),
                       0,
                       0,
                       records,
                       count);

  g_free (records);
  g_object_unref (group);
exit:
  self->busy--;
  g_object_unref (self);
}


static void
load_calls_find_cb (GomRepository    *repository,
                    GAsyncResult     *res,
                    CallsRecordStore *self)
{
  g_autoptr (GError) error = NULL;
  GomResourceGroup *group;
  guint count;

  group = gom_repository_find_finish (repository,
                                      res,
                                      &error);
  if (error) {
    g_debug ("Error finding call records in database `%s': %s",
             self->filename, error->message);
    goto exit;
  }
  g_assert (group != NULL);

  count = gom_resource_group_get_count (group);
  if (count == 0) {
    g_debug ("No call records found in database `%s'",
             self->filename);
    goto exit;
  }

  g_debug ("Found %u call records in database `%s', fetching",
           count, self->filename);
  self->busy++;
  gom_resource_group_fetch_async (group,
                                  0,
                                  count,
                                  (GAsyncReadyCallback) load_calls_fetch_cb,
                                  g_object_ref (self));
exit:
  self->busy--;
  g_object_unref (self);
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
  self->busy++;
  gom_repository_find_sorted_async (self->repository,
                                    CALLS_TYPE_CALL_RECORD,
                                    filter,
                                    sorting,
                                    (GAsyncReadyCallback) load_calls_find_cb,
                                    g_object_ref (self));

  g_object_unref (G_OBJECT (filter));
}


static void
set_up_repo_migrate_cb (GomRepository    *repo,
                        GAsyncResult     *res,
                        CallsRecordStore *self)
{
  g_autoptr (GError) error = NULL;
  gboolean ok;

  ok = gom_repository_automatic_migrate_finish (repo, res, &error);
  if (!ok) {
    if (error)
      g_warning ("Error migrating call record database `%s': %s",
                 self->filename, error->message);
    else
      g_warning ("Unknown error migrating call record database `%s'",
                 self->filename);

    g_clear_object (&self->repository);
    g_clear_object (&self->adapter);
  } else {
    g_debug ("Successfully migrated call record database `%s'",
             self->filename);
    load_calls (self);
  }

  g_signal_emit (self, signals[SIGNAL_DB_DONE], 0, ok);
  self->busy--;
  g_object_unref (self);
}


static void
set_up_repo (CallsRecordStore *self)
{
  GomRepository *repo;
  GList *types = NULL;

  if (self->repository) {
    g_warning ("Opened call record database `%s'"
               " while repository exists",
               self->filename);
    return;
  }

  repo = gom_repository_new (self->adapter);

  g_debug ("Attempting migration of call"
           " record database `%s'",
           self->filename);
  types = g_list_append (types, (gpointer) CALLS_TYPE_CALL_RECORD);
  self->busy++;
  gom_repository_automatic_migrate_async
    (repo,
    RECORD_STORE_VERSION,
    types,
    (GAsyncReadyCallback) set_up_repo_migrate_cb,
    g_object_ref (self));

  self->repository = repo;
}


static void
close_adapter (CallsRecordStore *self)
{
  g_autoptr (GError) error = NULL;
  gboolean ok;

  if (!self->adapter) {
    return;
  }

  ok = gom_adapter_close_sync (self->adapter, &error);
  if (!ok) {
    if (error)
      g_warning ("Error closing call record database `%s': %s",
                 self->filename, error->message);
    else
      g_warning ("Unknown error closing call record database `%s'",
                 self->filename);
  }

  g_clear_object (&self->adapter);
}


static void
open_repo_adapter_open_cb (GomAdapter       *adapter,
                           GAsyncResult     *res,
                           CallsRecordStore *self)
{
  g_autoptr (GError) error = NULL;
  gboolean ok;

  ok = gom_adapter_open_finish (adapter, res, &error);
  if (!ok) {
    if (error)
      g_warning ("Error opening call record database `%s': %s",
                 self->filename, error->message);
    else
      g_warning ("Unknown error opening call record database `%s'",
                 self->filename);

    close_adapter (self);
    g_signal_emit (self, signals[SIGNAL_DB_DONE], 0, FALSE);
  } else {
    g_debug ("Successfully opened call record database `%s'",
             self->filename);
    set_up_repo (self);
  }

  self->busy--;
  g_object_unref (self);
}


static void
open_repo (CallsRecordStore *self)
{
  gchar *dir;
  gint err;
  gchar *uri;

  if (self->adapter)
    return;


  dir = g_path_get_dirname (self->filename);
  err = g_mkdir_with_parents (dir, 0755);
  if (err)
    g_warning ("Could not create Calls data directory `%s': %s",
               dir, g_strerror (errno));
  g_free (dir);


  uri = g_strdup_printf ("file:%s", self->filename);
  g_debug ("Opening call record database using URI `%s'", uri);
  self->adapter = gom_adapter_new ();
  self->busy++;
  gom_adapter_open_async
    (self->adapter,
    uri,
    (GAsyncReadyCallback) open_repo_adapter_open_cb,
    g_object_ref (self));

  g_free (uri);
}


struct CallsRecordCallData {
  CallsRecordStore *self;
  CallsUiCallData  *call;
};


static void
record_call_save_cb (GomResource                *resource,
                     GAsyncResult               *res,
                     struct CallsRecordCallData *data)
{
  GObject * const call_obj = G_OBJECT (data->call);

  g_autoptr (GError) error = NULL;
  gboolean ok;

  ok = gom_resource_save_finish (resource, res, &error);
  if (!ok) {
    if (error)
      g_warning ("Error saving call record to database: %s",
                 error->message);
    else
      g_warning ("Unknown error saving call record to database");

    g_object_set_data (call_obj, "calls-call-record", NULL);

  } else {
    g_debug ("Successfully saved new call record to database");
    g_list_store_insert (G_LIST_STORE (data->self->list_store),
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
             CallsUiCallData  *call)
{
  GObject * const call_obj = G_OBJECT (call);
  GDateTime *start;
  CallsCallRecord *record;
  struct CallsRecordCallData *data;
  gboolean inbound;
  g_autofree char *protocol = NULL;

  g_assert (g_object_get_data (call_obj, "calls-call-record") == NULL);

  start = g_object_get_data (call_obj, "calls-call-start");
  g_assert (start != NULL);

  g_object_get (call, "inbound", &inbound, "protocol", &protocol, NULL);

  record = g_object_new (CALLS_TYPE_CALL_RECORD,
                         "repository", self->repository,
                         "target", cui_call_get_id (CUI_CALL (call)),
                         "inbound", inbound,
                         "protocol", protocol,
                         "start", start,
                         NULL);

  g_object_set_data_full (call_obj, "calls-call-record",
                          record, g_object_unref);

  data = g_new (struct CallsRecordCallData, 1);
  data->self = g_object_ref (self);
  data->call = g_object_ref (call);

  gom_resource_save_async (GOM_RESOURCE (record),
                           (GAsyncReadyCallback) record_call_save_cb,
                           data);
}


static void
update_cb (GomResource  *resource,
           GAsyncResult *res,
           gpointer     *unused)
{
  g_autoptr (GError) error = NULL;
  gboolean ok;

  ok = gom_resource_save_finish (resource, res, &error);
  if (!ok) {
    if (error)
      g_warning ("Error updating call record in database: %s",
                 error->message);
    else
      g_warning ("Unknown error updating call record in database");

  } else {
    g_debug ("Successfully updated call record in database");
  }
}


static void
stamp_call (CallsCallRecord *record,
            const char      *stamp_name)
{
  GDateTime *stamp = NULL;

  /* Check the call has not already been stamped */
  g_object_get (record,
                stamp_name, &stamp,
                NULL);
  if (stamp)
    return;


  g_debug ("Stamping call `%s'", stamp_name);
  stamp = g_date_time_new_now_utc ();
  g_object_set (record,
                stamp_name, stamp,
                NULL);
  g_date_time_unref (stamp);

  gom_resource_save_async (GOM_RESOURCE (record),
                           (GAsyncReadyCallback) update_cb,
                           NULL);
}


static void
state_changed_cb (CallsRecordStore *self,
                  CuiCallState      new_state,
                  CuiCallState      old_state,
                  CallsUiCallData  *call)
{
  GObject *call_obj = G_OBJECT (call);
  CallsCallRecord *record =
    g_object_get_data (call_obj, "calls-call-record");
  CallsCallRecordState new_rec_state, old_rec_state;

  g_debug ("Call state changed from %s (%d) to %s (%d)",
           cui_call_state_to_string (old_state), old_state,
           cui_call_state_to_string (new_state), new_state);

  /* Check whether the call is recorded */
  if (!record) {
    /* Try to record the call again */
    if (g_object_get_data (call_obj, "calls-call-start") != NULL)
      record_call (self, call);
    else
      g_warning ("Record store received state change"
                 " for non-started call");
    return;
  }

  new_rec_state = state_to_record_state (new_state);
  old_rec_state = state_to_record_state (old_state);

  if (new_rec_state == old_rec_state)
    return;

  switch (old_rec_state) {
  case STARTED:
    switch (new_rec_state) {
    case ANSWERED:
      stamp_call (record, "answered");
      break;

    case ENDED:
      stamp_call (record, "end");
      break;

    case STARTED:
    default:
      goto critical_exit;
    }
    break;

  case ANSWERED:
    switch (new_rec_state) {
    case ENDED:
      stamp_call (record, "end");
      break;

    case STARTED:
    case ANSWERED:
    default:
      goto critical_exit;
    }
    break;

  case ENDED:
  default:
    goto critical_exit;
  }

  return;

critical_exit:
  /* XXX Modem's may be buggy; let's print as much information as possible */
  g_critical ("Unexpected state change occurred!\n"
              "From %s (%s) to %s (%s)",
              cui_call_state_to_string (old_state), record_state_to_string (old_rec_state),
              cui_call_state_to_string (new_state), record_state_to_string (new_rec_state));
}


static void
call_added_cb (CallsRecordStore *self,
               CallsUiCallData  *call)
{
  GObject * const call_obj = G_OBJECT (call);
  GDateTime *start;

  g_assert (g_object_get_data (call_obj, "calls-call-start") == NULL);
  start = g_date_time_new_now_utc ();
  g_object_set_data_full (call_obj, "calls-call-start",
                          start, (GDestroyNotify) g_date_time_unref);

  if (!self->repository) {
    open_repo (self);
    return;
  }

  record_call (self, call);

  g_signal_connect_swapped (call,
                            "state-changed",
                            G_CALLBACK (state_changed_cb),
                            self);
}


static void
call_removed_cb (CallsRecordStore *self,
                 CallsUiCallData  *call,
                 const gchar      *reason)
{
  /* Stamp the call as ended if it hasn't already been done */
  CallsCallRecord *record =
    g_object_get_data (G_OBJECT (call), "calls-call-record");

  if (record)
    stamp_call (record, "end");

  g_signal_handlers_disconnect_by_data (call, self);
}


static void
constructed (GObject *object)
{
  g_autoptr (GList) calls = NULL;
  GList *c;
  CallsRecordStore *self = CALLS_RECORD_STORE (object);

  open_repo (self);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "ui-call-added",
                            G_CALLBACK (call_added_cb),
                            self);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "ui-call-removed",
                            G_CALLBACK (call_removed_cb),
                            self);

  calls = calls_manager_get_calls (calls_manager_get_default ());
  for (c = calls; c != NULL; c = c->next) {
    call_added_cb (self, c->data);
  }

  G_OBJECT_CLASS (calls_record_store_parent_class)->constructed (object);
}


static void
dispose (GObject *object)
{
  CallsRecordStore *self = CALLS_RECORD_STORE (object);

  g_clear_object (&self->repository);

  G_OBJECT_CLASS (calls_record_store_parent_class)->dispose (object);
}


static void
finalize (GObject *object)
{
  CallsRecordStore *self = CALLS_RECORD_STORE (object);

  g_free (self->filename);
  close_adapter (self);

  G_OBJECT_CLASS (calls_record_store_parent_class)->finalize (object);
}


static void
calls_record_store_class_init (CallsRecordStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  signals[SIGNAL_DB_DONE] = g_signal_new ("db-done",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                          NULL,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_BOOLEAN);
}


static void
calls_record_store_init (CallsRecordStore *self)
{
  g_autofree char *old_dir = g_build_filename (g_get_user_data_dir (),
                                               "calls",
                                               NULL);
  g_autofree char *new_dir = g_build_filename (g_get_user_data_dir (),
                                               APP_DATA_NAME,
                                               NULL);
  const char *env_dir = g_getenv ("CALLS_RECORD_DIR");
  char *used_dir = NULL;
  gboolean exist_old;
  gboolean exist_new;
  gboolean new_is_dir;

  exist_old = g_file_test (old_dir, G_FILE_TEST_EXISTS);
  exist_new = g_file_test (new_dir, G_FILE_TEST_EXISTS);
  new_is_dir = g_file_test (new_dir, G_FILE_TEST_IS_DIR);

  if (env_dir) {
    used_dir = (char *) env_dir;
  } else if (exist_old && !exist_new) {
    g_debug ("Trying to move database from `%s' to `%s'", old_dir, new_dir);

    if (g_rename (old_dir, new_dir) == 0) {
      used_dir = new_dir;
    } else {
      g_warning ("Moving folders to new location failed!");
      g_debug ("Continuing to use old location");
      used_dir = old_dir;
    }
  } else if (exist_new && new_is_dir) {
    used_dir = new_dir;
  } else {
    used_dir = old_dir;
  }

  g_assert (used_dir);

  self->filename = g_build_filename (used_dir,
                                     RECORD_STORE_FILENAME,
                                     NULL);

  self->list_store = g_list_store_new (CALLS_TYPE_CALL_RECORD);
}


CallsRecordStore *
calls_record_store_new (void)
{
  return g_object_new (CALLS_TYPE_RECORD_STORE, NULL);
}


GListModel *
calls_record_store_get_list_model (CallsRecordStore *self)
{
  return G_LIST_MODEL (self->list_store);
}

/**
 * calls_record_store_is_busy:
 * @self: The record store
 *
 * Check whether there are async database operations in flight. It
 * is only save to dispose the record store when this function returns
 * `FALSE`.
 *
 * Returns: `TRUE` when there are async db operations, otherwise
 *   `FALSE`.
 */
gboolean
calls_record_store_is_busy (CallsRecordStore *self)
{
  g_assert (CALLS_IS_RECORD_STORE (self));
  g_assert (self->busy >= 0);

  return !!self->busy;
}
