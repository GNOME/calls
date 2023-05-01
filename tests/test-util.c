/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-util.h"

#include <glib.h>

#include <sys/socket.h>


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


static gboolean
string_contains_char (char *str, char c)
{
  size_t len = strlen (str);
  for (guint i = 0; i < len; i++) {
    if (str[i] == c)
      return TRUE;
  }

  return FALSE;
}


static void
test_dtmf_tone_validity (void)
{
  char *valid_tones = "0123456789ABCD*#";

  for (char c = CHAR_MIN; c < CHAR_MAX; c++) {
    if (string_contains_char (valid_tones, c))
      g_assert_true (dtmf_tone_key_is_valid (c));
    else
      g_assert_false (dtmf_tone_key_is_valid (c));
  }
}


static void
test_call_icon_names (void)
{
  g_assert_cmpstr (get_call_icon_symbolic_name (FALSE, FALSE), ==, "call-arrow-outgoing-symbolic");
  g_assert_cmpstr (get_call_icon_symbolic_name (FALSE, TRUE), ==, "call-arrow-outgoing-missed-symbolic");
  g_assert_cmpstr (get_call_icon_symbolic_name (TRUE, FALSE), ==, "call-arrow-incoming-symbolic");
  g_assert_cmpstr (get_call_icon_symbolic_name (TRUE, TRUE), ==, "call-arrow-incoming-missed-symbolic");
}


static void
test_null_empty_strings (void)
{
  g_assert_true (STR_IS_NULL_OR_EMPTY ((const char *) NULL));
  g_assert_true (STR_IS_NULL_OR_EMPTY (""));
  g_assert_false (STR_IS_NULL_OR_EMPTY ("lorem ipsum"));
}


static void
test_ip_address_family (void)
{
  g_assert_cmpint (get_address_family_for_ip ("10.42.0.1", FALSE), ==, AF_INET);
  g_assert_cmpint (get_address_family_for_ip ("9.9.9.9", FALSE), ==, AF_INET);
  g_assert_cmpint (get_address_family_for_ip ("127.0.0.1", FALSE), ==, AF_INET);
  g_assert_cmpint (get_address_family_for_ip ("0.0.0.0", FALSE), ==, AF_INET);


  g_assert_cmpint (get_address_family_for_ip ("FE80:0000:0000:0000:0202:C3DF:FE1E:8329", FALSE), ==, AF_INET6);
  g_assert_cmpint (get_address_family_for_ip ("FE80::0202:C3DF:FE1E:8329", FALSE), ==, AF_INET6);
  g_assert_cmpint (get_address_family_for_ip ("fe80::0202:c3df:fe1e:8329", FALSE), ==, AF_INET6);
  g_assert_cmpint (get_address_family_for_ip ("::", FALSE), ==, AF_INET6);
  g_assert_cmpint (get_address_family_for_ip ("::1", FALSE), ==, AF_INET6);

  g_assert_cmpint (get_address_family_for_ip ("example.org", FALSE), ==, AF_UNSPEC);

}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/util/protocol_prefix", (GTestFunc) test_protocol_prefix);
  g_test_add_func ("/Calls/util/dtmf_tones", (GTestFunc) test_dtmf_tone_validity);
  g_test_add_func ("/Calls/util/call_icon_names", (GTestFunc) test_call_icon_names);
  g_test_add_func ("/Calls/util/null-or-empty-strings", (GTestFunc) test_null_empty_strings);
  g_test_add_func ("/Calls/util/ip_address_family", (GTestFunc) test_ip_address_family);

  g_test_run ();
}
