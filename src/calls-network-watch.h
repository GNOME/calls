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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Calls. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_NETWORK_WATCH (calls_network_watch_get_type ())

G_DECLARE_FINAL_TYPE (CallsNetworkWatch, calls_network_watch, CALLS, NETWORK_WATCH, GObject)

CallsNetworkWatch *calls_network_watch_get_default       (void);
const char        *calls_network_watch_get_ipv4          (CallsNetworkWatch *self);
const char        *calls_network_watch_get_ipv6          (CallsNetworkWatch *self);

G_END_DECLS
