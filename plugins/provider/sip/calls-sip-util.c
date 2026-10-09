/*
 * Copyright (C) 2021-2022 Purism SPC
 *               2026 Phosh.mobi e.V.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 */

#define G_LOG_DOMAIN "CallsSipUtil"

#include "calls-sip-util.h"
#include "calls-srtp-utils.h"
#include "calls-util.h"
#include "gst-rfc3551.h"


/* helper for calls_sip_util_get_sdp() */
static char *
get_connection_line (const char *ip)
{
  int af_family;

  if (STR_IS_NULL_OR_EMPTY(ip))
    goto invalid;

  af_family = get_address_family_for_ip (ip, TRUE);
  if (af_family == AF_UNSPEC)
    goto invalid;

  return g_strdup_printf ("c=IN %s %s\r\n",
                          af_family == AF_INET ? "IP4" : "IP6",
                          ip);

  invalid:
  return NULL;
}

gboolean
calls_sip_util_check_ipv6 (const char *host)
{
  /* TODO DNS SRV records to determine whether or not to use IPv6 */
  return FALSE;
}


gboolean
calls_sip_util_check_sips (const char *addr)
{
  /* To keep it simple we only check if the URL starts with "sips:" */
  return g_str_has_prefix (addr, "sips:");
}


const gchar *
calls_sip_util_get_protocol_prefix (const char *protocol)
{
  if (g_strcmp0 (protocol, "UDP") == 0 ||
      g_strcmp0 (protocol, "TCP") == 0)
    return "sip";

  if (g_strcmp0 (protocol, "TLS") == 0)
    return "sips";

  return NULL;
}


gboolean
calls_sip_util_protocol_is_valid (const char *protocol)
{
  return g_strcmp0 (protocol, "UDP") == 0 ||
         g_strcmp0 (protocol, "TCP") == 0 ||
         g_strcmp0 (protocol, "TLS") == 0;
}

/* calls_sip_util_get_sdp:
 * @own_ip: IP to set in the connection line (Should eventually come from the ICE stack)
 * @rtp_port: Port to use for RTP (Should eventually come from the ICE stack)
 * @rtcp_port: Port to use for RTCP (Should eventually come from the ICE stack)
 * @crypto_attributes: A #GList of #calls_srtp_crypto_attribute
 * @codecs: A #GList of #MediaCodecInfo
 *
 * Returns: (transfer full): string describing capabilities
 * to be used in the session description (SDP)
 */
char *
calls_sip_util_get_sdp (const char *own_ip,
                        int         rtp_port,
                        int         rtcp_port,
                        GList      *crypto_attributes,
                        GList      *codecs)
{
  g_autoptr (GString) media_line = NULL;
  g_autoptr (GString) attribute_lines = NULL;
  g_autofree char *connection_line = NULL;
  const char *rtp_profile = crypto_attributes ? "SAVP" : "AVP";
  GList *node;

  media_line = g_string_new (NULL);
  attribute_lines = g_string_new (NULL);

  if (codecs == NULL) {
    g_warning ("No supported codecs found. Can't build meaningful SDP message");
    g_string_append_printf (media_line, "m=audio 0 RTP/%s", rtp_profile);
    goto done;
  }

  /* media lines look f.e like "audio 31337 RTP/AVP 9 8 0" */
  g_string_append_printf (media_line,
                          "m=audio %d RTP/%s", rtp_port, rtp_profile);

  for (node = codecs; node != NULL; node = node->next) {
    MediaCodecInfo *codec = node->data;

    g_string_append_printf (media_line, " %u", codec->payload_id);
    g_string_append_printf (attribute_lines,
                            "a=rtpmap:%u %s/%u%s",
                            codec->payload_id,
                            codec->name,
                            codec->clock_rate,
                            "\r\n");
  }

  for (node = crypto_attributes; node != NULL; node = node->next) {
    calls_srtp_crypto_attribute *attr = node->data;
    g_autoptr (GError) error = NULL;
    g_autofree char *crypto_line =
      calls_srtp_print_sdp_crypto_attribute (attr, &error);

    if (!crypto_line) {
      g_warning ("Could not print SDP crypto line for tag %d: %s", attr->tag, error->message);
      continue;
    }
    g_string_append_printf (attribute_lines, "%s\r\n", crypto_line);
  }

  g_string_append_printf (attribute_lines, "a=rtcp:%d\r\n", rtcp_port);

done:
  connection_line = get_connection_line (own_ip);

  return g_strdup_printf ("v=0\r\n"
                          "%s"
                          "%s\r\n"
                          "%s\r\n",
                          connection_line ?: "",
                          media_line->str,
                          attribute_lines->str);
}

/* calls_sip_util_get_codecs_from_sdp
 *
 * @sdp: A #sdp_media_t media description
 *
 * Returns: (transfer container): A #GList of codecs found in the SDP message
 */
GList *
calls_sip_util_get_codecs_from_sdp (sdp_media_t *sdp_media)
{
  GList *codecs = NULL;
  sdp_rtpmap_t *rtpmap = NULL;

  g_return_val_if_fail (sdp_media, NULL);

  if (sdp_media->m_type != sdp_media_audio) {
    g_warning ("Only the 'audio' media type is supported");
    return NULL;
  }

  for (rtpmap = sdp_media->m_rtpmaps; rtpmap != NULL; rtpmap = rtpmap->rm_next) {
    MediaCodecInfo *codec = media_codec_by_payload_id (rtpmap->rm_pt);
    if (codec)
      codecs = g_list_append (codecs, codec);
  }

  if (sdp_media->m_next != NULL)
    g_warning ("Currently only a single media session is supported");

  if (codecs == NULL)
    g_warning ("Did not find any common codecs");

  return codecs;
}
