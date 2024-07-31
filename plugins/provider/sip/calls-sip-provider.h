/*
 * Copyright (C) 2021 Purism SPC
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

#pragma once

#include "calls-provider.h"
#include "calls-sip-origin.h"

#include <glib-object.h>
#include <libpeas.h>

G_BEGIN_DECLS

#define CALLS_TYPE_SIP_PROVIDER (calls_sip_provider_get_type ())

G_DECLARE_FINAL_TYPE (CallsSipProvider, calls_sip_provider, CALLS, SIP_PROVIDER, CallsProvider);

CallsSipProvider *calls_sip_provider_new           (void);
CallsSipOrigin   *calls_sip_provider_add_origin    (CallsSipProvider  *self,
                                                    const char        *id,
                                                    const char        *host,
                                                    const char        *user,
                                                    const char        *password,
                                                    const char        *display_name,
                                                    const char        *transport_protocol,
                                                    gint               port,
                                                    SipMediaEncryption media_encryption,
                                                    gboolean           store_credentials);
CallsSipOrigin *calls_sip_provider_add_origin_full (CallsSipProvider  *self,
                                                    const char        *id,
                                                    const char        *host,
                                                    const char        *user,
                                                    const char        *password,
                                                    const char        *display_name,
                                                    const char        *transport_protocol,
                                                    gint               port,
                                                    SipMediaEncryption media_encryption,
                                                    gboolean           auto_connect,
                                                    gboolean           direct_mode,
                                                    gint               local_port,
                                                    gboolean           use_for_tel,
                                                    gboolean           store_credentials);
gboolean calls_sip_provider_remove_origin          (CallsSipProvider *self,
                                                    CallsSipOrigin   *origin);
void     calls_sip_provider_load_accounts          (CallsSipProvider *self,
                                                    GKeyFile         *key_file);
void     calls_sip_provider_save_accounts          (CallsSipProvider *self,
                                                    GKeyFile         *key_file);
gboolean calls_sip_provider_save_accounts_to_disk  (CallsSipProvider *self);
void     peas_register_types                       (PeasObjectModule *module);

G_END_DECLS
