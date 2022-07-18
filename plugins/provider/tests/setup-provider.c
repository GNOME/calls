/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "setup-provider.h"

void
test_dummy_provider_set_up (ProviderFixture *fixture,
                            gconstpointer user_data)
{
  fixture->dummy_provider = calls_dummy_provider_new ();
}


void
test_dummy_provider_tear_down (ProviderFixture *fixture,
                               gconstpointer user_data)
{
  g_clear_object (&fixture->dummy_provider);
}
