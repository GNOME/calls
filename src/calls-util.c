/*
 * Copyright (C) 2018, 2019 Purism SPC
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
 * Author: Bob Ham <bob.ham@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "calls-util.h"

#include <sys/socket.h>
#include <netdb.h>


gboolean
calls_date_time_is_same_day (GDateTime *a,
                             GDateTime *b)
{
#define eq(member)                              \
  (g_date_time_get_##member (a) ==              \
   g_date_time_get_##member (b))

  return
    eq (year)
    &&
    eq (month)
    &&
    eq (day_of_month);

#undef eq
}


gboolean
calls_date_time_is_yesterday (GDateTime *now,
                              GDateTime *t)
{
  GDateTime *yesterday;
  gboolean same_day;

  yesterday = g_date_time_add_days (now, -1);

  same_day = calls_date_time_is_same_day (yesterday, t);

  g_date_time_unref (yesterday);

  return same_day;
}


gboolean
calls_date_time_is_same_year (GDateTime *a,
                              GDateTime *b)
{
  return
    g_date_time_get_year (a) ==
    g_date_time_get_year (b);
}


gboolean
calls_number_is_ussd (const char *number)
{
  /* USSD numbers start with *, #, **, ## or *# and are finished by # */
  if (!number ||
      (*number != '*' && *number != '#'))
    return FALSE;

  number++;

  if (*number == '#')
    number++;

  while (g_ascii_isdigit (*number) || *number == '*')
    number++;

  if (g_str_equal (number, "#"))
    return TRUE;

  return FALSE;
}

/**
 * calls_find_in_model:
 * @list: A #GListModel
 * @item: The #gpointer to find
 * @position: (out) (optional): The first position of @item, if it was found.
 *
 * Returns: %TRUE if @list contains @item, %FALSE otherwise.
 */
gboolean
calls_find_in_model (GListModel *list,
                     gpointer    item,
                     guint      *position)
{
  GListStore *store = (GListStore *) list;
  guint count;

  g_return_val_if_fail (G_IS_LIST_MODEL (list), FALSE);

  if (G_IS_LIST_STORE (store))
    return g_list_store_find (store,
                              item,
                              position);

  count = g_list_model_get_n_items (list);

  for (guint i = 0; i < count; i++) {
    g_autoptr (GObject) object = NULL;

    object = g_list_model_get_item (list, i);

    if (object == item) {
      if (position)
        *position = i;
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * get_protocol_from_address:
 * @target: The target address
 *
 * simply checks for the the scheme of an address without doing any validation
 *
 * Returns: The protocol used for address, or NULL if could not determine
 */
const char *
get_protocol_from_address (const char *target)
{
  g_autofree char *lower = NULL;

  g_return_val_if_fail (target, NULL);

  lower = g_ascii_strdown (target, -1);

  if (g_str_has_prefix (lower, "sips:"))
    return "sips";

  if (g_str_has_prefix (lower, "sip:"))
    return "sip";

  if (g_str_has_prefix (lower, "tel:"))
    return "tel";

  /* could not determine the protocol (which most probably means it's a telephone number) */
  return NULL;
}

/**
 * get_protocol_from_address_with_fallback:
 * @target: The address to check
 *
 * simply checks for the the scheme of an address without doing any validation
 *
 * Returns: The protocol used for address, or "tel" as a fallback
 */
const char *
get_protocol_from_address_with_fallback (const char *target)
{
  const char *protocol = get_protocol_from_address (target);

  if (!protocol)
    protocol = "tel";

  return protocol;
}

/**
 * dtmf_tone_is_valid:
 * @key:
 *
 * Checks if @key is a valid DTMF keytone
 *
 * Returns: %TRUE if @key is 0-9, A-D, * or #, %FALSE otherwise
 */
gboolean
dtmf_tone_key_is_valid (gchar key)
{
  return
    (key >= '0' && key <= '9')
    || (key >= 'A' && key <= 'D')
    ||  key == '*'
    ||  key == '#';
}


static const char * const type_icon_name[] = {
  "call-arrow-outgoing-symbolic",
  "call-arrow-outgoing-missed-symbolic",
  "call-arrow-incoming-symbolic",
  "call-arrow-incoming-missed-symbolic",
};

/**
 * get_call_icon_type_name:
 * @inbound: Whether the call was inbound
 * @missed: Whether the call was missed
 *
 * Returns: (transfer null): The icon symbolic name to use in the history, etc
 */
const char *
get_call_icon_symbolic_name (gboolean inbound,
                             gboolean missed)
{
  guint index = 0;

  index = ((inbound) << 1) + missed;

  return type_icon_name[index];
}

/**
 * get_address_family_for_ip:
 * @ip: The IP address to check
 * @only_configured_interfaces: Only consider address families of configured
 * network interfaces
 *
 * Returns: #AF_INET for IPv4, #AF_INET6 for IPv6 and #AF_UNSPEC otherwise.
 * If @only_configured_interfaces is #TRUE and the resulting address family of @ip
 * is not configured on any network interface, it will also return #AF_UNSPEC
 */
int
get_address_family_for_ip (const char *ip,
                           gboolean    only_configured_interfaces)
{
  struct addrinfo hints = { 0 };
  struct addrinfo *result;
  int family = AF_UNSPEC;
  int res_gai;

  g_return_val_if_fail (!STR_IS_NULL_OR_EMPTY (ip), AF_UNSPEC);

  hints.ai_flags = AI_V4MAPPED | AI_NUMERICHOST;
  if (only_configured_interfaces)
    hints.ai_flags |= AI_ADDRCONFIG;
  hints.ai_family = AF_UNSPEC;


  res_gai = getaddrinfo (ip, NULL, &hints, &result);
  if (res_gai != 0) {
    g_info ("Cannot get address information for host %s: %s",
            ip,
            gai_strerror (res_gai));

    return AF_UNSPEC;
  }

  family = result->ai_family;

  freeaddrinfo (result);

  if (family != AF_INET && family != AF_INET6) {
    g_warning ("Address information for host %s indicates non internet host", ip);

    return AF_UNSPEC;
  }

  return family;
}
