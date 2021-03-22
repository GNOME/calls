/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-sip-provider.h"
#include "calls-sip-origin.h"
#include "calls-sip-util.h"

#include <gtk/gtk.h>
#include <sofia-sip/su_uniqueid.h>

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
  g_assert_true (state == SIP_ENGINE_READY);
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
  fixture->provider = calls_sip_provider_new ();
}

static void
tear_down_sip_provider (SipFixture   *fixture,
                        gconstpointer user_data)
{
  g_clear_object (&fixture->provider);
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

  g_assert_true (state_alice == SIP_ACCOUNT_ONLINE);
  g_assert_true (state_bob == SIP_ACCOUNT_ONLINE);
  g_assert_true (state_offline == SIP_ACCOUNT_OFFLINE);
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

gint
main (gint   argc,
      gchar *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

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

  return g_test_run();
}
