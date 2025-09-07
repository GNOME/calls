/*
 * Copyright (C) 2025 The Phosh.mobi e.V.
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#include "calls-emergency-call-types.h"
#include "calls-service-providers.h"

#include <gio/gio.h>
#include <glib.h>


#define calls_assert_cmp_emergency_number(d, i, n, f) G_STMT_START {    \
  CallsEmergencyNumber *_n = g_ptr_array_index (d->numbers, i);         \
  if (!_n) {                                                            \
    g_autofree char *__msg =                                            \
      g_strdup_printf ("Emergency number '%u' does not exist", i);      \
    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, __msg); \
  }                                                                     \
  if (!_n->number) {                                                    \
    g_autofree char *__msg =                                            \
      g_strdup_printf ("Emergency number of element '%u' is NULL", i);  \
    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, __msg); \
  }                                                                     \
  if (!g_str_equal (_n->number, n)) {                                   \
    g_autofree char *__msg =                                            \
      g_strdup_printf ("Emergency number of element '%u' is '%s' not '%s'", i, _n->number, n); \
    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, __msg); \
  }                                                                     \
  if (_n->flags != f) {                                                 \
    g_autofree char *__msg =                                            \
      g_strdup_printf ("Emergency number of element '%u' has flags '0x%x'' not '0x%x'", i, _n->flags, f); \
    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, __msg); \
  }                                                                     \
} G_STMT_END


static gboolean
numbers_equal (gconstpointer a, gconstpointer b)
{
  const CallsEmergencyNumber *n_a = a;
  const char *needle = b;

  return g_str_equal (n_a->number, needle);
}


static void
test_service_providers_parse_de (void)
{
  g_autoptr (GHashTable) info = NULL;
  g_autoptr (GError) err = NULL;
  CallsEmergencyCallCountryData *data;
  CallsEmergencyNumber *number;
  guint index;

  info = g_hash_table_new_full (g_str_hash,
                                g_str_equal,
                                NULL,
                                (GDestroyNotify) calls_emergency_call_country_data_free);

  info = calls_service_providers_get_emergency_info_sync (TEST_DATABASE, &err);

  g_assert_no_error (err);
  g_assert_nonnull (info);

  data = g_hash_table_lookup (info, "xx");
  g_assert_nonnull (data);
  g_assert_cmpint (data->numbers->len, ==, 3);

  g_assert_true (g_ptr_array_find_with_equal_func (data->numbers, "114", numbers_equal, &index));
  number = g_ptr_array_index (data->numbers, index);
  g_assert_nonnull (number);
  calls_assert_cmp_emergency_number(data, index, "114", CALLS_EMERGENCY_CALL_TYPE_AMBULANCE);

  g_assert_true (g_ptr_array_find_with_equal_func (data->numbers, "117", numbers_equal, &index));
  number = g_ptr_array_index (data->numbers, index);
  g_assert_nonnull (number);
  calls_assert_cmp_emergency_number(data, index, "117", CALLS_EMERGENCY_CALL_TYPE_POLICE);

  g_assert_true (g_ptr_array_find_with_equal_func (data->numbers, "118", numbers_equal, &index));
  number = g_ptr_array_index (data->numbers, index);
  g_assert_nonnull (number);
  calls_assert_cmp_emergency_number(data, index, "118", CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE);

  data = g_hash_table_lookup (info, "yy");
  g_assert_nonnull (data);
  g_assert_cmpint (data->numbers->len, ==, 1);
  g_assert_true (g_ptr_array_find_with_equal_func (data->numbers, "112", numbers_equal, &index));
  number = g_ptr_array_index (data->numbers, index);
  g_assert_nonnull (number);
  calls_assert_cmp_emergency_number(data, index, "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
                                                         CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE |
                                                         CALLS_EMERGENCY_CALL_TYPE_AMBULANCE));

  data = g_hash_table_lookup (info, "zz");
  g_assert_null (data);
}


gint
main (gint argc, gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/calls/service-providers/parse", test_service_providers_parse_de);

  return g_test_run ();
}
