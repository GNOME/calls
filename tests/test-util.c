/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "util.h"

#include <gtk/gtk.h>
#include <limits.h>

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

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/util/protocol_prefix", (GTestFunc) test_protocol_prefix);
  g_test_add_func ("/Calls/util/dtmf_tones", (GTestFunc) test_dtmf_tone_validity);
  g_test_add_func ("/Calls/util/call_icon_names", (GTestFunc) test_call_icon_names);
  g_test_add_func ("/Calls/util/null-or-empty-strings", (GTestFunc) test_null_empty_strings);

  g_test_run ();
}
