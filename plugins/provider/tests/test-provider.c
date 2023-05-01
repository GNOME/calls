/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "setup-provider.h"
#include "calls-provider.h"
#include "calls-origin.h"
#include "calls-message-source.h"
#include "common.h"

#include <glib.h>
#include <string.h>

static void
test_dummy_provider_object (ProviderFixture *fixture,
                            gconstpointer user_data)
{
  g_assert_true (G_IS_OBJECT (fixture->dummy_provider));
  g_assert_true (CALLS_IS_MESSAGE_SOURCE (fixture->dummy_provider));
  g_assert_true (CALLS_IS_PROVIDER (fixture->dummy_provider));
}


static void
test_dummy_provider_get_name (ProviderFixture *fixture,
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
test_dummy_provider_origins (ProviderFixture *fixture,
                             gconstpointer user_data)
{
  GListModel *origins;

  origins = calls_provider_get_origins
    (CALLS_PROVIDER (fixture->dummy_provider));
  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 1);
}


gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

#define add_test(name) add_calls_test(Provider, provider, name)

  add_test(object);
  add_test(get_name);
  add_test(origins);

#undef add_test

  return g_test_run();
}
