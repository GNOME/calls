/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0+
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 */

#include "calls-sip-provider.h"
#include "calls-sip-util.h"

#include <gtk/gtk.h>
#include <sofia-sip/su_uniqueid.h>

typedef struct {
  CallsSipProvider *provider;
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
setup_sip (SipFixture   *fixture,
           gconstpointer user_data)
{
  fixture->provider = calls_sip_provider_new ();
}

static void
tear_down_sip (SipFixture   *fixture,
               gconstpointer user_data)
{
  g_clear_object (&fixture->provider);
}

gint
main (gint   argc,
      gchar *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  /* this is a workaround for an issue with sofia: https://github.com/freeswitch/sofia-sip/issues/58 */
  su_random64 ();

  g_test_add ("/Calls/SIP/object", SipFixture, NULL,
              setup_sip, test_sip_provider_object, tear_down_sip);

  return g_test_run();
}
