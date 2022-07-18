/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 */


#define add_calls_test(Object,object,name)                              \
  g_test_add ("/Calls/" #Object "/" #name, Object##Fixture, NULL,       \
              test_dummy_##object##_set_up,                             \
              test_dummy_##object##_##name,                             \
              test_dummy_##object##_tear_down)
