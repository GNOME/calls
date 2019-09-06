/*
 * Copyright (C) 2019 Purism SPC
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

#ifndef CALLS__WAYLAND_CONFIG_H__
#define CALLS__WAYLAND_CONFIG_H__

#include "config.h"

#if WL_SCANNER_FOUND
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_WAYLAND
#define CALLS_WAYLAND
#endif // GDK_WINDOWING_WAYLAND
#endif // WL_SCANNER_FOUND

#endif /* CALLS__WAYLAND_CONFIG_H__ */
