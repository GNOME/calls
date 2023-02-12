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

#include "calls-sdp-crypto-context.h"
#include "calls-sip-enums.h"

#include <sofia-sip/sdp.h>

enum {
  PROP_0,
  PROP_STATE,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

struct _CallsSdpCryptoContext {
  GObject                 parent_instance;

  GList                  *local_crypto_attributes;
  GList                  *remote_crypto_attributes;

  CallsCryptoContextState state;
  int                     negotiated_tag;
};

#if GLIB_CHECK_VERSION (2, 70, 0)
G_DEFINE_FINAL_TYPE (CallsSdpCryptoContext, calls_sdp_crypto_context, G_TYPE_OBJECT)
#else
G_DEFINE_TYPE (CallsSdpCryptoContext, calls_sdp_crypto_context, G_TYPE_OBJECT)
#endif


static GStrv
get_all_crypto_attributes_strv (sdp_media_t *media)
{
#if GLIB_CHECK_VERSION (2, 68, 0)
  g_autoptr (GStrvBuilder) builder = NULL;

  g_assert (media);

  builder = g_strv_builder_new ();

  for (sdp_attribute_t *attr = media->m_attributes; attr; attr = attr->a_next) {
    g_autofree char *crypto_str = NULL;

    if (g_strcmp0 (attr->a_name, "crypto") != 0)
      continue;

    crypto_str = g_strconcat ("a=crypto:", attr->a_value, NULL);
    g_strv_builder_add (builder, crypto_str);
  }

  return g_strv_builder_end (builder);
#else
  /* implement a poor mans GStrv */
  g_autofree char *attribute_string = NULL;

  g_assert (media);

  for (sdp_attribute_t *attr = media->m_attributes; attr; attr = attr->a_next) {
    g_autofree char *crypto_str = NULL;

    if (g_strcmp0 (attr->a_name, "crypto") != 0)
      continue;

    crypto_str = g_strconcat ("a=crypto:", attr->a_value, NULL);
    if (!attribute_string) {
      attribute_string = g_strdup (crypto_str);
    } else {
      g_autofree char *tmp = attribute_string;
      attribute_string = g_strconcat (attribute_string, "\n", crypto_str, NULL);
    }
  }
  return g_strsplit (attribute_string, "\n", -1);
#endif
}


static gboolean
crypto_attribute_is_supported (CallsSdpCryptoContext       *self,
                               calls_srtp_crypto_attribute *attr)
{
  g_assert (attr);

  if (attr->crypto_suite == CALLS_SRTP_SUITE_UNKNOWN)
    return FALSE;

  /* TODO setup a policy mechanism, for now this is hardcoded */
  if (attr->unencrypted_srtp ||
      attr->unauthenticated_srtp ||
      attr->unencrypted_srtcp)
    return FALSE;

  return TRUE;
}

static calls_srtp_crypto_attribute *
get_crypto_attribute_by_tag (GList *attributes,
                             guint  tag)
{

  g_assert (attributes);
  g_assert (tag > 0);

  for (GList *node = attributes; node; node = node->next) {
    calls_srtp_crypto_attribute *attr = node->data;

    if (attr->tag == tag)
      return attr;
  }

  return NULL;
}


static void
set_state (CallsSdpCryptoContext  *self,
           CallsCryptoContextState state)
{
  g_assert (CALLS_IS_SDP_CRYPTO_CONTEXT (self));

  if (self->state == state)
    return;

  self->state = state;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
}


/* Returns if %TRUE if the state was updated, %FALSE otherwise */
static gboolean
update_state (CallsSdpCryptoContext *self)
{
  GList *tags_local = NULL;
  GList *tags_remote = NULL;
  gint negotiated_tag = -1;
  calls_srtp_crypto_attribute *local_crypto;
  calls_srtp_crypto_attribute *remote_crypto;


  g_assert (CALLS_IS_SDP_CRYPTO_CONTEXT (self));

  /* Cannot update final states */
  if (self->state == CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_FAILED ||
      self->state == CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_SUCCESS)
    return FALSE;

  if (self->state == CALLS_CRYPTO_CONTEXT_STATE_INIT) {
    if (self->local_crypto_attributes) {
      set_state (self, CALLS_CRYPTO_CONTEXT_STATE_OFFER_LOCAL);
      return TRUE;
    }
    if (self->remote_crypto_attributes) {
      set_state (self, CALLS_CRYPTO_CONTEXT_STATE_OFFER_REMOTE);
      return TRUE;
    }

    return FALSE;
  }

  for (GList *node = self->local_crypto_attributes; node; node = node->next) {
    calls_srtp_crypto_attribute *attr = node->data;
    tags_local = g_list_append (tags_local, GUINT_TO_POINTER (attr->tag));
  }

  for (GList *node = self->remote_crypto_attributes; node; node = node->next) {
    calls_srtp_crypto_attribute *attr = node->data;
    tags_remote = g_list_append (tags_remote, GUINT_TO_POINTER (attr->tag));
  }

  if (self->state == CALLS_CRYPTO_CONTEXT_STATE_OFFER_LOCAL) {
    for (GList *node = tags_local; node; node = node->next) {
      if (g_list_find (tags_remote, node->data)) {
        negotiated_tag = GPOINTER_TO_UINT (node->data);
        break;
      }
    }
  } else if (self->state == CALLS_CRYPTO_CONTEXT_STATE_OFFER_REMOTE) {
    for (GList *node = tags_remote; node; node = node->next) {
      if (g_list_find (tags_local, node->data)) {
        negotiated_tag = GPOINTER_TO_UINT (node->data);
        break;
      }
    }
  } else {
    g_assert_not_reached ();
  }

  g_list_free (tags_local);
  g_list_free (tags_remote);

  if (negotiated_tag == -1) {
    self->state = CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_FAILED;
    return TRUE;
  }

  local_crypto = get_crypto_attribute_by_tag (self->local_crypto_attributes,
                                              negotiated_tag);
  remote_crypto = get_crypto_attribute_by_tag (self->remote_crypto_attributes,
                                               negotiated_tag);

  if (local_crypto->crypto_suite != remote_crypto->crypto_suite) {
    set_state (self, CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_FAILED);
    return TRUE;
  }

  /* TODO check mandatory parameters and policy constrains */

  self->negotiated_tag = negotiated_tag;
  set_state (self, CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_SUCCESS);

  return TRUE;
}


static void
calls_sdp_crypto_context_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  CallsSdpCryptoContext *self = CALLS_SDP_CRYPTO_CONTEXT (object);

  switch (property_id) {
  case PROP_STATE:
    g_value_set_enum (value, self->state);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_sdp_crypto_context_dispose (GObject *object)
{
  CallsSdpCryptoContext *self = CALLS_SDP_CRYPTO_CONTEXT (object);

  g_clear_list (&self->local_crypto_attributes, (GDestroyNotify) calls_srtp_crypto_attribute_free);
  g_clear_list (&self->remote_crypto_attributes, (GDestroyNotify) calls_srtp_crypto_attribute_free);

  G_OBJECT_CLASS (calls_sdp_crypto_context_parent_class)->dispose (object);
}


static void
calls_sdp_crypto_context_class_init (CallsSdpCryptoContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = calls_sdp_crypto_context_dispose;
  object_class->get_property = calls_sdp_crypto_context_get_property;

  props[PROP_STATE] =
    g_param_spec_enum ("state",
                       "State",
                       "State of the crypto context",
                       CALLS_TYPE_CRYPTO_CONTEXT_STATE,
                       CALLS_CRYPTO_CONTEXT_STATE_INIT,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_sdp_crypto_context_init (CallsSdpCryptoContext *self)
{
}


CallsSdpCryptoContext *
calls_sdp_crypto_context_new (void)
{
  return g_object_new (CALLS_TYPE_SDP_CRYPTO_CONTEXT, NULL);
}


gboolean
calls_sdp_crypto_context_set_local_media (CallsSdpCryptoContext *self,
                                          sdp_media_t           *media)
{
  g_auto (GStrv) crypto_strv = NULL;
  guint n_crypto_attr;

  g_return_val_if_fail (CALLS_IS_SDP_CRYPTO_CONTEXT (self), FALSE);
  g_return_val_if_fail (media, FALSE);

  if (self->local_crypto_attributes) {
    g_warning ("Local crypto attributes already set");
    return FALSE;
  }

  crypto_strv = get_all_crypto_attributes_strv (media);
  n_crypto_attr = g_strv_length (crypto_strv);

  if (n_crypto_attr == 0) {
    g_warning ("No crypto attributes found in given SDP media");
    return FALSE;
  }

  for (guint i = 0; i < n_crypto_attr; i++) {
    g_autoptr (GError) error = NULL;
    calls_srtp_crypto_attribute *attr;

    attr = calls_srtp_parse_sdp_crypto_attribute (crypto_strv[i], &error);
    if (!attr) {
      g_warning ("Failed parsing crypto attribute '%s': %s",
                 crypto_strv[i], error->message);
      continue;
    }
    self->local_crypto_attributes =
      g_list_append (self->local_crypto_attributes, attr);
  }

  if (!self->local_crypto_attributes) {
    g_warning ("Could not parse a single crypto attribute, aborting");
    return FALSE;
  }

  return update_state (self);
}


gboolean
calls_sdp_crypto_context_set_remote_media (CallsSdpCryptoContext *self,
                                           sdp_media_t           *media)
{
  g_auto (GStrv) crypto_strv = NULL;

  guint n_crypto_attr;

  g_return_val_if_fail (CALLS_IS_SDP_CRYPTO_CONTEXT (self), FALSE);
  g_return_val_if_fail (media, FALSE);

  if (self->remote_crypto_attributes) {
    g_warning ("Remote crypto attributes already set");
    return FALSE;
  }

  crypto_strv = get_all_crypto_attributes_strv (media);
  n_crypto_attr = g_strv_length (crypto_strv);

  if (n_crypto_attr == 0) {
    g_warning ("No crypto attributes found in given SDP media");
    return FALSE;
  }

  for (guint i = 0; i < n_crypto_attr; i++) {
    g_autoptr (GError) error = NULL;
    calls_srtp_crypto_attribute *attr;

    attr = calls_srtp_parse_sdp_crypto_attribute (crypto_strv[i], &error);
    if (!attr) {
      g_warning ("Failed parsing crypto attribute '%s': %s",
                 crypto_strv[i], error->message);
      continue;
    }
    self->remote_crypto_attributes =
      g_list_append (self->remote_crypto_attributes, attr);
  }

  if (!self->remote_crypto_attributes) {
    g_warning ("Could not parse a single crypto attribute, aborting");
    return FALSE;
  }

  return update_state (self);
}


calls_srtp_crypto_attribute *
calls_sdp_crypto_context_get_local_crypto (CallsSdpCryptoContext *self)
{
  g_return_val_if_fail (CALLS_IS_SDP_CRYPTO_CONTEXT (self), NULL);

  if (self->state != CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_SUCCESS)
    return NULL;

  return get_crypto_attribute_by_tag (self->local_crypto_attributes,
                                      self->negotiated_tag);
}


calls_srtp_crypto_attribute *
calls_sdp_crypto_context_get_remote_crypto (CallsSdpCryptoContext *self)
{
  g_return_val_if_fail (CALLS_IS_SDP_CRYPTO_CONTEXT (self), NULL);

  if (self->state != CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_SUCCESS)
    return NULL;

  return get_crypto_attribute_by_tag (self->remote_crypto_attributes,
                                      self->negotiated_tag);
}


gboolean
calls_sdp_crypto_context_generate_offer (CallsSdpCryptoContext *self)
{
  calls_srtp_crypto_attribute *attr;

  g_return_val_if_fail (CALLS_IS_SDP_CRYPTO_CONTEXT (self), FALSE);

  if (self->state != CALLS_CRYPTO_CONTEXT_STATE_INIT) {
    g_warning ("Cannot generate offer. Need INIT state, but found %d",
               self->state);
    return FALSE;
  }

  g_assert (!self->local_crypto_attributes);

  attr = calls_srtp_crypto_attribute_new (1);
  attr->tag = 1;
  attr->crypto_suite = CALLS_SRTP_SUITE_AES_256_CM_SHA1_80;
  calls_srtp_crypto_attribute_init_keys (attr);

  self->local_crypto_attributes = g_list_append (NULL, attr);

  attr = calls_srtp_crypto_attribute_new (1);
  attr->tag = 2;
  attr->crypto_suite = CALLS_SRTP_SUITE_AES_256_CM_SHA1_32;
  calls_srtp_crypto_attribute_init_keys (attr);

  self->local_crypto_attributes = g_list_append (self->local_crypto_attributes, attr);

  attr = calls_srtp_crypto_attribute_new (1);
  attr->tag = 3;
  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_80;
  calls_srtp_crypto_attribute_init_keys (attr);

  self->local_crypto_attributes = g_list_append (self->local_crypto_attributes, attr);

  attr = calls_srtp_crypto_attribute_new (1);
  attr->tag = 4;
  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_32;
  calls_srtp_crypto_attribute_init_keys (attr);

  self->local_crypto_attributes = g_list_append (self->local_crypto_attributes, attr);

  return update_state (self);
}


gboolean
calls_sdp_crypto_context_generate_answer (CallsSdpCryptoContext *self)
{
  calls_srtp_crypto_attribute *attr = NULL;

  g_return_val_if_fail (CALLS_IS_SDP_CRYPTO_CONTEXT (self), FALSE);

  if (self->state != CALLS_CRYPTO_CONTEXT_STATE_OFFER_REMOTE) {
    g_warning ("Cannot generate answer. Need OFFER_REMOTE state, but found %d",
               self->state);
    return FALSE;
  }

  for (GList *node = self->remote_crypto_attributes; node; node = node->next) {
    calls_srtp_crypto_attribute *attr_offer = node->data;

    if (crypto_attribute_is_supported (self, attr_offer)) {
      attr = calls_srtp_crypto_attribute_new (1);
      attr->crypto_suite = attr_offer->crypto_suite;
      attr->tag = attr_offer->tag;
      calls_srtp_crypto_attribute_init_keys (attr);
      break;
    }
  }

  if (!attr)
    return FALSE;

  self->local_crypto_attributes = g_list_append (NULL, attr);

  return update_state (self);
}


gboolean
calls_sdp_crypto_context_get_is_negotiated (CallsSdpCryptoContext *self)
{
  g_return_val_if_fail (CALLS_IS_SDP_CRYPTO_CONTEXT (self), FALSE);

  return self->state == CALLS_CRYPTO_CONTEXT_STATE_NEGOTIATION_SUCCESS;
}


CallsCryptoContextState
calls_sdp_crypto_context_get_state (CallsSdpCryptoContext *self)
{
  g_return_val_if_fail (CALLS_IS_SDP_CRYPTO_CONTEXT (self), CALLS_CRYPTO_CONTEXT_STATE_INIT);

  return self->state;
}


GList *
calls_sdp_crypto_context_get_crypto_candidates (CallsSdpCryptoContext *self,
                                                gboolean               remote)
{
  g_return_val_if_fail (CALLS_IS_SDP_CRYPTO_CONTEXT (self), NULL);

  return g_list_copy (remote ? self->remote_crypto_attributes : self->local_crypto_attributes);
}
