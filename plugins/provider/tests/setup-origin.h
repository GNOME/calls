/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "setup-provider.h"
#include "calls-dummy-origin.h"

#define TEST_ORIGIN_NAME "Test Dummy origin"

#define TEST_CALL_NUMBER "0123456789"

typedef struct {
  ProviderFixture   parent;
  CallsDummyOrigin *dummy_origin;
} OriginFixture;

void test_dummy_origin_set_up (OriginFixture *fixture, gconstpointer user_data);
void test_dummy_origin_tear_down (OriginFixture *fixture, gconstpointer user_data);
