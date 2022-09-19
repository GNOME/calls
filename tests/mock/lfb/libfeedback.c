/*
 * Copyright (C) 2020 Purism SPC
 * SPDX-License-Identifier: LGPL-2.1+
 * Author: Guido Günther <agx@sigxcpu.org>
 */
#include "libfeedback.h"

static gboolean          _initted;


gboolean
lfb_init (const gchar *app_id, GError **error)
{
  _initted = TRUE;
  g_debug ("libfeedback mock library initialized");
  return TRUE;
}

void
lfb_uninit (void)
{
  _initted = FALSE;
}

gboolean
lfb_is_initted (void)
{
  return _initted;
}
