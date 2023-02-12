/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-sip-call.h"
#include "calls-sip-media-manager.h"
#include "calls-srtp-utils.h"
#include "gst-rfc3551.h"

#include <glib.h>

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
  CallsSipMediaManager *manager = calls_sip_media_manager_default ();
  char *sdp_message = NULL;
  GList *codecs = NULL;
  GList *crypto_attributes;
  calls_srtp_crypto_attribute *attr;

  attr = calls_srtp_crypto_attribute_new (1);
  attr->tag = 1;
  attr->crypto_suite = CALLS_SRTP_SUITE_AES_CM_128_SHA1_80;
  calls_srtp_crypto_attribute_init_keys (attr);

  crypto_attributes = g_list_append (NULL, attr);
  /* Check single codecs */
  codecs = g_list_append (NULL, media_codec_by_name ("PCMA"));

  g_debug ("Testing generated SDP messages");

  /* PCMA RTP */
  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, NULL, 40002, 40003, NULL, codecs);

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
    calls_sip_media_manager_get_capabilities (manager, NULL, 42002, 42003, crypto_attributes, codecs);
  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 42002 RTP/SAVP 8"));

  g_clear_pointer (&codecs, g_list_free);
  g_free (sdp_message);

  g_debug ("PCMA SRTP test OK");

  /* G722 RTP */
  codecs = g_list_append (NULL, media_codec_by_name ("G722"));

  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, NULL, 42042, 55543, NULL, codecs);

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
    calls_sip_media_manager_get_capabilities (manager, NULL, 33340, 33341, NULL, codecs);

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
    calls_sip_media_manager_get_capabilities (manager, NULL, 18098, 18099, crypto_attributes, codecs);

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
    calls_sip_media_manager_get_capabilities (manager, NULL, 25048, 25049, NULL, NULL);

  g_test_assert_expected_messages ();
  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 0 RTP/AVP"));

  g_free (sdp_message);

  g_debug ("no codecs test OK");

  g_list_free (crypto_attributes);
  calls_srtp_crypto_attribute_free (attr);
}


static void
test_media_pipeline_states (void)
{
  CallsSipMediaPipeline *pipeline = calls_sip_media_pipeline_new (NULL);
  MediaCodecInfo *codec = media_codec_by_name ("PCMA");

  g_assert_cmpint (calls_sip_media_pipeline_get_state (pipeline), ==,
                   CALLS_MEDIA_PIPELINE_STATE_NEED_CODEC);

  calls_sip_media_pipeline_set_codec (pipeline, codec);

  g_assert_cmpint (calls_sip_media_pipeline_get_state (pipeline), ==,
                   CALLS_MEDIA_PIPELINE_STATE_READY);

  calls_sip_media_pipeline_start (pipeline);
  g_assert_cmpint (calls_sip_media_pipeline_get_state (pipeline), ==,
                   CALLS_MEDIA_PIPELINE_STATE_PLAY_PENDING);

  calls_sip_media_pipeline_pause (pipeline, TRUE);
  g_assert_cmpint (calls_sip_media_pipeline_get_state (pipeline), ==,
                   CALLS_MEDIA_PIPELINE_STATE_PAUSE_PENDING);

  calls_sip_media_pipeline_pause (pipeline, FALSE);
  g_assert_cmpint (calls_sip_media_pipeline_get_state (pipeline), ==,
                   CALLS_MEDIA_PIPELINE_STATE_PLAY_PENDING);

  calls_sip_media_pipeline_stop (pipeline);
  g_assert_cmpint (calls_sip_media_pipeline_get_state (pipeline), ==,
                   CALLS_MEDIA_PIPELINE_STATE_STOP_PENDING);

  g_assert_finalize_object (pipeline);
}


static void
test_media_pipeline_setup_codecs (void)
{
  const char * const codec_names[] = {"PCMA", "PCMU", "GSM", "G722"};

  for (uint i = 0; i < G_N_ELEMENTS (codec_names); i++) {
    g_autoptr (CallsSipMediaPipeline) pipeline = calls_sip_media_pipeline_new (NULL);
    MediaCodecInfo *codec = media_codec_by_name ("PCMA");

    g_assert_cmpint (calls_sip_media_pipeline_get_state (pipeline), ==,
                     CALLS_MEDIA_PIPELINE_STATE_NEED_CODEC);

    calls_sip_media_pipeline_set_codec (pipeline, codec);

    g_assert_cmpint (calls_sip_media_pipeline_get_state (pipeline), ==,
                     CALLS_MEDIA_PIPELINE_STATE_READY);
  }
}


static void
test_media_pipeline_start_no_codec (void)
{
  g_autoptr (CallsSipMediaPipeline) pipeline = calls_sip_media_pipeline_new (NULL);

  g_assert_cmpint (calls_sip_media_pipeline_get_state (pipeline), ==,
                   CALLS_MEDIA_PIPELINE_STATE_NEED_CODEC);

  g_test_expect_message ("CallsSipMediaPipeline", G_LOG_LEVEL_WARNING,
                         "Cannot start pipeline because it's not ready");

  calls_sip_media_pipeline_start (pipeline);
  g_test_assert_expected_messages ();

  g_test_expect_message ("CallsSipMediaPipeline", G_LOG_LEVEL_WARNING,
                         "Cannot pause or unpause pipeline because it's not currently active");

  calls_sip_media_pipeline_pause (pipeline, TRUE);
  g_test_assert_expected_messages ();
}


static void
test_media_pipeline_finalized_in_call (void)
{
  CallsSipMediaManager *manager = calls_sip_media_manager_default ();
  CallsSipMediaPipeline *pipeline = calls_sip_media_pipeline_new (NULL);
  CallsSipCall *call = calls_sip_call_new ("sip:alice@example.org",
                                           TRUE,
                                           "127.0.0.1",
                                           pipeline,
                                           SIP_MEDIA_ENCRYPTION_NONE,
                                           NULL);

  g_object_unref (call);
  g_assert_finalize_object (pipeline);

  pipeline = calls_sip_media_manager_get_pipeline (manager);
  call = calls_sip_call_new ("sip:bob@example.org",
                             TRUE,
                             "127.0.0.1",
                             pipeline,
                             SIP_MEDIA_ENCRYPTION_NONE,
                             NULL);
  g_object_unref (call);
  g_assert_finalize_object (pipeline);
}


int
main (int   argc,
      char *argv[])
{
  CallsSipMediaManager *manager = calls_sip_media_manager_default ();
  int ret;

  g_test_init (&argc, &argv, NULL);

  gst_init (NULL, NULL);

  g_test_add_func ("/Calls/media/media_manager/capabilities", test_sip_media_manager_caps);
  g_test_add_func ("/Calls/media/pipeline/states", test_media_pipeline_states);
  g_test_add_func ("/Calls/media/pipeline/setup_codecs", test_media_pipeline_setup_codecs);
  g_test_add_func ("/Calls/media/pipeline/start_no_codec", test_media_pipeline_start_no_codec);
  g_test_add_func ("/Calls/media/pipeline/finalized_in_call", test_media_pipeline_finalized_in_call);

  ret = g_test_run ();

  g_assert_finalize_object (manager);

  gst_deinit ();

  return ret;
}
