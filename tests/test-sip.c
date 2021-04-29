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
#include "calls-account.h"
#include "gst-rfc3551.h"

#include <gtk/gtk.h>

#include <sofia-sip/su_uniqueid.h>
#include <libpeas/peas.h>

typedef struct {
  CallsSipProvider *provider;
  CallsSipOrigin *origin_alice;
  CallsSipOrigin *origin_bob;
  CallsSipOrigin *origin_offline;
  CallsCredentials *credentials_alice;
  CallsCredentials *credentials_bob;
  CallsCredentials *credentials_offline;
} SipFixture;


static gboolean is_call_test_done = FALSE;
static gboolean are_call_tests_done = FALSE;

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

  is_call_test_done = FALSE;
  are_call_tests_done = FALSE;
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
  CallsAccountState state_alice, state_bob, state_offline;

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

  g_assert_cmpint (state_alice, ==, CALLS_ACCOUNT_ONLINE);
  g_assert_cmpint (state_bob, ==, CALLS_ACCOUNT_ONLINE);
  g_assert_cmpint (state_offline, ==, CALLS_ACCOUNT_OFFLINE);
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

static gboolean
on_call_hangup_cb (gpointer user_data)
{
  CallsCall *call = CALLS_CALL (user_data);

  g_debug ("Hanging up call");
  calls_call_hang_up (call);

  return G_SOURCE_REMOVE;
}

static gboolean
on_call_answer_cb (gpointer user_data)
{
  CallsCall *call = CALLS_CALL (user_data);

  g_debug ("Answering incoming call");
  calls_call_answer (call);

  return G_SOURCE_REMOVE;
}

static void
on_autoreject_state_changed_cb (CallsCall     *call,
                                CallsCallState new_state,
                                CallsCallState old_state,
                                gpointer       user_data)
{
  g_assert_cmpint (old_state, ==, CALLS_CALL_STATE_INCOMING);
  g_assert_cmpint (new_state, ==, CALLS_CALL_STATE_DISCONNECTED);

  g_object_unref (call);

  is_call_test_done = TRUE;
}

static void
on_state_changed_cb (CallsCall     *call,
                     CallsCallState new_state,
                     CallsCallState old_state,
                     gpointer       user_data)
{
  gboolean schedule_hangup = GPOINTER_TO_INT (user_data);

  switch (old_state) {
  case CALLS_CALL_STATE_INCOMING:
  case CALLS_CALL_STATE_DIALING:
    g_assert_cmpint (new_state, ==, CALLS_CALL_STATE_ACTIVE);

    if (schedule_hangup)
      g_idle_add ((GSourceFunc) on_call_hangup_cb, call);
    break;

  case CALLS_CALL_STATE_ACTIVE:
    g_assert_cmpint (new_state, ==, CALLS_CALL_STATE_DISCONNECTED);

    g_object_unref (call);

    if (is_call_test_done)
      are_call_tests_done = TRUE;

    is_call_test_done = TRUE;
    break;

  default:
    g_assert_not_reached ();
  }
}

static gboolean
on_incoming_call_autoaccept_cb (CallsOrigin *origin,
                                CallsCall   *call,
                                gpointer     user_data)
{
  CallsCallState state = calls_call_get_state (call);

  g_assert_cmpint (state, ==, CALLS_CALL_STATE_INCOMING);

  g_object_ref (call);

  g_idle_add ((GSourceFunc) on_call_answer_cb, call);

  g_signal_connect (call, "state-changed",
                    (GCallback) on_state_changed_cb, user_data);

  return G_SOURCE_REMOVE;
}

static gboolean
on_incoming_call_autoreject_cb (CallsOrigin *origin,
                                CallsCall   *call,
                                gpointer     user_data)
{
  CallsCallState state = calls_call_get_state (call);

  g_assert_cmpint (state, ==, CALLS_CALL_STATE_INCOMING);

  g_object_ref (call);
  g_idle_add ((GSourceFunc) on_call_hangup_cb, call);

  g_signal_connect (call, "state-changed",
                    (GCallback) on_autoreject_state_changed_cb, NULL);

  return G_SOURCE_REMOVE;
}


static gboolean
on_outgoing_call_cb (CallsOrigin *origin,
                     CallsCall   *call,
                     gpointer     user_data)
{
  CallsCallState state = calls_call_get_state (call);

  g_assert_cmpint (state, ==, CALLS_CALL_STATE_DIALING);

  g_object_ref (call);

  g_signal_connect (call, "state-changed",
                    (GCallback) on_state_changed_cb, user_data);

  return G_SOURCE_REMOVE;
}

static void
test_sip_call_direct_calls (SipFixture   *fixture,
                            gconstpointer user_data)
{
  gint local_port_alice, local_port_bob;
  g_autofree gchar *address_alice = NULL;
  g_autofree gchar *address_bob = NULL;
  gulong handler_alice, handler_bob;

  g_object_get (fixture->origin_alice,
                "local-port", &local_port_alice,
                NULL);
  address_alice = g_strdup_printf ("sip:alice@127.0.0.1:%d",
                                   local_port_alice);

  g_object_get (fixture->origin_bob,
                "local-port", &local_port_bob,
                NULL);
  address_bob = g_strdup_printf ("sip:bob@127.0.0.1:%d",
                                 local_port_bob);

  /* Case 1: Bob calls Alice, Alice rejects call */

  g_debug ("Call test: Stage 1");

  handler_alice =
    g_signal_connect (fixture->origin_alice,
                      "call-added",
                      G_CALLBACK (on_incoming_call_autoreject_cb),
                      NULL);

  calls_origin_dial (CALLS_ORIGIN (fixture->origin_bob), address_alice);

  while (!is_call_test_done)
    g_main_context_iteration (NULL, TRUE);

  is_call_test_done = FALSE;
  are_call_tests_done = FALSE;

  g_signal_handler_disconnect (fixture->origin_alice, handler_alice);

  /* Case 2: Alice calls Bob, Bob accepts and hangs up shortly after */

  g_debug ("Call test: Stage 2");

  handler_alice =
    g_signal_connect (fixture->origin_alice,
                      "call-added",
                      G_CALLBACK (on_outgoing_call_cb),
                      GINT_TO_POINTER (FALSE));

  handler_bob =
    g_signal_connect (fixture->origin_bob,
                      "call-added",
                      G_CALLBACK (on_incoming_call_autoaccept_cb),
                      GINT_TO_POINTER (TRUE));

  calls_origin_dial (CALLS_ORIGIN (fixture->origin_alice), address_bob);

  while (!are_call_tests_done)
    g_main_context_iteration (NULL, TRUE);

  is_call_test_done = FALSE;
  are_call_tests_done = FALSE;

  g_signal_handler_disconnect (fixture->origin_alice, handler_alice);
  g_signal_handler_disconnect (fixture->origin_bob, handler_bob);

  /* Case 3: Alice calls Bob, Bob accepts and Alice hangs up shortly after */

  g_debug ("Call test: Stage 3");

  handler_alice =
    g_signal_connect (fixture->origin_alice,
                      "call-added",
                      G_CALLBACK (on_outgoing_call_cb),
                      GINT_TO_POINTER (TRUE));

  handler_bob =
    g_signal_connect (fixture->origin_bob,
                      "call-added",
                      G_CALLBACK (on_incoming_call_autoaccept_cb),
                      GINT_TO_POINTER (FALSE));

  calls_origin_dial (CALLS_ORIGIN (fixture->origin_alice), address_bob);

  while (!are_call_tests_done)
    g_main_context_iteration (NULL, TRUE);

  is_call_test_done = FALSE;
  are_call_tests_done = FALSE;

  g_signal_handler_disconnect (fixture->origin_alice, handler_alice);
  g_signal_handler_disconnect (fixture->origin_bob, handler_bob);

}

static void
setup_sip_origins (SipFixture   *fixture,
                   gconstpointer user_data)
{
  GListModel *origins;
  CallsCredentials *alice = calls_credentials_new ();
  CallsCredentials *bob = calls_credentials_new ();
  CallsCredentials *offline = calls_credentials_new ();

  setup_sip_provider (fixture, user_data);

  g_object_set (alice, "name", "Alice", "user", "alice", NULL);

  calls_sip_provider_add_origin (fixture->provider, alice, 5060, TRUE);

  g_object_set (bob, "name", "Bob", "user", "bob", NULL);

  calls_sip_provider_add_origin (fixture->provider, bob, 5061, TRUE);

  g_object_set (offline,
                "name", "Offline",
                "user", "someuser",
                "host", "sip.imaginary-host.org",
                "password", "password123",
                "port", 5060,
                "protocol", "UDP",
                "auto-connect", FALSE,
                NULL);

  calls_sip_provider_add_origin (fixture->provider, offline, 0, FALSE);

  origins = calls_provider_get_origins
    (CALLS_PROVIDER (fixture->provider));

  fixture->origin_alice = g_list_model_get_item (origins, 0);
  fixture->credentials_alice = alice;

  fixture->origin_bob = g_list_model_get_item (origins, 1);
  fixture->credentials_bob = bob;

  fixture->origin_offline = g_list_model_get_item (origins, 2);
  fixture->credentials_offline = offline;
}

static void
tear_down_sip_origins (SipFixture   *fixture,
                       gconstpointer user_data)
{
  g_clear_object (&fixture->origin_alice);
  g_clear_object (&fixture->credentials_alice);

  g_clear_object (&fixture->origin_bob);
  g_clear_object (&fixture->credentials_bob);

  g_clear_object (&fixture->origin_offline);
  g_clear_object (&fixture->credentials_offline);

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
