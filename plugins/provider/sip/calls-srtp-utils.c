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

#include "calls-srtp-utils.h"
#include "calls-util.h"

#include <gio/gio.h>
#include <sys/random.h>

/**
 * SECTION:srtp-utils
 * @short_description: SRTP utilities for SDP parsing
 * @Title: CallsSrtpUtils
 *
 * Utilities for parsing and generating the crypto attribute
 * in SDP for SRTP use based on RFC 4568.
 *
 * Note that limitations of libsrtp are taken into account when checking validity
 * of the parsed attribute. These are:
 * A maximum of 16 keys,
 * key derivation rate must be 0,
 * lifetimes other than 2^48 (we actually ignore the specified lifetimes)
 */


/* The default used in libsrtp. No API to change this. See https://github.com/cisco/libsrtp/issues/588 */
#define SRTP_DEFAULT_LIFETIME_POW2 48
#define SRTP_MAX_LIFETIME_POW2 48

/* The default used in libsrtp (and GstSrtpEnc/GstSrtpDec) */
#define SRTP_DEFAULT_WINDOW_SIZE 128

static gsize
get_key_size_for_suite (calls_srtp_crypto_suite suite)
{
  switch (suite) {
  case CALLS_SRTP_SUITE_AES_CM_128_SHA1_32:
  case CALLS_SRTP_SUITE_AES_CM_128_SHA1_80:
    return 30;
  case CALLS_SRTP_SUITE_AES_192_CM_SHA1_32:
  case CALLS_SRTP_SUITE_AES_192_CM_SHA1_80:
    return 38;
  case CALLS_SRTP_SUITE_AES_256_CM_SHA1_32:
  case CALLS_SRTP_SUITE_AES_256_CM_SHA1_80:
    return 46;
  case CALLS_SRTP_SUITE_AEAD_AES_128_GCM:
    return 28;
  case CALLS_SRTP_SUITE_AEAD_AES_256_GCM:
    return 44;

  case CALLS_SRTP_SUITE_UNKNOWN:
  default:
    return 0;
  }
}


static gboolean
validate_crypto_attribute (calls_srtp_crypto_attribute *attr,
                           GError                     **error)
{
  guint expected_key_salt_length;
  gboolean need_mki;
  guint expected_mki_length = 0;
  calls_srtp_crypto_key_param *key_param;
  GSList *mki_list = NULL; /* for checking uniqueness of MKIs */
  GSList *key_list = NULL; /* for checking uniqueness of keys */

  if (!attr) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Attribute is NULL");
    return FALSE;
  }

  if (attr->tag <= 0 || attr->tag >= 1000000000) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Tag is not valid: %d", attr->tag);
    return FALSE;
  }

  expected_key_salt_length = get_key_size_for_suite (attr->crypto_suite);
  if (expected_key_salt_length == 0) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Crypto suite unknown");
    return FALSE;
  }

  /* at least one and a maximum of 16 key parameters */
  if (attr->n_key_params == 0 || attr->n_key_params > 16) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Must have between 1 and 16 keys, got %d", attr->n_key_params);
    return FALSE;
  }

  need_mki = attr->n_key_params > 1 ||
             attr->key_params[0].mki ||
             attr->key_params[0].mki_length;

  if (need_mki) {
    expected_mki_length = attr->key_params[0].mki_length;
    if (expected_mki_length == 0 ||
        expected_mki_length > 128) {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "MKI length must be between 1 and 128, got %u",
                   attr->key_params[0].mki_length);
      return FALSE;
    }
  }

  for (guint i = 0; i < attr->n_key_params; i++) {
    g_autofree guchar *key_salt = NULL;
    gsize key_salt_length;

    key_param = &attr->key_params[i];

    /* must have a key */
    if (!key_param->b64_keysalt) {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "No key found in parameter %d", i);
      goto failed;
    }

    key_salt = g_base64_decode (key_param->b64_keysalt, &key_salt_length);

    /* key must have length consistent with suite */
    if (key_salt_length != expected_key_salt_length) {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Key %d has length %" G_GSIZE_FORMAT ", but expected %d",
                   i, key_salt_length, expected_key_salt_length);
      goto failed;
    }

    /* key must be unique */
    if (g_slist_find_custom (key_list,
                             key_param->b64_keysalt,
                             (GCompareFunc) g_strcmp0)) {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Key %d is not unique: %s", i, key_param->b64_keysalt);
      goto failed;
    }

    key_list = g_slist_append (key_list, key_param->b64_keysalt);

    /* lifetime in range */
    if (key_param->lifetime_type == CALLS_SRTP_LIFETIME_AS_DECIMAL_NUMBER &&
        key_param->lifetime >= (1ULL << SRTP_MAX_LIFETIME_POW2)) {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Lifetime of key %d out of bounds: Got %" G_GINT64_FORMAT " but maximum is 2^%d",
                   i, key_param->lifetime, SRTP_MAX_LIFETIME_POW2);
      goto failed;
    }

    if (key_param->lifetime_type == CALLS_SRTP_LIFETIME_AS_POWER_OF_TWO &&
        key_param->lifetime >= SRTP_MAX_LIFETIME_POW2) {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Lifetime of key %d out of bounds: Got 2^%" G_GINT64_FORMAT " but maximum is 2^%d",
                   i, key_param->lifetime, SRTP_MAX_LIFETIME_POW2);
      goto failed;
    }

    /* if MKI length is set, it must be the same for all key parameters */
    if (need_mki) {
      if (key_param->mki_length != expected_mki_length) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "MKI length must be the same for all keys. Key %d has length %d but expected %d",
                     i, key_param->mki_length, expected_mki_length);
        goto failed;
      }

      /* MKI must not have leading zero */
      if (key_param->mki == 0) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "No MKI set for key %d", i);
        goto failed;
      }

      /* MKI must be unique */
      if (g_slist_find (mki_list, GINT_TO_POINTER (key_param->mki))) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "MKI for key %d is not unique", i);
        goto failed;
      }

      mki_list = g_slist_append (mki_list, GINT_TO_POINTER (key_param->mki));
    }

  }

  /* check session parameters */

  /* libsrtp does only support kdr=0 */
  if (attr->kdr != 0) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Key derivation rate must be 0, got %d",
                 attr->kdr);
    goto failed;
  }

  g_slist_free (mki_list);
  g_slist_free (key_list);

  return TRUE;

failed:

  g_slist_free (mki_list);
  g_slist_free (key_list);

  return FALSE;
}

/**
 * calls_srtp_generate_key_salt:
 * @length: Desired length of random data
 *
 * Generate random data to be used as master key and master salt of desired length @length
 *
 * Returns: (transfer full): Random data to be used as key and salt in SRTP
 * or %NULL if failed. Free with g_free() when done.
 */
guchar *
calls_srtp_generate_key_salt (gsize length)
{
  g_autofree guchar *key_salt = NULL;
  gsize n_bytes;

  g_return_val_if_fail (length > 0, NULL);

  key_salt = g_malloc (length);

  n_bytes = getrandom (key_salt, length, GRND_NONBLOCK);
  if (n_bytes == -1) {
    return NULL;
  }

  return g_steal_pointer (&key_salt);
}


/**
 * calls_srtp_generate_key_salt_for_suite:
 * @suite: a #calls_srtp_crypto_suite
 *
 * Generate random data to be used as master key and master salt.
 * The required length is determined by the requirements of the @suite
 *
 * Returns: (transfer full): Random data to be used as key and salt in SRTP
 * or %NULL if failed. Free with g_free() when done.
 */
guchar *
calls_srtp_generate_key_salt_for_suite (calls_srtp_crypto_suite suite)
{
  gsize size = get_key_size_for_suite (suite);

  if (size == 0)
    return NULL;

  return calls_srtp_generate_key_salt (size);
}

/**
 * calls_srtp_parse_sdp_crypto_attribute:
 * @attribute: attribute line
 * @error: a #GError
 *
 * Parse textual attribute line into structured data.
 *
 * Returns: (transfer full): A #calls_srtp_crypto_attribute containing
 * parsed attribute data, or %NULL if parsing failed.
 */
calls_srtp_crypto_attribute *
calls_srtp_parse_sdp_crypto_attribute (const char *attribute,
                                       GError    **error)
{
  g_auto (GStrv) attr_fields = NULL;
  g_auto (GStrv) key_info_strv = NULL;
  guint n_attr_fields;
  guint n_key_params;
  char *tag_str;
  gint tag;
  calls_srtp_crypto_attribute *attr;
  calls_srtp_crypto_suite crypto_suite;
  gboolean need_mki;
  gboolean attr_invalid = FALSE;
  g_autofree char *err_msg = NULL;

  if (STR_IS_NULL_OR_EMPTY (attribute)) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Cannot parse null or empty strings");
    return NULL;
  }

  if (!g_str_has_prefix (attribute, "a=crypto:")) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Does not look like a SDP crypto attribute: %s",
                 attribute);

    return NULL;
  }

  attr_fields = g_strsplit (attribute, " ", -1);
  n_attr_fields = g_strv_length (attr_fields);

  if (n_attr_fields < 3) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Need at least three fields in a SDP crypto attribute: %s",
                 attribute);

    return NULL;
  }

  tag_str = &attr_fields[0][9]; /* 9 is the length of "a=crypto:" */

  /* leading zeros MUST NOT be used */
  if (*tag_str == '0') {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Tag must not have a leading zero: %s",
                 tag_str);

    return NULL;
  }

  tag = (int) strtol (tag_str, NULL, 10);
  if (tag == 0) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Tag set to 0: %s", tag_str);

    return NULL;
  }

  if (tag < 0) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Tag must be positive: %s", tag_str);

    return NULL;
  }

  if (tag >= 1000000000) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Tag must have a maximum of 9 digits: %s", tag_str);

    return NULL;
  }

  if (g_strcmp0 (attr_fields[1], "AES_CM_128_HMAC_SHA1_32") == 0)
    crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_32;
  else if (g_strcmp0 (attr_fields[1], "AES_CM_128_HMAC_SHA1_80") == 0)
    crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_80;
  else if (g_strcmp0 (attr_fields[1], "AES_256_CM_HMAC_SHA1_32") == 0)
    crypto_suite = CALLS_SRTP_SUITE_AES_256_CM_SHA1_32;
  else if (g_strcmp0 (attr_fields[1], "AES_256_CM_HMAC_SHA1_80") == 0)
    crypto_suite = CALLS_SRTP_SUITE_AES_256_CM_SHA1_80;
  else
    crypto_suite = CALLS_SRTP_SUITE_UNKNOWN; /* error */

  /* key infos are split by ';' */
  key_info_strv = g_strsplit (attr_fields[2], ";", -1);
  n_key_params = g_strv_length (key_info_strv);

  /* libsrtp supports a maximum of 16 master keys */
  if (n_key_params > 16) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "More than 16 keys are not supported by libsrtp");

    return NULL;
  }

  need_mki = n_key_params > 1;

  attr = calls_srtp_crypto_attribute_new (n_key_params);

  attr->tag = tag;
  attr->crypto_suite = crypto_suite;

  for (guint i = 0; i < n_key_params; i++) {
    char *key_info; /* srtp-key-info       = key-salt ["|" lifetime] ["|" mki] */
    g_auto (GStrv) key_info_fields = NULL;
    guint n_key_infos;
    guint key_info_lifetime_index;
    guint key_info_mki_index;
    calls_srtp_crypto_key_param *key_param = &attr->key_params[i];

    if (!g_str_has_prefix (key_info_strv[i], "inline:")) {
      attr_invalid = TRUE;
      err_msg = g_strdup_printf ("Key method not 'inline': %s", key_info_strv[i]);
      break;
    }

    key_info = &key_info_strv[i][7]; /* 7 is the length of "inline:" */
    key_info_fields = g_strsplit (key_info, "|", -1);

    n_key_infos = g_strv_length (key_info_fields);

    switch (strlen (key_info_fields[0]) % 4) {
    case 3:
      key_param->b64_keysalt = g_strconcat (key_info_fields[0], "=", NULL);
      break;
    case 2:
      key_param->b64_keysalt = g_strconcat (key_info_fields[0], "==", NULL);
      break;
    case 1:
      g_assert_not_reached (); /* impossible with base64 */
      break;
    case 0:
    default:
      key_param->b64_keysalt = g_strdup (key_info_fields[0]);
    }

    if (n_key_infos == 1) {
      key_info_lifetime_index = 0;
      key_info_mki_index = 0;
    } else if (n_key_infos == 2) {
      /* either MKI or lifetime */
      if (g_strstr_len (key_info_fields[1], -1, ":")) {
        key_info_lifetime_index = 0;
        key_info_mki_index = 1;
      } else {
        key_info_lifetime_index = 1;
        key_info_mki_index = 0;
      }
    } else if (n_key_infos == 3) {
      key_info_lifetime_index = 1;
      key_info_mki_index = 2;
    } else {
      /* invalid */
      attr_invalid = TRUE;
      err_msg = g_strdup_printf ("Unexpected number of key-info fields: %s", key_info);
      break;
    }

    /* lifetime type */
    if (key_info_lifetime_index) {
      char *lifetime_number;
      char *endptr;
      if (g_str_has_prefix (key_info_fields[key_info_lifetime_index], "2^")) {
        key_param->lifetime_type = CALLS_SRTP_LIFETIME_AS_POWER_OF_TWO;
        lifetime_number = &key_info_fields[key_info_lifetime_index][2]; /* 2 is the length of "2^" */
      } else {
        key_param->lifetime_type = CALLS_SRTP_LIFETIME_AS_DECIMAL_NUMBER;
        lifetime_number = key_info_fields[key_info_lifetime_index];
      }

      if (*lifetime_number == '0') {
        attr_invalid = TRUE;
        err_msg = g_strdup_printf ("Leading zero in lifetime: %s",
                                   key_info_fields[key_info_lifetime_index]);
        break;
      }

      key_param->lifetime = g_ascii_strtoull (lifetime_number, &endptr, 10);
      if (key_param->lifetime == 0) {
        attr_invalid = TRUE;
        err_msg = g_strdup_printf ("Lifetime set to zero: %s",
                                   key_info_fields[key_info_lifetime_index]);
        break;
      }

      if (*endptr != '\0') {
        attr_invalid = TRUE;
        err_msg = g_strdup_printf ("Non numeric characters in lifetime: %s",
                                   key_info_fields[key_info_lifetime_index]);
        break;
      }

      /* out of bounds check will be performed during validation of the attribute */
    }

    if (need_mki && key_info_mki_index == 0) {
      attr_invalid = TRUE;
      err_msg = g_strdup_printf ("MKI needed, but not found: %s", key_info);
      break;
    }

    if (need_mki) {
      g_auto (GStrv) mki_split = g_strsplit (key_info_fields[key_info_mki_index], ":", -1);
      guint n_mki = g_strv_length (mki_split);
      guint64 mki;
      guint64 mki_length;
      char *endptr;

      if (n_mki != 2) {
        attr_invalid = TRUE;
        err_msg = g_strdup_printf ("MKI field not separated into two fields by colon: %s",
                                   key_info_fields[key_info_mki_index]);
        break;
      }

      /* no leading zero allowed */
      if (*mki_split[0] == '0') {
        attr_invalid = TRUE;
        err_msg = g_strdup_printf ("Leading zero in MKI: %s", mki_split[0]);
        break;
      }

      if (*mki_split[1] == '0') {
        attr_invalid = TRUE;
        err_msg = g_strdup_printf ("Leading zero in MKI length: %s", mki_split[1]);
        break;
      }

      mki = g_ascii_strtoull (mki_split[0], &endptr, 10);
      if (mki == 0) {
        attr_invalid = TRUE;
        err_msg = g_strdup_printf ("MKI set to 0: %s", mki_split[0]);
        break;
      }

      if (*endptr != '\0') {
        attr_invalid = TRUE;
        err_msg = g_strdup_printf ("Non numeric characters found in MKI: %s", mki_split[0]);
        break;
      }

      mki_length = g_ascii_strtoull (mki_split[1], &endptr, 10);
      /* number of bytes of the MKI field in the SRTP packet */
      if (mki_length == 0 || mki_length > 128) {
        attr_invalid = TRUE;
        err_msg = g_strdup_printf ("MKI length not between 0 and 128: %s", mki_split[1]);
        break;
      }

      if (*endptr != '\0') {
        attr_invalid = TRUE;
        err_msg = g_strdup_printf ("Non numeric characters found in MKI length: %s", mki_split[1]);
        break;
      }

      key_param->mki = mki;
      key_param->mki_length = (guint) mki_length;
    }

  }

  if (attr_invalid) {
    calls_srtp_crypto_attribute_free (attr);
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, err_msg);

    return NULL;
  }

  /* TODO session parameters */

  if (!validate_crypto_attribute (attr, error)) {
    calls_srtp_crypto_attribute_free (attr);

    return NULL;

  }

  return attr;
}

/**
 * calls_srtp_print_sdp_crypto_attribute:
 * @attr: Structured crypto attribute
 * @error: A #GError
 *
 * Returns: (transfer full): Textual representation of crypto attribute
 * or %NULL if attribute contains invalid data.
 */
char *
calls_srtp_print_sdp_crypto_attribute (calls_srtp_crypto_attribute *attr,
                                       GError                     **error)
{
  const char *crypto_suite;
  GString *attr_str;

  if (!validate_crypto_attribute (attr, error))
    return NULL;

  if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_CM_128_SHA1_32)
    crypto_suite = "AES_CM_128_HMAC_SHA1_32";
  else if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_CM_128_SHA1_80)
    crypto_suite = "AES_CM_128_HMAC_SHA1_80";
  else if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_192_CM_SHA1_32)
    crypto_suite = "AES_196_CM_HMAC_SHA1_32";
  else if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_192_CM_SHA1_80)
    crypto_suite = "AES_196_CM_HMAC_SHA1_80";
  else if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_256_CM_SHA1_32)
    crypto_suite = "AES_256_CM_HMAC_SHA1_32";
  else if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_256_CM_SHA1_80)
    crypto_suite = "AES_256_CM_HMAC_SHA1_80";
  else if (attr->crypto_suite == CALLS_SRTP_SUITE_F8_128_HMAC_SHA1_32)
    crypto_suite = "F8_128_HMAC_SHA1_80";
  else if (attr->crypto_suite == CALLS_SRTP_SUITE_AEAD_AES_128_GCM)
    crypto_suite = "AEAD_AES_128_GCM";
  else if (attr->crypto_suite == CALLS_SRTP_SUITE_AEAD_AES_256_GCM)
    crypto_suite = "AEAD_AES_256_GCM";
  else
    return NULL;

  attr_str = g_string_sized_new (96); /* minimal string length is 82 */

  g_string_append_printf (attr_str, "a=crypto:%d %s ",
                          attr->tag, crypto_suite);

  /* key parameters */
  for (guint i = 0; i < attr->n_key_params; i++) {
    calls_srtp_crypto_key_param *key_param = &attr->key_params[i];
    int keylen = strlen (key_param->b64_keysalt);

    /* https://www.rfc-editor.org/rfc/rfc4568.html#section-6.1 says:
      When base64 decoding the key and salt, padding characters (i.e.,
      one or two "=" at the end of the base64-encoded data) are discarded
      (see [RFC3548] for details).
    */
    if (key_param->b64_keysalt[keylen - 2] == '=')
      g_string_append_printf (attr_str, "inline:%.*s", keylen - 2, key_param->b64_keysalt);
    else if (key_param->b64_keysalt[keylen - 1] == '=')
      g_string_append_printf (attr_str, "inline:%.*s", keylen - 1, key_param->b64_keysalt);
    else
      g_string_append_printf (attr_str, "inline:%s", key_param->b64_keysalt);
    if (key_param->lifetime_type == CALLS_SRTP_LIFETIME_AS_DECIMAL_NUMBER)
      g_string_append_printf (attr_str, "|%" G_GINT64_FORMAT, key_param->lifetime);
    if (key_param->lifetime_type == CALLS_SRTP_LIFETIME_AS_POWER_OF_TWO)
      g_string_append_printf (attr_str, "|2^%" G_GINT64_FORMAT, key_param->lifetime);

    if (key_param->mki > 0) {
      g_string_append_printf(attr_str, "|%" G_GUINT64_FORMAT ":%u",
                             key_param->mki, key_param->mki_length);
    }

    if (i + 1 < attr->n_key_params)
      g_string_append (attr_str, ";");
  }

  /* TODO session parameters */

  return g_string_free (attr_str, FALSE);
}

/**
 * calls_srtp_crypto_attribute_new:
 * @n_key_params: The number of key parameters
 *
 * Returns: (transfer full): A new empty #calls_srtp_crypto_attribute
 * with @n_key_params key parameters allocated. Key parameters must be set either
 * manually or by using calls_srtp_crypto_attribute_init_keys().
 *
 * Free the attribute with calls_srtp_crypto_attribute_free() after you are done.
 */
calls_srtp_crypto_attribute *
calls_srtp_crypto_attribute_new (guint n_key_params)
{
  calls_srtp_crypto_attribute *attr;

  g_return_val_if_fail (n_key_params > 0 || n_key_params < 16, NULL);

  attr = g_new0 (calls_srtp_crypto_attribute, 1);
  attr->key_params = g_new0 (calls_srtp_crypto_key_param, n_key_params);
  attr->n_key_params = n_key_params;

  return attr;
}

/**
 * calls_srtp_crypto_attribute_init_keys:
 * @attr: A #calls_srtp_crypto_attribute
 *
 * Generate key material and set sane default parameters for each
 * key parameter.
 */
gboolean
calls_srtp_crypto_attribute_init_keys (calls_srtp_crypto_attribute *attr)
{
  gsize key_size;
  gboolean need_mki;

  g_return_val_if_fail (attr, FALSE);

  key_size = get_key_size_for_suite (attr->crypto_suite);
  if (key_size == 0)
    return FALSE;

  need_mki = attr->n_key_params > 1;

  for (uint i = 0; i < attr->n_key_params; i++) {
    g_autofree guchar *key = calls_srtp_generate_key_salt (key_size);
    g_free (attr->key_params[i].b64_keysalt);
    attr->key_params[i].b64_keysalt = g_base64_encode (key, key_size);

    if (need_mki) {
      attr->key_params[i].mki = i + 1;
      attr->key_params[i].mki_length = 4;
    }
  }

  return TRUE;
}

/**
 * calls_srtp_crypto_attribute_free:
 * @attr: A #calls_srtp_crypto_attribute
 *
 * Frees all memory allocated for @attr.
 */
void
calls_srtp_crypto_attribute_free (calls_srtp_crypto_attribute *attr)
{
  for (guint i = 0; i < attr->n_key_params; i++) {
    g_free (attr->key_params[i].b64_keysalt);
  }

  g_free (attr->key_params);
  g_free (attr->b64_fec_key);
  g_free (attr);
}

/**
 * calls_srtp_crypto_get_srtpdec_params:
 * @attr: A #calls_srtp_crypto_attribute
 * @srtp_cipher (out): SRTP cipher
 * @srtp_auth (out): SRTP auth
 * @srtcp_cipher (out): SRTCP cipher
 * @srtcp_auth (out): SRTCP auth
 *
 * Sets the parameters suitable for #GstSrtpDec (as a #GstCaps).
 */
gboolean
calls_srtp_crypto_get_srtpdec_params (calls_srtp_crypto_attribute *attr,
                                      const char                 **srtp_cipher,
                                      const char                 **srtp_auth,
                                      const char                 **srtcp_cipher,
                                      const char                 **srtcp_auth)
{
  g_return_val_if_fail (attr, FALSE);

  if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_CM_128_SHA1_32) {
    *srtp_cipher = attr->unencrypted_srtp ? "null" : "aes-128-icm";
    *srtp_auth = attr->unauthenticated_srtp ? "null" : "hmac-sha1-32";
    *srtcp_cipher = attr->unencrypted_srtcp ? "null" : "aes-128-icm";
    *srtcp_auth = attr->unencrypted_srtcp ? "null" : "hmac-sha1-32";

    return TRUE;
  } else if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_CM_128_SHA1_80) {
    *srtp_cipher = attr->unencrypted_srtp ? "null" : "aes-128-icm";
    *srtp_auth = attr->unauthenticated_srtp ? "null" : "hmac-sha1-80";
    *srtcp_cipher = attr->unencrypted_srtcp ? "null" : "aes-128-icm";
    *srtcp_auth = attr->unencrypted_srtcp ? "null" : "hmac-sha1-80";

    return TRUE;
  }
  if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_192_CM_SHA1_32) {
    /* NOT OFFERED BY GSTREAMER
    *srtp_cipher = attr->unencrypted_srtp ? "null" : "aes-192-icm";
    *srtp_auth = attr->unauthenticated_srtp ? "null" : "hmac-sha1-32";
    *srtcp_cipher = attr->unencrypted_srtcp ? "null" : "aes-192-icm";
    *srtcp_auth = attr->unencrypted_srtcp ? "null" : "hmac-sha1-32";
    */
    return FALSE;
  }
  if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_192_CM_SHA1_80) {
    /* NOT OFFERED BY GSTREAMER
    *srtp_cipher = attr->unencrypted_srtp ? "null" : "aes-192-icm";
    *srtp_auth = attr->unauthenticated_srtp ? "null" : "hmac-sha1-80";
    *srtcp_cipher = attr->unencrypted_srtcp ? "null" : "aes-192-icm";
    *srtcp_auth = attr->unencrypted_srtcp ? "null" : "hmac-sha1-80";
    */
    return FALSE;
  }
  if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_256_CM_SHA1_32) {
    *srtp_cipher = attr->unencrypted_srtp ? "null" : "aes-256-icm";
    *srtp_auth = attr->unauthenticated_srtp ? "null" : "hmac-sha1-32";
    *srtcp_cipher = attr->unencrypted_srtcp ? "null" : "aes-256-icm";
    *srtcp_auth = attr->unencrypted_srtcp ? "null" : "hmac-sha1-32";

    return TRUE;
  }
  if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_256_CM_SHA1_80) {
    *srtp_cipher = attr->unencrypted_srtp ? "null" : "aes-256-icm";
    *srtp_auth = attr->unauthenticated_srtp ? "null" : "hmac-sha1-80";
    *srtcp_cipher = attr->unencrypted_srtcp ? "null" : "aes-256-icm";
    *srtcp_auth = attr->unencrypted_srtcp ? "null" : "hmac-sha1-80";

    return TRUE;
  }
  if (attr->crypto_suite == CALLS_SRTP_SUITE_F8_128_HMAC_SHA1_32) {
    // F8 IS NOT OFFERED BY GSTREAMER
    return FALSE;
  }
  if (attr->crypto_suite == CALLS_SRTP_SUITE_AEAD_AES_128_GCM) {

    return FALSE;
  }
  if (attr->crypto_suite == CALLS_SRTP_SUITE_AEAD_AES_256_GCM) {

    return FALSE;
  }

  return FALSE;
}


/**
 * calls_srtp_crypto_get_srtpenc_params:
 * @attr: A #calls_srtp_crypto_attribute
 * @srtp_cipher (out): SRTP cipher
 * @srtp_auth (out): SRTP auth
 * @srtcp_cipher (out): SRTCP cipher
 * @srtcp_auth (out): SRTCP auth
 *
 * Sets the parameters suitable for #GstSrtpDec (as a #GstCaps).
 */
gboolean
calls_srtp_crypto_get_srtpenc_params (calls_srtp_crypto_attribute *attr,
                                      GstSrtpCipherType           *srtp_cipher,
                                      GstSrtpAuthType             *srtp_auth,
                                      GstSrtpCipherType           *srtcp_cipher,
                                      GstSrtpAuthType             *srtcp_auth)
{
  g_return_val_if_fail (attr, FALSE);

  if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_CM_128_SHA1_32) {
    *srtp_cipher = attr->unencrypted_srtp ? GST_SRTP_CIPHER_NULL : GST_SRTP_CIPHER_AES_128_ICM;
    *srtp_auth = attr->unauthenticated_srtp ? GST_SRTP_AUTH_NULL : GST_SRTP_AUTH_HMAC_SHA1_32;
    *srtcp_cipher = attr->unencrypted_srtcp ? GST_SRTP_CIPHER_NULL : GST_SRTP_CIPHER_AES_128_ICM;
    *srtcp_auth = attr->unencrypted_srtcp ? GST_SRTP_AUTH_NULL : GST_SRTP_AUTH_HMAC_SHA1_32;

    return TRUE;
  } else if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_CM_128_SHA1_80) {

    *srtp_cipher = attr->unencrypted_srtp ? GST_SRTP_CIPHER_NULL : GST_SRTP_CIPHER_AES_128_ICM;
    *srtp_auth = attr->unauthenticated_srtp ? GST_SRTP_AUTH_NULL : GST_SRTP_AUTH_HMAC_SHA1_80;
    *srtcp_cipher = attr->unencrypted_srtcp ? GST_SRTP_CIPHER_NULL : GST_SRTP_CIPHER_AES_128_ICM;
    *srtcp_auth = attr->unencrypted_srtcp ? GST_SRTP_AUTH_NULL : GST_SRTP_AUTH_HMAC_SHA1_80;

    return TRUE;
  }
  if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_256_CM_SHA1_32) {
    *srtp_cipher = attr->unencrypted_srtp ? GST_SRTP_CIPHER_NULL : GST_SRTP_CIPHER_AES_256_ICM;
    *srtp_auth = attr->unauthenticated_srtp ? GST_SRTP_AUTH_NULL : GST_SRTP_AUTH_HMAC_SHA1_32;
    *srtcp_cipher = attr->unencrypted_srtcp ? GST_SRTP_CIPHER_NULL : GST_SRTP_CIPHER_AES_256_ICM;
    *srtcp_auth = attr->unencrypted_srtcp ? GST_SRTP_AUTH_NULL : GST_SRTP_AUTH_HMAC_SHA1_32;

    return TRUE;
  }
  if (attr->crypto_suite == CALLS_SRTP_SUITE_AES_256_CM_SHA1_80) {
    *srtp_cipher = attr->unencrypted_srtp ? GST_SRTP_CIPHER_NULL : GST_SRTP_CIPHER_AES_256_ICM;
    *srtp_auth = attr->unauthenticated_srtp ? GST_SRTP_AUTH_NULL : GST_SRTP_AUTH_HMAC_SHA1_80;
    *srtcp_cipher = attr->unencrypted_srtcp ? GST_SRTP_CIPHER_NULL : GST_SRTP_CIPHER_AES_256_ICM;
    *srtcp_auth = attr->unencrypted_srtcp ? GST_SRTP_AUTH_NULL : GST_SRTP_AUTH_HMAC_SHA1_80;

    return TRUE;
  }

  return FALSE;
}
