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

#include "calls-sip-provider.h"

#include <adwaita.h>

G_BEGIN_DECLS

#define CALLS_TYPE_SIP_ACCOUNT_WIDGET (calls_sip_account_widget_get_type ())

G_DECLARE_FINAL_TYPE (CallsSipAccountWidget, calls_sip_account_widget, CALLS, SIP_ACCOUNT_WIDGET, AdwBin)

CallsSipAccountWidget *calls_sip_account_widget_new           (CallsSipProvider      *provider);
void            calls_sip_account_widget_set_origin    (CallsSipAccountWidget *self,
                                                        CallsSipOrigin        *origin);
CallsSipOrigin *calls_sip_account_widget_get_origin    (CallsSipAccountWidget *self);

G_END_DECLS
