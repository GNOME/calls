/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-srtp-utils.h"

#include <glib.h>


static void
assert_attr_eq (calls_srtp_crypto_attribute *a,
                calls_srtp_crypto_attribute *b)
{
  g_assert_cmpint (a->tag, ==, b->tag);
  g_assert_cmpint (a->crypto_suite, ==, b->crypto_suite);
  g_assert_cmpuint (a->n_key_params, ==, b->n_key_params);

  for (guint i = 0; i < a->n_key_params; i++) {
    g_assert_cmpstr (a->key_params[i].b64_keysalt, ==,
                     b->key_params[i].b64_keysalt);
    g_assert_cmpuint (a->key_params[i].mki, ==,
                      b->key_params[i].mki);
    g_assert_cmpuint (a->key_params[i].mki_length, ==,
                      b->key_params[i].mki_length);
  }

  g_assert_cmpint (a->unencrypted_srtp, ==, b->unencrypted_srtp);
  g_assert_cmpint (a->unauthenticated_srtp, ==, b->unauthenticated_srtp);
  g_assert_cmpint (a->unencrypted_srtcp, ==, b->unencrypted_srtcp);

  g_assert_cmpint (a->kdr, ==, b->kdr);
  g_assert_cmpint (a->fec_order, ==, b->fec_order);
  g_assert_cmpint (a->window_size_hint, ==, b->window_size_hint);
}


static void
test_crypto_attribute_validity (void)
{
  g_autoptr (calls_srtp_crypto_attribute) attr = NULL;
  char *attr_str;
  guchar *key_salt;
  char *tmp_str;

  /* single key parameter */

  attr = calls_srtp_crypto_attribute_new (1);

  attr->tag = 1;
  attr->crypto_suite = CALLS_SRTP_SUITE_UNKNOWN;

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_32;
  key_salt = calls_srtp_generate_key_salt (30);
  attr->key_params[0].b64_keysalt = g_base64_encode (key_salt, 30);
  g_free (key_salt);

  attr_str = calls_srtp_print_sdp_crypto_attribute (attr, NULL);
  g_assert_true (attr_str);

  g_free (attr_str);

  /* tag out of bounds */
  attr->tag = 0;
  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  attr->tag = 1000000000;
  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  attr->tag = 1;

  /* lifetime out of bounds */
  attr->key_params[0].lifetime = 49;
  attr->key_params[0].lifetime_type = CALLS_SRTP_LIFETIME_AS_POWER_OF_TWO;

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  attr->key_params[0].lifetime = 1ULL << 48;
  attr->key_params[0].lifetime_type = CALLS_SRTP_LIFETIME_AS_DECIMAL_NUMBER;

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  attr->key_params[0].lifetime = 0;
  attr->key_params[0].lifetime_type = CALLS_SRTP_LIFETIME_UNSET;

  /* MKI without length */
  attr->key_params[0].mki = 1;
  attr->key_params[0].mki_length = 0;

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  attr->key_params[0].mki_length = 129;

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  attr->key_params[0].mki = 0;
  attr->key_params[0].mki_length = 4;

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  /* missing key */
  attr->key_params[0].mki_length = 0;
  g_clear_pointer (&attr->key_params[0].b64_keysalt, g_free);

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  /* wrong key length */
  key_salt = calls_srtp_generate_key_salt (29);
  attr->key_params[0].b64_keysalt = g_base64_encode (key_salt, 29);
  g_free (key_salt);

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  calls_srtp_crypto_attribute_free (attr);

  /* multiple key parameters */

  attr = calls_srtp_crypto_attribute_new (4);
  attr->tag = 12;
  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_80;

  calls_srtp_crypto_attribute_init_keys (attr);
  attr->key_params[0].lifetime = 31;
  attr->key_params[0].lifetime_type = CALLS_SRTP_LIFETIME_AS_POWER_OF_TWO;

  attr_str = calls_srtp_print_sdp_crypto_attribute (attr, NULL);
  g_assert_true (attr_str);

  g_free (attr_str);
  calls_srtp_crypto_attribute_free (attr);


  /* same key */
  attr = calls_srtp_crypto_attribute_new (2);
  attr->tag = 1;
  tmp_str = attr->key_params[1].b64_keysalt;
  attr->key_params[1].b64_keysalt = attr->key_params[0].b64_keysalt;

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  attr->key_params[1].b64_keysalt = tmp_str;

  /* same MKI */
  attr->key_params[0].mki = 1;
  attr->key_params[1].mki = 1;

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  /* different MKI lengths */
  attr->key_params[1].mki = 2;
  attr->key_params[0].mki_length = 1;
  attr->key_params[1].mki_length = 3;

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (attr, NULL));

  g_assert_null (calls_srtp_print_sdp_crypto_attribute (NULL, NULL));
}


static void
test_parse (void)
{
  g_autoptr (calls_srtp_crypto_attribute) attr_simple = NULL;
  g_autoptr (calls_srtp_crypto_attribute) attr_parsed_simple = NULL;
  g_autofree char *attr_simple_str = NULL;
  g_autofree char *attr_simple_str_expected = NULL;

  g_autoptr (calls_srtp_crypto_attribute) attr_multi = NULL;
  g_autoptr (calls_srtp_crypto_attribute) attr_parsed_multi = NULL;
  g_autofree char *attr_multi_str = NULL;
  g_autofree char *attr_multi_str_expected = NULL;

  g_autofree guchar *key_salt = NULL;

  /* single key */

  attr_simple = calls_srtp_crypto_attribute_new (1);
  key_salt = calls_srtp_generate_key_salt (30);
  attr_simple->tag = 1;
  attr_simple->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_32;
  attr_simple->key_params[0].b64_keysalt = g_base64_encode (key_salt, 30);

  attr_simple_str = calls_srtp_print_sdp_crypto_attribute (attr_simple, NULL);
  attr_simple_str_expected =
    g_strdup_printf ("a=crypto:%d AES_CM_128_HMAC_SHA1_32 inline:%s",
                     attr_simple->tag,
                     attr_simple->key_params[0].b64_keysalt);

  g_assert_cmpstr (attr_simple_str, ==, attr_simple_str_expected);

  attr_parsed_simple = calls_srtp_parse_sdp_crypto_attribute (attr_simple_str, NULL);
  assert_attr_eq (attr_simple, attr_parsed_simple);

  /* multiple keys */

  attr_multi = calls_srtp_crypto_attribute_new (2);
  attr_multi->tag = 42;
  attr_multi->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_80;
  calls_srtp_crypto_attribute_init_keys (attr_multi);

  attr_multi_str = calls_srtp_print_sdp_crypto_attribute (attr_multi, NULL);
  attr_multi_str_expected =
    g_strdup_printf ("a=crypto:%d AES_CM_128_HMAC_SHA1_80 "
                     "inline:%s|%" G_GUINT64_FORMAT ":%u;"
                     "inline:%s|%" G_GUINT64_FORMAT ":%u",
                     attr_multi->tag,
                     attr_multi->key_params[0].b64_keysalt,
                     attr_multi->key_params[0].mki,
                     attr_multi->key_params[0].mki_length,
                     attr_multi->key_params[1].b64_keysalt,
                     attr_multi->key_params[1].mki,
                     attr_multi->key_params[1].mki_length);

  g_assert_cmpstr (attr_multi_str, ==, attr_multi_str_expected);

  attr_parsed_multi = calls_srtp_parse_sdp_crypto_attribute (attr_multi_str, NULL);
  assert_attr_eq (attr_multi, attr_parsed_multi);
}


static void
test_srtp_params (void)
{
  calls_srtp_crypto_attribute *attr = calls_srtp_crypto_attribute_new (1);
  const char *srtp_cipher;
  const char *srtp_auth;
  const char *srtcp_cipher;
  const char *srtcp_auth;
  GstSrtpCipherType srtp_cipher_enum;
  GstSrtpAuthType srtp_auth_enum;
  GstSrtpCipherType srtcp_cipher_enum;
  GstSrtpAuthType srtcp_auth_enum;

  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_32;
  attr->unencrypted_srtp = FALSE;
  attr->unauthenticated_srtp = FALSE;
  attr->unencrypted_srtcp = FALSE;

  g_assert_true (calls_srtp_crypto_get_srtpdec_params (attr,
                                                       &srtp_cipher,
                                                       &srtp_auth,
                                                       &srtcp_cipher,
                                                       &srtcp_auth));
  g_assert_cmpstr (srtp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtp_auth, ==, "hmac-sha1-32");
  g_assert_cmpstr (srtcp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtcp_auth, ==, "hmac-sha1-32");

  g_assert_true (calls_srtp_crypto_get_srtpenc_params (attr,
                                                       &srtp_cipher_enum,
                                                       &srtp_auth_enum,
                                                       &srtcp_cipher_enum,
                                                       &srtcp_auth_enum));

  g_assert_cmpint (srtp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_32);
  g_assert_cmpint (srtcp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtcp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_32);


  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_32;
  attr->unencrypted_srtp = TRUE;
  attr->unauthenticated_srtp = FALSE;
  attr->unencrypted_srtcp = FALSE;

  g_assert_true (calls_srtp_crypto_get_srtpdec_params (attr,
                                                       &srtp_cipher,
                                                       &srtp_auth,
                                                       &srtcp_cipher,
                                                       &srtcp_auth));
  g_assert_cmpstr (srtp_cipher, ==, "null");
  g_assert_cmpstr (srtp_auth, ==, "hmac-sha1-32");
  g_assert_cmpstr (srtcp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtcp_auth, ==, "hmac-sha1-32");

  g_assert_true (calls_srtp_crypto_get_srtpenc_params (attr,
                                                       &srtp_cipher_enum,
                                                       &srtp_auth_enum,
                                                       &srtcp_cipher_enum,
                                                       &srtcp_auth_enum));

  g_assert_cmpint (srtp_cipher_enum, ==, GST_SRTP_CIPHER_NULL);
  g_assert_cmpint (srtp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_32);
  g_assert_cmpint (srtcp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtcp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_32);


  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_32;
  attr->unencrypted_srtp = FALSE;
  attr->unauthenticated_srtp = TRUE;
  attr->unencrypted_srtcp = FALSE;

  g_assert_true (calls_srtp_crypto_get_srtpdec_params (attr,
                                                       &srtp_cipher,
                                                       &srtp_auth,
                                                       &srtcp_cipher,
                                                       &srtcp_auth));
  g_assert_cmpstr (srtp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtp_auth, ==, "null");
  g_assert_cmpstr (srtcp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtcp_auth, ==, "hmac-sha1-32");

  g_assert_true (calls_srtp_crypto_get_srtpenc_params (attr,
                                                       &srtp_cipher_enum,
                                                       &srtp_auth_enum,
                                                       &srtcp_cipher_enum,
                                                       &srtcp_auth_enum));

  g_assert_cmpint (srtp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtp_auth_enum, ==, GST_SRTP_AUTH_NULL);
  g_assert_cmpint (srtcp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtcp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_32);


  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_32;
  attr->unencrypted_srtp = FALSE;
  attr->unauthenticated_srtp = FALSE;
  attr->unencrypted_srtcp = TRUE;

  g_assert_true (calls_srtp_crypto_get_srtpdec_params (attr,
                                                       &srtp_cipher,
                                                       &srtp_auth,
                                                       &srtcp_cipher,
                                                       &srtcp_auth));
  g_assert_cmpstr (srtp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtp_auth, ==, "hmac-sha1-32");
  g_assert_cmpstr (srtcp_cipher, ==, "null");
  g_assert_cmpstr (srtcp_auth, ==, "null");

  g_assert_true (calls_srtp_crypto_get_srtpenc_params (attr,
                                                       &srtp_cipher_enum,
                                                       &srtp_auth_enum,
                                                       &srtcp_cipher_enum,
                                                       &srtcp_auth_enum));

  g_assert_cmpint (srtp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_32);
  g_assert_cmpint (srtcp_cipher_enum, ==, GST_SRTP_CIPHER_NULL);
  g_assert_cmpint (srtcp_auth_enum, ==, GST_SRTP_AUTH_NULL);


  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_80;
  attr->unencrypted_srtp = FALSE;
  attr->unauthenticated_srtp = FALSE;
  attr->unencrypted_srtcp = FALSE;

  g_assert_true (calls_srtp_crypto_get_srtpdec_params (attr,
                                                       &srtp_cipher,
                                                       &srtp_auth,
                                                       &srtcp_cipher,
                                                       &srtcp_auth));
  g_assert_cmpstr (srtp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtp_auth, ==, "hmac-sha1-80");
  g_assert_cmpstr (srtcp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtcp_auth, ==, "hmac-sha1-80");

  g_assert_true (calls_srtp_crypto_get_srtpenc_params (attr,
                                                       &srtp_cipher_enum,
                                                       &srtp_auth_enum,
                                                       &srtcp_cipher_enum,
                                                       &srtcp_auth_enum));

  g_assert_cmpint (srtp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_80);
  g_assert_cmpint (srtcp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtcp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_80);


  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_80;
  attr->unencrypted_srtp = TRUE;
  attr->unauthenticated_srtp = FALSE;
  attr->unencrypted_srtcp = FALSE;

  g_assert_true (calls_srtp_crypto_get_srtpdec_params (attr,
                                                       &srtp_cipher,
                                                       &srtp_auth,
                                                       &srtcp_cipher,
                                                       &srtcp_auth));
  g_assert_cmpstr (srtp_cipher, ==, "null");
  g_assert_cmpstr (srtp_auth, ==, "hmac-sha1-80");
  g_assert_cmpstr (srtcp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtcp_auth, ==, "hmac-sha1-80");

  g_assert_true (calls_srtp_crypto_get_srtpenc_params (attr,
                                                       &srtp_cipher_enum,
                                                       &srtp_auth_enum,
                                                       &srtcp_cipher_enum,
                                                       &srtcp_auth_enum));

  g_assert_cmpint (srtp_cipher_enum, ==, GST_SRTP_CIPHER_NULL);
  g_assert_cmpint (srtp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_80);
  g_assert_cmpint (srtcp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtcp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_80);


  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_80;
  attr->unencrypted_srtp = FALSE;
  attr->unauthenticated_srtp = TRUE;
  attr->unencrypted_srtcp = FALSE;

  g_assert_true (calls_srtp_crypto_get_srtpdec_params (attr,
                                                       &srtp_cipher,
                                                       &srtp_auth,
                                                       &srtcp_cipher,
                                                       &srtcp_auth));
  g_assert_cmpstr (srtp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtp_auth, ==, "null");
  g_assert_cmpstr (srtcp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtcp_auth, ==, "hmac-sha1-80");

  g_assert_true (calls_srtp_crypto_get_srtpenc_params (attr,
                                                       &srtp_cipher_enum,
                                                       &srtp_auth_enum,
                                                       &srtcp_cipher_enum,
                                                       &srtcp_auth_enum));

  g_assert_cmpint (srtp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtp_auth_enum, ==, GST_SRTP_AUTH_NULL);
  g_assert_cmpint (srtcp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtcp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_80);


  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_80;
  attr->unencrypted_srtp = FALSE;
  attr->unauthenticated_srtp = FALSE;
  attr->unencrypted_srtcp = TRUE;

  g_assert_true (calls_srtp_crypto_get_srtpdec_params (attr,
                                                       &srtp_cipher,
                                                       &srtp_auth,
                                                       &srtcp_cipher,
                                                       &srtcp_auth));
  g_assert_cmpstr (srtp_cipher, ==, "aes-128-icm");
  g_assert_cmpstr (srtp_auth, ==, "hmac-sha1-80");
  g_assert_cmpstr (srtcp_cipher, ==, "null");
  g_assert_cmpstr (srtcp_auth, ==, "null");

  g_assert_true (calls_srtp_crypto_get_srtpenc_params (attr,
                                                       &srtp_cipher_enum,
                                                       &srtp_auth_enum,
                                                       &srtcp_cipher_enum,
                                                       &srtcp_auth_enum));

  g_assert_cmpint (srtp_cipher_enum, ==, GST_SRTP_CIPHER_AES_128_ICM);
  g_assert_cmpint (srtp_auth_enum, ==, GST_SRTP_AUTH_HMAC_SHA1_80);
  g_assert_cmpint (srtcp_cipher_enum, ==, GST_SRTP_CIPHER_NULL);
  g_assert_cmpint (srtcp_auth_enum, ==, GST_SRTP_AUTH_NULL);


  attr->crypto_suite = CALLS_SRTP_SUITE_UNKNOWN;

  g_assert_false (calls_srtp_crypto_get_srtpdec_params (attr,
                                                        &srtp_cipher,
                                                        &srtp_auth,
                                                        &srtcp_cipher,
                                                        &srtcp_auth));

  g_assert_false (calls_srtp_crypto_get_srtpenc_params (attr,
                                                        &srtp_cipher_enum,
                                                        &srtp_auth_enum,
                                                        &srtcp_cipher_enum,
                                                        &srtcp_auth_enum));
  calls_srtp_crypto_attribute_free (attr);
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/SRTP-SDP/crypto_attribute_validity", test_crypto_attribute_validity);
  g_test_add_func ("/Calls/SRTP-SDP/parse", test_parse);
  g_test_add_func ("/Calls/SRTP/srtp_params", test_srtp_params);

  return g_test_run ();
}
