/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 */

#include "mock-call.h"

#include <glib/gi18n.h>

struct _CallsMockCall
{
  CallsCall       parent_instance;
};

G_DEFINE_TYPE (CallsMockCall, calls_mock_call, CALLS_TYPE_CALL)


static void
calls_mock_call_answer (CallsCall *call)
{
  g_assert (CALLS_IS_MOCK_CALL (call));
  g_assert_cmpint (calls_call_get_state (call), ==, CALLS_CALL_STATE_INCOMING);

  calls_call_set_state (call, CALLS_CALL_STATE_ACTIVE);
}


static void
calls_mock_call_hang_up (CallsCall *call)
{
  g_assert (CALLS_IS_MOCK_CALL (call));
  g_assert_cmpint (calls_call_get_state (call), !=, CALLS_CALL_STATE_DISCONNECTED);

  calls_call_set_state (call, CALLS_CALL_STATE_DISCONNECTED);
}


static void
calls_mock_call_class_init (CallsMockCallClass *klass)
{
  CallsCallClass *call_class = CALLS_CALL_CLASS (klass);

  call_class->answer = calls_mock_call_answer;
  call_class->hang_up = calls_mock_call_hang_up;
}


static void
calls_mock_call_init (CallsMockCall *self)
{
}


CallsMockCall *
calls_mock_call_new (void)
{
   return g_object_new (CALLS_TYPE_MOCK_CALL,
                        "inbound", TRUE,
                        "id", "0800 1234",
                        "name", "John Doe",
                        NULL);
}

