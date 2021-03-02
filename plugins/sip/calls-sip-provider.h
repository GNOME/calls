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

#include <glib-object.h>

#include "calls-provider.h"

G_BEGIN_DECLS

#define CALLS_TYPE_SIP_PROVIDER (calls_sip_provider_get_type ())

G_DECLARE_FINAL_TYPE (CallsSipProvider, calls_sip_provider, CALLS, SIP_PROVIDER, CallsProvider)

CallsSipProvider *calls_sip_provider_new                    ();
void              calls_sip_provider_add_origin             (CallsSipProvider *self,
                                                             const gchar      *name,
                                                             const gchar      *user,
                                                             const gchar      *password,
                                                             const gchar      *host,
                                                             gint              port,
                                                             gint              local_port,
                                                             const gchar      *protocol,
                                                             gboolean          direct_connection,
                                                             gboolean          auto_connect);

G_END_DECLS
