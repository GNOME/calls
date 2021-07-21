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

#define G_LOG_DOMAIN "CallsSecretStore"

#include "calls-secret-store.h"

#include <libsecret/secret.h>

const SecretSchema *
calls_secret_get_schema (void)
{
  static const SecretSchema schema = {
    /** SECRET_SCHEMA_DONT_MATCH_NAME is used as a workaround for a bug in gnome-keyring
     *  which prevents cold keyrings from being searched (and hence does not prompt for unlocking)
     *  see https://gitlab.gnome.org/GNOME/gnome-keyring/-/issues/89 and
     *  https://gitlab.gnome.org/GNOME/libsecret/-/issues/7 for more information
     */
    "sm.puri.Calls", SECRET_SCHEMA_DONT_MATCH_NAME,
    {
      { CALLS_USERNAME_ATTRIBUTE, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { CALLS_SERVER_ATTRIBUTE, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { CALLS_PROTOCOL_ATTRIBUTE, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { NULL, 0 },
    }
  };

  return &schema;
}
