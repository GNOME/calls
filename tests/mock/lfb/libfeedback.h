/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

#include "lfb-enums.h"
#include "lfb-event.h"

gboolean    lfb_init (const gchar *app_id, GError **error);
void        lfb_uninit (void);
gboolean    lfb_is_initted (void);

G_END_DECLS
