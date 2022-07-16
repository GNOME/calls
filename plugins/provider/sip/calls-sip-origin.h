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

#include "calls-sip-util.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_SIP_ORIGIN (calls_sip_origin_get_type ())

G_DECLARE_FINAL_TYPE (CallsSipOrigin, calls_sip_origin, CALLS, SIP_ORIGIN, GObject)

void calls_sip_origin_set_credentials          (CallsSipOrigin    *self,
                                                const char        *host,
                                                const char        *user,
                                                const char        *password,
                                                const char        *display_name,
                                                const char        *transport_protocol,
                                                gint               port,
                                                SipMediaEncryption media_encryption,
                                                gboolean           use_for_tel,
                                                gboolean           auto_connect);

G_END_DECLS
