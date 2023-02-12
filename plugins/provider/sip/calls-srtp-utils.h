/*
 * Copyright (C) 2022 Purism SPC
 *
 * This file is part of Calls.
 *
 * Calls is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Calls is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Calls.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  CALLS_SRTP_SUITE_UNKNOWN = 0,
  CALLS_SRTP_SUITE_AES_CM_128_SHA1_32, /* RFC 4568 */
  CALLS_SRTP_SUITE_AES_CM_128_SHA1_80, /* RFC 4568 */
  CALLS_SRTP_SUITE_AES_192_CM_SHA1_32, /* RFC 6188 not supperted by Gst */
  CALLS_SRTP_SUITE_AES_192_CM_SHA1_80, /* RFC 6188 not supperted by Gst */
  CALLS_SRTP_SUITE_AES_256_CM_SHA1_32, /* RFC 6188 */
  CALLS_SRTP_SUITE_AES_256_CM_SHA1_80, /* RFC 6188 */
  CALLS_SRTP_SUITE_F8_128_HMAC_SHA1_32, /* RFC 4568 but not supported by GstSrtpEnc/GstSrtpDec */
  CALLS_SRTP_SUITE_AEAD_AES_128_GCM, /* RFC 7714 TODO support in the future */
  CALLS_SRTP_SUITE_AEAD_AES_256_GCM  /* RFC 7714 TODO support in the future */
} calls_srtp_crypto_suite;


typedef enum {
  CALLS_SRTP_FEC_ORDER_UNSET = 0,
  CALLS_SRTP_FEC_ORDER_FEC_SRTP,
  CALLS_SRTP_FEC_ORDER_SRTP_FEC
} calls_srtp_fec_order;


typedef enum {
  CALLS_SRTP_LIFETIME_UNSET = 0,
  CALLS_SRTP_LIFETIME_AS_DECIMAL_NUMBER,
  CALLS_SRTP_LIFETIME_AS_POWER_OF_TWO
} calls_srtp_lifetime_type;


/* from GStreamer */
typedef enum {
  GST_SRTP_CIPHER_NULL,
  GST_SRTP_CIPHER_AES_128_ICM,
  GST_SRTP_CIPHER_AES_256_ICM,
  GST_SRTP_CIPHER_AES_128_GCM,
  GST_SRTP_CIPHER_AES_256_GCM
} GstSrtpCipherType;


typedef enum {
  GST_SRTP_AUTH_NULL,
  GST_SRTP_AUTH_HMAC_SHA1_32,
  GST_SRTP_AUTH_HMAC_SHA1_80
} GstSrtpAuthType;


typedef struct {
  char                    *b64_keysalt;
  calls_srtp_lifetime_type lifetime_type;
  /* maximum lifetime: SRTP 2^48 packets; SRTCP 2^31 packets; libsrtp uses a hardcoded limit of 2^48 */
  guint64                  lifetime;

  guint64                  mki;
  guint                    mki_length;
} calls_srtp_crypto_key_param;


typedef struct {
  gint                         tag;
  calls_srtp_crypto_suite      crypto_suite;

  calls_srtp_crypto_key_param *key_params;
  guint                        n_key_params;

  /** session parameters
   * For more information see https://datatracker.ietf.org/doc/html/rfc4568#section-6.3
   * KDR (key derivation rate) defaults to 0; declarative parameter
   *  decimal integer in {1,2,...,24}, denotes a power of two. 0 means unspecified
   *  defaulting to a single initial key derivation
   * UNENCRYPTED_SRTCP, UNENCRYPTED_SRTP; negotiated parameter
   * UNAUTHENTICATED_SRTP; negotiated parameter
   * FEC_ORDER; declarative parameter
   * FEC_KEY separate key-params for forward error correction; declarative parameter
   * WSH (Window Size Hint); declarative parameter; MAY be ignored
   */
  gint                 kdr;
  gboolean             unencrypted_srtp;
  gboolean             unencrypted_srtcp;
  gboolean             unauthenticated_srtp;
  calls_srtp_fec_order fec_order; /* FEC in RTP: RFC2733 */
  char                *b64_fec_key; /* TODO this should also be an array of calls_srtp_crypto_key_param */
  guint                window_size_hint;
} calls_srtp_crypto_attribute;


guchar                      *calls_srtp_generate_key_salt              (gsize length);
guchar                      *calls_srtp_generate_key_salt_for_suite    (calls_srtp_crypto_suite suite);
calls_srtp_crypto_attribute *calls_srtp_parse_sdp_crypto_attribute     (const char *attr,
                                                                        GError    **error);
char                        *calls_srtp_print_sdp_crypto_attribute     (calls_srtp_crypto_attribute *attr,
                                                                        GError                     **error);
calls_srtp_crypto_attribute *calls_srtp_crypto_attribute_new           (guint n_key_params);
gboolean                     calls_srtp_crypto_attribute_init_keys     (calls_srtp_crypto_attribute *attr);
void                         calls_srtp_crypto_attribute_free          (calls_srtp_crypto_attribute *attr);
char                        *calls_srtp_generate_crypto_for_offer      (void);
gboolean                     calls_srtp_crypto_get_srtpdec_params      (calls_srtp_crypto_attribute *attr,
                                                                        const char                 **srtp_cipher,
                                                                        const char                 **srtp_auth,
                                                                        const char                 **srtcp_cipher,
                                                                        const char                 **srtcp_auth);
gboolean                     calls_srtp_crypto_get_srtpenc_params      (calls_srtp_crypto_attribute *attr,
                                                                        GstSrtpCipherType           *srtp_cipher,
                                                                        GstSrtpAuthType             *srtp_auth,
                                                                        GstSrtpCipherType           *srtcp_cipher,
                                                                        GstSrtpAuthType             *srtcp_auth);


G_DEFINE_AUTOPTR_CLEANUP_FUNC (calls_srtp_crypto_attribute, calls_srtp_crypto_attribute_free)


G_END_DECLS
