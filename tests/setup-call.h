/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */

#include "setup-origin.h"
#include "calls-dummy-call.h"

typedef struct {
  OriginFixture   parent;
  CallsDummyCall *dummy_call;
} CallFixture;

void test_dummy_call_set_up (CallFixture *fixture, gconstpointer user_data);
void test_dummy_call_tear_down (CallFixture *fixture, gconstpointer user_data);
