/*
 * Copyright (C) 2025 The Phosh.mobi e.V.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "calls-service-providers"

#include "calls-emergency-call-types.h"
#include "calls-service-providers.h"

#include <gio/gio.h>
#include <gmobile.h>

typedef enum {
  PARSER_TOPLEVEL = 0,
  PARSER_COUNTRY,
  PARSER_EMERGENCY_NUMBERS,
  PARSER_EMERGENCY_NUMBER,
  PARSER_CALLEE,
  PARSER_DONE,
  PARSER_ERROR
} GsdParseContextState;


typedef struct {
  GMarkupParseContext           *ctx;
  GHashTable                    *info;
  char buffer[4096];

  char *text_buffer;
  GsdParseContextState           state;
  CallsEmergencyCallCountryData *current_data;
  CallsEmergencyNumber          *current_number;
  CallsEmergencyCallTypeFlags    current_flags;
} CallsParseContext;


typedef struct {
  GAsyncResult *res;
  GMainLoop    *loop;
} GetChannelsSyncData;


static void
calls_parse_context_free (CallsParseContext *parse_context)
{
  g_markup_parse_context_free (parse_context->ctx);
  g_clear_pointer (&parse_context->current_data, calls_emergency_call_country_data_free);
  g_clear_pointer (&parse_context->current_number, calls_emergency_number_free);
  g_clear_pointer (&parse_context->info, g_hash_table_unref);
  g_clear_pointer (&parse_context->text_buffer, g_free);

  g_free (parse_context);
}


static void
parser_toplevel_start (CallsParseContext  *parse_context,
                       const char         *name,
                       const char        **attribute_names,
                       const char        **attribute_values)
{
  if (g_str_equal (name, "serviceproviders")) {
    for (int i = 0; !gm_str_is_null_or_empty (attribute_names[i]); i++) {
      if (g_str_equal (attribute_names[i], "format")) {
        if (!g_str_equal (attribute_values[i], "2.0")) {
          g_warning ("Mobile broadband provider database format '%s' not supported.",
                     attribute_values[i]);
          parse_context->state = PARSER_ERROR;
          break;
        }
      }
    }
  } else if (g_str_equal (name, "country")) {
    parse_context->state = PARSER_COUNTRY;

    if (parse_context->current_data) {
      g_warning ("Country '%s' not fully parsed", parse_context->current_data->country_code);
      g_clear_pointer (&parse_context->current_data, calls_emergency_call_country_data_free);
    }

    for (int i = 0; !gm_str_is_null_or_empty (attribute_names[i]); i++) {
      if (g_str_equal (attribute_names[i], "code")) {
        g_assert (parse_context->current_data == NULL);
        parse_context->current_data = calls_emergency_call_country_data_new (attribute_values[i]);
        break;
      }
    }
  }
}


static void
parser_country_start (CallsParseContext  *parse_context,
                      const char         *name,
                      const char        **attribute_names,
                      const char        **attribute_values)
{
  if (g_str_equal (name, "emergency-numbers"))
    parse_context->state = PARSER_EMERGENCY_NUMBERS;
}


static void
parser_emergency_numbers_start (CallsParseContext *parse_context,
                                const char        *name,
                                const char       **attribute_names,
                                const char       **attribute_values)
{
  if (g_str_equal (name, "emergency-number")) {
    for (int i = 0; !gm_str_is_null_or_empty (attribute_names[i]); i++) {
      if (g_str_equal (attribute_names[i], "number")) {

        g_assert (parse_context->current_number == NULL);
        parse_context->current_number = calls_emergency_number_new (attribute_values[i],
                                                                    CALLS_EMERGENCY_CALL_TYPE_NONE);

        break;
      }
    }
    parse_context->state = PARSER_EMERGENCY_NUMBER;
  }
}


static CallsEmergencyCallTypeFlags
type_to_flag (const char *type)
{
  if (g_str_equal (type, "police")) {
    return CALLS_EMERGENCY_CALL_TYPE_POLICE;
  } else if (g_str_equal (type, "ambulance")) {
    return CALLS_EMERGENCY_CALL_TYPE_AMBULANCE;
  } else if (g_str_equal (type, "fire-brigade")) {
    return CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE;
  }

  return CALLS_EMERGENCY_CALL_TYPE_NONE;
}


static void
parser_emergency_number_start (CallsParseContext *parse_context,
                               const char        *name,
                               const char       **attribute_names,
                               const char       **attribute_values)
{
  if (g_str_equal (name, "callee")) {

    g_assert (parse_context->current_flags == CALLS_EMERGENCY_CALL_TYPE_NONE);
    for (int i = 0; !gm_str_is_null_or_empty (attribute_names[i]); i++) {
      if (g_str_equal (attribute_names[i], "type")) {
        parse_context->current_flags = type_to_flag (attribute_values[i]);
        break;
      }
    }

    parse_context->state = PARSER_CALLEE;
  }
}


static void
parser_start_element (GMarkupParseContext *context,
                      const char          *element_name,
                      const char         **attribute_names,
                      const char         **attribute_values,
                      gpointer             user_data,
                      GError             **error)
{
  CallsParseContext *parse_context = user_data;

  g_clear_pointer (&parse_context->text_buffer, g_free);

  switch (parse_context->state) {
  case PARSER_TOPLEVEL:
    parser_toplevel_start (parse_context, element_name, attribute_names, attribute_values);
    break;
  case PARSER_COUNTRY:
    parser_country_start (parse_context, element_name, attribute_names, attribute_values);
    break;
  case PARSER_EMERGENCY_NUMBERS:
    parser_emergency_numbers_start (parse_context, element_name, attribute_names, attribute_values);
    break;
  case PARSER_EMERGENCY_NUMBER:
    parser_emergency_number_start (parse_context, element_name, attribute_names, attribute_values);
    break;
  case PARSER_CALLEE:
    break;
  case PARSER_ERROR:
    break;
  case PARSER_DONE:
    break;
  default:
    g_assert_not_reached ();
  }
}


static void
parser_callee_end (CallsParseContext *parse_context, const char *name)
{
  if (g_str_equal (name, "callee")) {
    parse_context->current_number->flags |= parse_context->current_flags;
    parse_context->current_flags = CALLS_EMERGENCY_CALL_TYPE_NONE;
    g_clear_pointer (&parse_context->text_buffer, g_free);
    parse_context->state = PARSER_EMERGENCY_NUMBER;
  }
}


static void
parser_emergency_number_end (CallsParseContext *parse_context, const char *name)
{
  if (g_str_equal (name, "emergency-number")) {
    g_ptr_array_add (parse_context->current_data->numbers,
                     g_steal_pointer (&parse_context->current_number));
    g_clear_pointer (&parse_context->text_buffer, g_free);
    parse_context->state = PARSER_EMERGENCY_NUMBERS;
  }
}


static void
parser_emergency_numbers_end (CallsParseContext *parse_context, const char *name)
{
  if (g_str_equal (name, "emergency-numbers")) {
    g_clear_pointer (&parse_context->text_buffer, g_free);
    parse_context->state = PARSER_COUNTRY;
  }
}


static void
parser_country_end (CallsParseContext *parse_context, const char *name)
{
  if (g_str_equal (name, "country")) {
    /* Only add country if we have any emergency numbers */
    if (parse_context->current_data->numbers->len) {
      g_hash_table_insert (parse_context->info,
                           parse_context->current_data->country_code,
                           parse_context->current_data);
      parse_context->current_data = NULL;
    }
    g_clear_pointer (&parse_context->current_data, calls_emergency_call_country_data_free);
    g_clear_pointer (&parse_context->text_buffer, g_free);
    parse_context->state = PARSER_TOPLEVEL;
  }
}


static void
parser_end_element (GMarkupParseContext *context,
                    const char          *element_name,
                    gpointer             user_data,
                    GError             **error)
{
  CallsParseContext *parse_context = user_data;

  switch (parse_context->state) {
  case PARSER_TOPLEVEL:
    break;
  case PARSER_COUNTRY:
    parser_country_end (parse_context, element_name);
    break;
  case PARSER_EMERGENCY_NUMBERS:
    parser_emergency_numbers_end (parse_context, element_name);
    break;
  case PARSER_EMERGENCY_NUMBER:
    parser_emergency_number_end (parse_context, element_name);
    break;
  case PARSER_CALLEE:
    parser_callee_end (parse_context, element_name);
  case PARSER_ERROR:
    break;
  case PARSER_DONE:
    break;
  default:
    g_assert_not_reached ();
  }
}


static void
parser_text (GMarkupParseContext *context,
             const char          *text,
             gsize                text_len,
             gpointer             user_data,
             GError             **error)
{
  CallsParseContext *parse_context = user_data;

  g_free (parse_context->text_buffer);
  parse_context->text_buffer = g_strdup (text);
}


static const GMarkupParser parser = {
  .start_element = parser_start_element,
  .end_element   = parser_end_element,
  .text          = parser_text,
  .passthrough   = NULL,
  .error         = NULL,
};


static void read_next_chunk (GInputStream *stream, GTask *task);


static void
on_stream_read_ready (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GInputStream *stream = G_INPUT_STREAM (source_object);
  g_autoptr (GTask) task = G_TASK (user_data);
  CallsParseContext *parse_context = g_task_get_task_data (task);
  gssize len;
  GError *error = NULL;

  len = g_input_stream_read_finish (stream, res, &error);
  if (len == -1) {
    g_prefix_error (&error, "Error reading service provider database: ");
    g_task_return_error (task, error);
    return;
  }

  if (len == 0) {
    g_task_return_pointer (task,
                           g_steal_pointer (&parse_context->info),
                           (GDestroyNotify)g_hash_table_unref);
    return;
  }

  if (!g_markup_parse_context_parse (parse_context->ctx, parse_context->buffer, len, &error)) {
    g_prefix_error (&error, "Error parsing service provider database: ");
    g_task_return_error (task, error);
    return;
  }

  read_next_chunk (stream, g_steal_pointer (&task));
}


static void
read_next_chunk (GInputStream *stream, GTask *task)
{
  CallsParseContext *parse_context = g_task_get_task_data (task);

  g_input_stream_read_async (stream,
                             parse_context->buffer,
                             sizeof (parse_context->buffer),
                             G_PRIORITY_DEFAULT,
                             g_task_get_cancellable (task),
                             on_stream_read_ready,
                             task);
}


static void
on_file_read_ready (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GFileInputStream) stream = NULL;
  GError *error = NULL;
  GFile *file = G_FILE (source_object);

  stream = g_file_read_finish (file, res, &error);
  if (!stream) {
    g_prefix_error (&error, "Error opening service provider database: ");
    g_task_return_error (task, error);
    return;
  }

  read_next_chunk (G_INPUT_STREAM (stream), g_steal_pointer (&task));
}


GHashTable *

calls_service_providers_get_emergency_info_finish (GAsyncResult  *res,
                                                   GError       **error)
{
  g_assert (G_IS_TASK (res));
  g_assert (g_task_get_source_tag(G_TASK (res)) == calls_service_providers_get_emergency_info);

  return g_task_propagate_pointer (G_TASK (res), error);
}


void
calls_service_providers_get_emergency_info (const char          *serviceproviders,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GTask) task = g_task_new (NULL, cancellable, callback, user_data);
  CallsParseContext *parse_context = g_new0 (CallsParseContext, 1);

  g_assert (serviceproviders);

  parse_context->ctx = g_markup_parse_context_new (&parser, 0, parse_context, NULL);
  parse_context->info = g_hash_table_new_full (g_str_hash,
                                               g_str_equal,
                                               NULL,
                                               (GDestroyNotify)
                                               calls_emergency_call_country_data_free);

  g_task_set_task_data (task, parse_context, (GDestroyNotify)calls_parse_context_free);
  g_task_set_source_tag (task, calls_service_providers_get_emergency_info);

  file = g_file_new_for_path (serviceproviders);
  g_file_read_async (file, G_PRIORITY_DEFAULT,
                     cancellable,
                     on_file_read_ready,
                     g_steal_pointer (&task));
}


static void
on_get_emergency_info_ready (GObject *object, GAsyncResult *res, gpointer user_data)
{
  GetChannelsSyncData *data = user_data;

  g_assert (data->res == NULL);
  data->res = g_object_ref (res);
  g_main_loop_quit (data->loop);
}


GHashTable *
calls_service_providers_get_emergency_info_sync (const char *serviceproviders,
                                                 GError    **error)
{
  GHashTable *info;
  GetChannelsSyncData data;
  g_autoptr (GMainContext) context = g_main_context_new ();
  g_autoptr (GMainLoop) loop = NULL;

  g_main_context_push_thread_default (context);
  loop = g_main_loop_new (context, FALSE);

  data = (GetChannelsSyncData) {
    .loop = loop,
    .res = NULL,
  };

  calls_service_providers_get_emergency_info (serviceproviders,
                                              NULL,
                                              on_get_emergency_info_ready,
                                              &data);
  g_main_loop_run (data.loop);

  info = calls_service_providers_get_emergency_info_finish (data.res, error);

  g_clear_object (&data.res);
  g_main_context_pop_thread_default (context);

  return info;
}
