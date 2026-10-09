/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 */

#pragma once

#include "calls-sip-util.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_SIP_ORIGIN (calls_sip_origin_get_type ())

G_DECLARE_FINAL_TYPE (CallsSipOrigin, calls_sip_origin, CALLS, SIP_ORIGIN, GObject)

void calls_sip_origin_set_credentials          (CallsSipOrigin    *self,
                                                const char        *host,
                                                const char        *proxy,
                                                const char        *user,
                                                const char        *password,
                                                const char        *display_name,
                                                const char        *transport_protocol,
                                                int                port,
                                                SipMediaEncryption media_encryption,
                                                gboolean           use_for_tel,
                                                gboolean           auto_connect);

G_END_DECLS
