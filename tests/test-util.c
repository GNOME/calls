/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "util.h"

#include <gtk/gtk.h>

static void
test_protocol_prefix (void)
{
  g_assert_cmpstr (get_protocol_from_address ("sip:alice@example.org"), ==, "sip");
  g_assert_cmpstr (get_protocol_from_address ("SIP:alice@example.org"), ==, "sip");
  g_assert_cmpstr (get_protocol_from_address ("sips:bob@example.org"), ==, "sips");
  g_assert_cmpstr (get_protocol_from_address ("sIpS:bob@example.org"), ==, "sips");
  g_assert_cmpstr (get_protocol_from_address ("tel:+49123456789"), ==, "tel");
  g_assert_cmpstr (get_protocol_from_address ("+49123456789"), ==, NULL);
  g_assert_cmpstr (get_protocol_from_address_with_fallback ("+49123456789"), ==, "tel");
  g_assert_cmpstr (get_protocol_from_address ("mailto:charley@spam.com"), ==, NULL);
}


int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/util/protocol_prefix", (GTestFunc) test_protocol_prefix);

  g_test_run ();
}
