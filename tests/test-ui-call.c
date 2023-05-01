/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 */

#include "calls-ui-call-data.h"
#include "mock-call.h"

#include <glib.h>

static void
test_cui_call_state_mapping (void)
{
  g_assert_cmpint (calls_call_state_to_cui_call_state (CALLS_CALL_STATE_UNKNOWN),
                   ==, CUI_CALL_STATE_UNKNOWN);
  g_assert_cmpint (calls_call_state_to_cui_call_state (CALLS_CALL_STATE_ACTIVE),
                   ==, CUI_CALL_STATE_ACTIVE);
  g_assert_cmpint (calls_call_state_to_cui_call_state (CALLS_CALL_STATE_DIALING),
                   ==, CUI_CALL_STATE_CALLING);
  g_assert_cmpint (calls_call_state_to_cui_call_state (CALLS_CALL_STATE_ALERTING),
                   ==, CUI_CALL_STATE_CALLING);
  g_assert_cmpint (calls_call_state_to_cui_call_state (CALLS_CALL_STATE_INCOMING),
                   ==, CUI_CALL_STATE_INCOMING);
  g_assert_cmpint (calls_call_state_to_cui_call_state (CALLS_CALL_STATE_DISCONNECTED),
                   ==, CUI_CALL_STATE_DISCONNECTED);
  g_assert_cmpint (calls_call_state_to_cui_call_state (42), ==, CUI_CALL_STATE_UNKNOWN);
}


static void
test_cui_call_properties (void)
{
  CallsMockCall *mock_call = calls_mock_call_new ();
  CallsCall *call; /* so we can avoid having to cast all the time */
  CallsUiCallData *ui_call;
  CuiCall *cui_call; /* so we can avoid having to cast all the time */

  gboolean inbound;

  g_assert_true (CALLS_IS_CALL (mock_call));
  call = CALLS_CALL (mock_call);
  ui_call = calls_ui_call_data_new (call, "test-id");

  g_assert_true (CUI_IS_CALL (ui_call));
  cui_call = CUI_CALL (ui_call);

  g_assert_cmpstr (calls_ui_call_data_get_origin_id (ui_call), ==, "test-id");
  g_assert_cmpstr (calls_call_get_id (call), ==, cui_call_get_id (cui_call));
  g_assert_cmpstr (calls_call_get_name (call), ==, cui_call_get_display_name (cui_call));
  g_assert_cmpint (calls_call_state_to_cui_call_state (calls_call_get_state (call)), ==,
                 cui_call_get_state (cui_call));

  g_object_get (cui_call, "inbound", &inbound, NULL);
  g_assert_true (calls_call_get_inbound (call) == inbound);

  /* Test if properties get updated */
  calls_call_answer (call);
  g_assert_cmpint (calls_call_state_to_cui_call_state (calls_call_get_state (call)), ==,
                 cui_call_get_state (cui_call));

  calls_call_hang_up (call);
  g_assert_cmpint (calls_call_state_to_cui_call_state (calls_call_get_state (call)), ==,
                 cui_call_get_state (cui_call));

  calls_call_set_id (call, "0123456789");
  g_assert_cmpstr (calls_call_get_id (call), ==, cui_call_get_id (cui_call));
  g_assert_cmpstr (cui_call_get_id (cui_call), ==, "0123456789");

  calls_call_set_name (call, "Jane Doe");
  g_assert_cmpstr (calls_call_get_name (call), ==, cui_call_get_display_name (cui_call));
  g_assert_cmpstr (cui_call_get_display_name (cui_call), ==, "Jane Doe");
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/UI/state_mapping", (GTestFunc) test_cui_call_state_mapping);
  g_test_add_func ("/Calls/UI/call_properties", (GTestFunc) test_cui_call_properties);

  g_test_run ();
}
