/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "calls-dummy-provider.h"

typedef struct {
  CallsDummyProvider *dummy_provider;
} ProviderFixture;

void test_dummy_provider_set_up (ProviderFixture *fixture, gconstpointer user_data);
void test_dummy_provider_tear_down (ProviderFixture *fixture, gconstpointer user_data);
