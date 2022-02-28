/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-sip-media-manager.h"
#include "gst-rfc3551.h"

#include <gtk/gtk.h>

#include <gst/gst.h>

static gboolean
find_string_in_sdp_message (const char *sdp,
                            const char *string)
{
  char **split_string = NULL;
  gboolean found = FALSE;

  split_string = g_strsplit (sdp, "\r\n", -1);

  for (guint i = 0; split_string[i] != NULL; i++) {
    if (g_strcmp0 (split_string[i], string) == 0) {
      found = TRUE;
      break;
    }
  }

  g_strfreev (split_string);
  return found;
}


static void
test_sip_media_manager_caps (void)
{
  g_autoptr (CallsSipMediaManager) manager = calls_sip_media_manager_default ();
  char *sdp_message = NULL;
  GList *codecs = NULL;

  /* Check single codecs */
  codecs = g_list_append (NULL, media_codec_by_name ("PCMA"));

  g_debug ("Testing generated SDP messages");

  /* PCMA RTP */
  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, NULL, 40002, 40003, FALSE, codecs);

  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 40002 RTP/AVP 8"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtpmap:8 PCMA/8000"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtcp:40003"));

  g_free (sdp_message);

  g_debug ("PCMA RTP test OK");

  /* PCMA SRTP */
  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, NULL, 42002, 42003, TRUE, codecs);
  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 42002 RTP/SAVP 8"));

  g_clear_pointer (&codecs, g_list_free);
  g_free (sdp_message);

  g_debug ("PCMA SRTP test OK");

  /* G722 RTP */
  codecs = g_list_append (NULL, media_codec_by_name ("G722"));

  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, NULL, 42042, 55543, FALSE, codecs);

  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 42042 RTP/AVP 9"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtpmap:9 G722/8000"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtcp:55543"));

  g_clear_pointer (&codecs, g_list_free);
  g_free (sdp_message);

  g_debug ("G722 RTP test OK");

  /* G722 PCMU PCMA RTP (in this order) */
  codecs = g_list_append (NULL, media_codec_by_name ("G722"));
  codecs = g_list_append (codecs, media_codec_by_name ("PCMU"));
  codecs = g_list_append (codecs, media_codec_by_name ("PCMA"));

  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, NULL, 33340, 33341, FALSE, codecs);

  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 33340 RTP/AVP 9 0 8"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtpmap:9 G722/8000"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtpmap:0 PCMU/8000"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtpmap:8 PCMA/8000"));

  g_clear_pointer (&codecs, g_list_free);
  g_free (sdp_message);

  g_debug ("multiple codecs RTP test OK");

  /* GSM PCMA G722 PCMU SRTP (in this order) */
  codecs = g_list_append (NULL, media_codec_by_name ("GSM"));
  codecs = g_list_append (codecs, media_codec_by_name ("PCMA"));
  codecs = g_list_append (codecs, media_codec_by_name ("G722"));
  codecs = g_list_append (codecs, media_codec_by_name ("PCMU"));

  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, NULL, 18098, 18099, TRUE, codecs);

  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 18098 RTP/SAVP 3 8 9 0"));

  g_clear_pointer (&codecs, g_list_free);
  g_free (sdp_message);

  g_debug ("multiple codecs SRTP test OK");

  /* no codecs */
  g_test_expect_message ("CallsSipMediaManager", G_LOG_LEVEL_WARNING,
                         "No supported codecs found. Can't build meaningful SDP message");
  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, NULL, 25048, 25049, FALSE, NULL);

  g_test_assert_expected_messages ();
  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 0 RTP/AVP"));

  g_free (sdp_message);

  g_debug ("no codecs test OK");
}


int
main (int   argc,
      char *argv[])
{
  int ret;

  gtk_test_init (&argc, &argv, NULL);

  gst_init (NULL, NULL);

  g_test_add_func ("/Calls/SIP/media_manager/capabilities", test_sip_media_manager_caps);

  ret = g_test_run();

  gst_deinit ();

  return ret;
}
