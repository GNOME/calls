/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-sip-call.h"
#include "calls-sip-media-manager.h"
#include "gst-rfc3551.h"

#include <glib.h>

#include <gst/gst.h>

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

  g_test_add_func ("/Calls/media/pipeline/states", test_media_pipeline_states);
  g_test_add_func ("/Calls/media/pipeline/setup_codecs", test_media_pipeline_setup_codecs);
  g_test_add_func ("/Calls/media/pipeline/start_no_codec", test_media_pipeline_start_no_codec);
  g_test_add_func ("/Calls/media/pipeline/finalized_in_call", test_media_pipeline_finalized_in_call);

  ret = g_test_run ();

  g_assert_finalize_object (manager);

  gst_deinit ();

  return ret;
}
