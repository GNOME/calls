/*
 * Copyright (C) 2021-2022 Purism SPC
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

#include "calls-sip-util.h"

gboolean
check_ipv6 (const char *host)
{
  /* TODO DNS SRV records to determine whether or not to use IPv6 */
  return FALSE;
}


gboolean
check_sips (const char *addr)
{
  /* To keep it simple we only check if the URL starts with "sips:" */
  return g_str_has_prefix (addr, "sips:");
}


const gchar *
get_protocol_prefix (const char *protocol)
{
  if (g_strcmp0 (protocol, "UDP") == 0 ||
      g_strcmp0 (protocol, "TCP") == 0)
    return "sip";

  if (g_strcmp0 (protocol, "TLS") == 0)
    return "sips";

  return NULL;
}


gboolean
protocol_is_valid (const char *protocol)
{
  return g_strcmp0 (protocol, "UDP") == 0 ||
         g_strcmp0 (protocol, "TCP") == 0 ||
         g_strcmp0 (protocol, "TLS") == 0;
}
