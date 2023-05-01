/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-sdp-crypto-context.h"

#include <glib.h>


static void
test_crypto_manual (void)
{
  g_autoptr (CallsSdpCryptoContext) ctx = NULL;
  g_autofree guchar *offer_key = NULL;
  g_autofree char *offer_key_b64 = NULL;
  g_autofree char *offer_crypto_attr = NULL;
  g_autofree char *offer_full = NULL;
  g_autofree guchar *answer_key = NULL;
  g_autofree char *answer_key_b64 = NULL;
  g_autofree char *answer_crypto_attr = NULL;
  g_autofree char *answer_full = NULL;
  calls_srtp_crypto_attribute *offer_attr;
  calls_srtp_crypto_attribute *answer_attr;

  su_home_t home;
  sdp_parser_t *sdp_parser_offer;
  sdp_session_t *sdp_offer;
  sdp_parser_t *sdp_parser_answer;
  sdp_session_t *sdp_answer;

  su_home_init (&home);

  ctx = calls_sdp_crypto_context_new ();

  offer_key = calls_srtp_generate_key_salt (30);
  offer_key_b64 = g_base64_encode (offer_key, 30);
  g_debug ("Offer key: %s", offer_key_b64);

  offer_crypto_attr = g_strdup_printf ("a=crypto:1 AES_CM_128_HMAC_SHA1_32 inline:%s",
                                       offer_key_b64);
  offer_full = g_strdup_printf ("m=audio 21000 RTP/SAVP 0\r\n"
                                "%s",
                                offer_crypto_attr);

  sdp_parser_offer = sdp_parse (&home, offer_full, strlen(offer_full), sdp_f_config);

  sdp_offer = sdp_session (sdp_parser_offer);

  if (!sdp_offer) {
    g_error("%s", sdp_parsing_error(sdp_parser_offer));
  }

  g_assert_true (sdp_offer != NULL);

  g_assert_true (calls_sdp_crypto_context_set_remote_media (ctx, sdp_offer->sdp_media));

  answer_key = calls_srtp_generate_key_salt (30);
  answer_key_b64 = g_base64_encode (answer_key, 30);
  g_debug ("Answer key: %s", answer_key_b64);

  answer_crypto_attr = g_strdup_printf ("a=crypto:1 AES_CM_128_HMAC_SHA1_32 inline:%s",
                                        answer_key_b64);
  answer_full = g_strdup_printf ("m=audio 42000 RTP/SAVP 0\r\n"
                                 "%s",
                                 answer_crypto_attr);

  sdp_parser_answer = sdp_parse (&home, answer_full, strlen(answer_full), sdp_f_config);
  sdp_answer = sdp_session (sdp_parser_answer);

  g_assert_true (calls_sdp_crypto_context_set_local_media (ctx, sdp_answer->sdp_media));

  g_assert_true (calls_sdp_crypto_context_get_is_negotiated (ctx));

  offer_attr = calls_sdp_crypto_context_get_remote_crypto (ctx);
  g_assert_true (offer_attr);
  g_assert_cmpuint (offer_attr->n_key_params, ==, 1);
  g_assert_cmpstr (offer_attr->key_params[0].b64_keysalt, ==, offer_key_b64);

  answer_attr = calls_sdp_crypto_context_get_local_crypto (ctx);
  g_assert_true (answer_attr);
  g_assert_cmpuint (answer_attr->n_key_params, ==, 1);
  g_assert_cmpstr (answer_attr->key_params[0].b64_keysalt, ==, answer_key_b64);

  g_assert_cmpint (offer_attr->tag, ==, answer_attr->tag);
  g_assert_cmpint (offer_attr->crypto_suite, ==, answer_attr->crypto_suite);

  sdp_parser_free (sdp_parser_offer);
  sdp_parser_free (sdp_parser_answer);

  su_home_deinit (&home);
}


static void
test_crypto_offer_answer (void)
{
  g_autoptr (CallsSdpCryptoContext) ctx_offer = calls_sdp_crypto_context_new ();
  g_autoptr (CallsSdpCryptoContext) ctx_answer = calls_sdp_crypto_context_new ();
  g_autoptr (GList) attr_offer_list = NULL;
  calls_srtp_crypto_attribute *attr_offer;
  calls_srtp_crypto_attribute *attr_answer;
  g_autofree char *attr_offer_str = NULL;
  g_autofree char *attr_answer_str = NULL;
  g_autofree char *media_line_offer = NULL;
  g_autofree char *media_line_answer = NULL;
  su_home_t home;
  sdp_parser_t *sdp_parser_offer;
  sdp_parser_t *sdp_parser_answer;
  sdp_session_t *sdp_session_offer;
  sdp_session_t *sdp_session_answer;

  su_home_init (&home);

  g_assert_cmpint (calls_sdp_crypto_context_get_state (ctx_offer), ==,
                   CALLS_CRYPTO_CONTEXT_STATE_INIT);
  g_assert_cmpint (calls_sdp_crypto_context_get_state (ctx_answer), ==,
                   CALLS_CRYPTO_CONTEXT_STATE_INIT);

  calls_sdp_crypto_context_generate_offer (ctx_offer);
  g_assert_cmpint (calls_sdp_crypto_context_get_state (ctx_offer), ==,
                   CALLS_CRYPTO_CONTEXT_STATE_OFFER_LOCAL);

  attr_offer_list =
    calls_sdp_crypto_context_get_crypto_candidates (ctx_offer, FALSE);
  attr_offer = attr_offer_list->data;
  attr_offer_str = calls_srtp_print_sdp_crypto_attribute (attr_offer, NULL);

  g_assert_true (attr_offer_str);

  media_line_offer = g_strdup_printf ("m=audio 42024 RTP/SAVP 0\r\n%s",
                                      attr_offer_str);
  sdp_parser_offer = sdp_parse (&home, media_line_offer, strlen(media_line_offer), sdp_f_config);
  sdp_session_offer = sdp_session (sdp_parser_offer);

  calls_sdp_crypto_context_set_remote_media (ctx_answer,
                                             sdp_session_offer->sdp_media);

  g_assert_cmpint (calls_sdp_crypto_context_get_state (ctx_answer), ==,
                   CALLS_CRYPTO_CONTEXT_STATE_OFFER_REMOTE);

  calls_sdp_crypto_context_generate_answer (ctx_answer);

  g_assert_cmpint (calls_sdp_crypto_context_get_state (ctx_answer), ==,
                   CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_SUCCESS);
  g_assert_true (calls_sdp_crypto_context_get_is_negotiated (ctx_answer));

  attr_answer = calls_sdp_crypto_context_get_remote_crypto (ctx_answer);
  attr_answer_str = calls_srtp_print_sdp_crypto_attribute (attr_answer, NULL);

  g_assert_true (attr_answer_str);

  media_line_answer = g_strdup_printf ("m=audio 42124 RTP/SAVP 0\r\n%s",
                                       attr_answer_str);
  sdp_parser_answer = sdp_parse (&home, media_line_answer, strlen(media_line_answer), sdp_f_config);
  sdp_session_answer = sdp_session (sdp_parser_answer);

  calls_sdp_crypto_context_set_remote_media (ctx_offer,
                                             sdp_session_answer->sdp_media);

  g_assert_cmpint (calls_sdp_crypto_context_get_state (ctx_offer), ==,
                   CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_SUCCESS);
  g_assert_true (calls_sdp_crypto_context_get_is_negotiated (ctx_offer));

  su_home_deinit (&home);
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Calls/SDP_crypto/manual", test_crypto_manual);
  g_test_add_func ("/Calls/SDP_crypto/offer_answer", test_crypto_offer_answer);

  return g_test_run ();
}
