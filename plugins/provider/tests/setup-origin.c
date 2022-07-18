/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "setup-origin.h"
#include "calls-provider.h"

void
test_dummy_origin_set_up (OriginFixture *fixture,
                          gconstpointer user_data)
{
  GListModel *origins;

  test_dummy_provider_set_up (&fixture->parent, user_data);

  /* provider adds an origin with name "Dummy origin" by itself... */
  calls_dummy_provider_add_origin (fixture->parent.dummy_provider,
                                   TEST_ORIGIN_NAME);

  origins = calls_provider_get_origins
    (CALLS_PROVIDER (fixture->parent.dummy_provider));
  fixture->dummy_origin = g_list_model_get_item (origins, 1);
}


void
test_dummy_origin_tear_down (OriginFixture *fixture,
                             gconstpointer user_data)
{
  g_clear_object (&fixture->dummy_origin);
  test_dummy_provider_tear_down (&fixture->parent, user_data);
}
