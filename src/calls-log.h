/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* calls-log.c
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>

#ifndef CALLS_LOG_LEVEL_TRACE
# define CALLS_LOG_LEVEL_TRACE ((GLogLevelFlags)(1 << G_LOG_LEVEL_USER_SHIFT))
# define CALLS_LOG_DETAILED ((GLogLevelFlags)(8 << G_LOG_LEVEL_USER_SHIFT))
#endif

/* XXX: Should we use the semi-private g_log_structured_standard() API? */
#define CALLS_TRACE_MSG(...)                                            \
  g_log_structured_standard (G_LOG_DOMAIN, CALLS_LOG_LEVEL_TRACE,       \
                             __FILE__, G_STRINGIFY (__LINE__),          \
                             G_STRFUNC, __VA_ARGS__)


#define CALLS_TRACE(...)                                                \
  g_log_structured_standard (G_LOG_DOMAIN,                              \
                             CALLS_LOG_LEVEL_TRACE | CALLS_LOG_DETAILED, \
                             __FILE__, G_STRINGIFY (__LINE__),          \
                             G_STRFUNC, __VA_ARGS__)
#define CALLS_DEBUG(...)                                                \
  g_log_structured_standard (G_LOG_DOMAIN,                              \
                             G_LOG_LEVEL_DEBUG | CALLS_LOG_DETAILED,    \
                             __FILE__, G_STRINGIFY (__LINE__),          \
                             G_STRFUNC, __VA_ARGS__)

void  calls_log_init               (void);
void  calls_log_increase_verbosity (void);
guint calls_log_get_verbosity      (void);
int   calls_log_set_verbosity      (guint new_verbosity);
