/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-provider.h"
#include "calls-sip-media-manager.h"
#include "calls-sip-provider.h"
#include "calls-sip-origin.h"
#include "calls-sip-util.h"
#include "gst-rfc3551.h"

#include <gtk/gtk.h>

#include <sofia-sip/su_uniqueid.h>
#include <libpeas/peas.h>

typedef struct {
  CallsSipProvider *provider;
  CallsSipOrigin *origin_alice;
  CallsSipOrigin *origin_bob;
  CallsSipOrigin *origin_offline;
} SipFixture;

static void
test_sip_provider_object (SipFixture   *fixture,
                          gconstpointer user_data)
{
  SipEngineState state;

  g_assert_true (G_IS_OBJECT (fixture->provider));
  g_assert_true (CALLS_IS_MESSAGE_SOURCE (fixture->provider));
  g_assert_true (CALLS_IS_PROVIDER (fixture->provider));
  g_assert_true (CALLS_IS_SIP_PROVIDER (fixture->provider));

  g_object_get (fixture->provider,
                "sip-state", &state,
                NULL);
  g_assert_cmpint (state, ==, SIP_ENGINE_READY);
}

static void
test_sip_provider_origins (SipFixture   *fixture,
                           gconstpointer user_data)
{
  GListModel *origins;

  origins = calls_provider_get_origins (CALLS_PROVIDER (fixture->provider));

  g_assert_cmpuint (g_list_model_get_n_items (origins), ==, 0);
}

static void
setup_sip_provider (SipFixture   *fixture,
                    gconstpointer user_data)
{
  CallsProvider *provider = calls_provider_load_plugin ("sip");
  fixture->provider = CALLS_SIP_PROVIDER (provider);
}

static void
tear_down_sip_provider (SipFixture   *fixture,
                        gconstpointer user_data)
{
  g_clear_object (&fixture->provider);
  calls_provider_unload_plugin ("sip");
}


static void
test_sip_origin_objects (SipFixture   *fixture,
                         gconstpointer user_data)
{
  SipAccountState state_alice, state_bob, state_offline;

  g_assert_true (G_IS_OBJECT (fixture->origin_alice));
  g_assert_true (G_IS_OBJECT (fixture->origin_bob));
  g_assert_true (G_IS_OBJECT (fixture->origin_offline));

  g_assert_true (CALLS_IS_MESSAGE_SOURCE (fixture->origin_alice));
  g_assert_true (CALLS_IS_MESSAGE_SOURCE (fixture->origin_bob));
  g_assert_true (CALLS_IS_MESSAGE_SOURCE (fixture->origin_offline));

  g_assert_true (CALLS_IS_ORIGIN (fixture->origin_alice));
  g_assert_true (CALLS_IS_ORIGIN (fixture->origin_bob));
  g_assert_true (CALLS_IS_ORIGIN (fixture->origin_offline));

  g_assert_true (CALLS_IS_SIP_ORIGIN (fixture->origin_alice));
  g_assert_true (CALLS_IS_SIP_ORIGIN (fixture->origin_bob));
  g_assert_true (CALLS_IS_SIP_ORIGIN (fixture->origin_offline));

  g_object_get (fixture->origin_alice,
                "account-state", &state_alice,
                NULL);
  g_object_get (fixture->origin_bob,
                "account-state", &state_bob,
                NULL);
  g_object_get (fixture->origin_offline,
                "account-state", &state_offline,
                NULL);

  g_assert_cmpint (state_alice, ==, SIP_ACCOUNT_ONLINE);
  g_assert_cmpint (state_bob, ==, SIP_ACCOUNT_ONLINE);
  g_assert_cmpint (state_offline, ==, SIP_ACCOUNT_OFFLINE);
}

static void
test_sip_origin_call_lists (SipFixture   *fixture,
                            gconstpointer user_data)
{
  g_autoptr (GList) calls_alice = NULL;
  g_autoptr (GList) calls_bob = NULL;
  g_autoptr (GList) calls_offline = NULL;

  calls_alice = calls_origin_get_calls (CALLS_ORIGIN (fixture->origin_alice));
  g_assert_null (calls_alice);

  calls_bob = calls_origin_get_calls (CALLS_ORIGIN (fixture->origin_bob));
  g_assert_null (calls_bob);

  calls_offline = calls_origin_get_calls (CALLS_ORIGIN (fixture->origin_offline));
  g_assert_null (calls_offline);
}

static void
test_sip_call_direct_calls (SipFixture   *fixture,
                            gconstpointer user_data)
{
  ;
}

static void
setup_sip_origins (SipFixture   *fixture,
                   gconstpointer user_data)
{
  GListModel *origins;

  setup_sip_provider (fixture, user_data);

  calls_sip_provider_add_origin (fixture->provider, "Alice",
                                 "alice", NULL, NULL, 5060,
                                 5060, "UDP", TRUE, FALSE);

  calls_sip_provider_add_origin (fixture->provider, "Bob",
                                 "bob", NULL, NULL, 5060,
                                 5061, "UDP", TRUE, FALSE);

  calls_sip_provider_add_origin (fixture->provider, "Offline",
                                 "someuser", "sip.imaginary-host.org", "password", 5060,
                                 5062, "UDP", FALSE, FALSE);

  origins = calls_provider_get_origins
    (CALLS_PROVIDER (fixture->provider));

  fixture->origin_alice = g_list_model_get_item (origins, 0);
  fixture->origin_bob = g_list_model_get_item (origins, 1);
  fixture->origin_offline = g_list_model_get_item (origins, 2);
}

static void
tear_down_sip_origins (SipFixture   *fixture,
                       gconstpointer user_data)
{
  g_clear_object (&fixture->origin_alice);
  g_clear_object (&fixture->origin_bob);
  g_clear_object (&fixture->origin_offline);

  tear_down_sip_provider (fixture, user_data);
}

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
test_sip_media_manager ()
{
  g_autoptr (CallsSipMediaManager) manager = calls_sip_media_manager_default ();
  char *sdp_message = NULL;
  GList *codecs = NULL;

  /* Check single codecs */
  codecs = g_list_append (NULL, media_codec_by_name ("PCMA"));

  /* PCMA RTP */
  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, 40002, FALSE, codecs);

  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 40002 RTP/AVP 8"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtpmap:8 PCMA/8000"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtcp:40003"));

  g_free (sdp_message);

  /* PCMA SRTP */
  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, 42002, TRUE, codecs);
  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 42002 RTP/SAVP 8"));

  g_clear_pointer (&codecs, g_list_free);
  g_free (sdp_message);

  /* G722 RTP */
  codecs = g_list_append (NULL, media_codec_by_name ("G722"));

  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, 42042, FALSE, codecs);

  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 42042 RTP/AVP 9"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtpmap:9 G722/8000"));
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "a=rtcp:42043"));

  g_clear_pointer (&codecs, g_list_free);
  g_free (sdp_message);

  /* G722 PCMU PCMA RTP (in this order) */
  codecs = g_list_append (NULL, media_codec_by_name ("G722"));
  codecs = g_list_append (codecs, media_codec_by_name ("PCMU"));
  codecs = g_list_append (codecs, media_codec_by_name ("PCMA"));

  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, 33340, FALSE, codecs);

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

  /* GSM PCMA G722 PCMU (in this order) */
  codecs = g_list_append (NULL, media_codec_by_name ("GSM"));
  codecs = g_list_append (codecs, media_codec_by_name ("PCMA"));
  codecs = g_list_append (codecs, media_codec_by_name ("G722"));
  codecs = g_list_append (codecs, media_codec_by_name ("PCMU"));

  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, 18098, TRUE, codecs);

  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 18098 RTP/SAVP 3 8 9 0"));

  g_clear_pointer (&codecs, g_list_free);
  g_free (sdp_message);

  g_test_expect_message ("CallsSipMediaManager", G_LOG_LEVEL_WARNING,
                         "No supported codecs found. Can't build meaningful SDP message");
  sdp_message =
    calls_sip_media_manager_get_capabilities (manager, 25048, FALSE, NULL);

  g_test_assert_expected_messages ();
  g_assert_true (sdp_message);
  g_assert_true (find_string_in_sdp_message (sdp_message,
                                             "m=audio 0 RTP/AVP"));

  g_free (sdp_message);
}

gint
main (gint   argc,
      gchar *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

#ifdef PLUGIN_BUILDDIR
  peas_engine_add_search_path (peas_engine_get_default (), PLUGIN_BUILDDIR, NULL);
#endif
  /* this is a workaround for an issue with sofia: https://github.com/freeswitch/sofia-sip/issues/58 */
  su_random64 ();

  g_test_add ("/Calls/SIP/provider_object", SipFixture, NULL,
              setup_sip_provider, test_sip_provider_object, tear_down_sip_provider);

  g_test_add ("/Calls/SIP/provider_origins", SipFixture, NULL,
              setup_sip_provider, test_sip_provider_origins, tear_down_sip_provider);

  g_test_add ("/Calls/SIP/origin_objects", SipFixture, NULL,
              setup_sip_origins, test_sip_origin_objects, tear_down_sip_origins);

  g_test_add ("/Calls/SIP/origin_call_lists", SipFixture, NULL,
              setup_sip_origins, test_sip_origin_call_lists, tear_down_sip_origins);

  g_test_add ("/Calls/SIP/calls_direct_call", SipFixture, NULL,
              setup_sip_origins, test_sip_call_direct_calls, tear_down_sip_origins);

  g_test_add_func ("/Calls/SIP/media_manager", test_sip_media_manager);

  return g_test_run();
}
