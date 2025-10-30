/*
 * Copyright (C) 2025 Phosh.mobi e.V.
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
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define CALLS_TYPE_MEDIA_PLAYBACK (calls_media_playback_get_type ())

G_DECLARE_FINAL_TYPE (CallsMediaPlayback, calls_media_playback, CALLS, MEDIA_PLAYBACK, GObject)


CallsMediaPlayback   *calls_media_playback_new           (void);
void                  calls_media_playback_play_calling  (CallsMediaPlayback *self);
void                  calls_media_playback_stop_calling  (CallsMediaPlayback *self);
void                  calls_media_playback_play_busy     (CallsMediaPlayback *self);
void                  calls_media_playback_stop_busy     (CallsMediaPlayback *self);

G_END_DECLS
