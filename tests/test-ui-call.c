/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 */

#include "calls-ui-call-data.h"

#include <gtk/gtk.h>

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
  g_assert_cmpint (calls_call_state_to_cui_call_state (CALLS_CALL_STATE_WAITING),
                   ==, CUI_CALL_STATE_INCOMING);
  g_assert_cmpint (calls_call_state_to_cui_call_state (CALLS_CALL_STATE_DISCONNECTED),
                   ==, CUI_CALL_STATE_DISCONNECTED);
  g_assert_cmpint (calls_call_state_to_cui_call_state (42), ==, CUI_CALL_STATE_UNKNOWN);
}


int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/UI/state_mapping", (GTestFunc) test_cui_call_state_mapping);

  g_test_run ();
}
