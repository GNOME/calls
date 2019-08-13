/*
 * Copyright (C) 2018, 2019 Purism SPC
 * SPDX-License-Identifier: GPL-3.0+
 * Author: Guido Günther <agx@sigxcpu.org>
 *
 * Copied from phosh and modified for Calls
 * by Bob Ham <bob.ham@puri.sm>
 */

#pragma once

#include <glib-object.h>

void calls_session_register (const char *client_id);
void calls_session_unregister (void);
