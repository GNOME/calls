/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "calls-dummy-provider.h"
#include "calls-provider.h"
#include "calls-origin.h"
#include "calls-message-source.h"

#include <gtk/gtk.h>
#include <string.h>

typedef struct {
  CallsDummyProvider *dummy_provider;
} Fixture;


static void
test_dummy_provider_set_up (Fixture *fixture,
                            gconstpointer user_data)
{
  fixture->dummy_provider = calls_dummy_provider_new ();
  calls_dummy_provider_add_origin (fixture->dummy_provider,
                                   "Test origin 1");
  calls_dummy_provider_add_origin (fixture->dummy_provider,
                                   "Test origin 2");
}


static void
test_dummy_provider_tear_down (Fixture *fixture,
                               gconstpointer user_data)
{
  g_clear_object (&fixture->dummy_provider);
}


static void
test_dummy_provider_object (Fixture *fixture,
                            gconstpointer user_data)
{
  g_assert_true (G_IS_OBJECT (fixture->dummy_provider));
  g_assert_true (CALLS_IS_MESSAGE_SOURCE (fixture->dummy_provider));
  g_assert_true (CALLS_IS_PROVIDER (fixture->dummy_provider));
}


static void
test_dummy_provider_get_name (Fixture *fixture,
                              gconstpointer user_data)
{
  CallsProvider *provider;
  const gchar *name;

  provider = CALLS_PROVIDER (fixture->dummy_provider);

  name = calls_provider_get_name (provider);
  g_assert_nonnull (name);
  g_assert_cmpuint (strlen (name), >, 0U);
}


static void
test_dummy_provider_get_origins (Fixture *fixture,
                                 gconstpointer user_data)
{
  GList *origins, *node;

  origins = calls_provider_get_origins
    (CALLS_PROVIDER (fixture->dummy_provider));

  g_assert_cmpuint (g_list_length (origins), ==, 2);

  for (node = origins; node; node = node->next)
    {
      g_assert_true (G_IS_OBJECT (node->data));
      g_assert_true (CALLS_IS_ORIGIN (node->data));
    }

  g_list_free (origins);
}


gint
main (gint   argc,
      gchar *argv[])
{
  gtk_test_init (&argc, &argv, NULL);


#define add_test(name)                                  \
  g_test_add ("/Calls/Provider/" #name, Fixture, NULL,  \
              test_dummy_provider_set_up,               \
              test_dummy_provider_##name,               \
              test_dummy_provider_tear_down);           \

  add_test(object);
  add_test(get_name);
  add_test(get_origins);

#undef add_test

  return g_test_run();
}
