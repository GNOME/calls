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

#define _GNU_SOURCE

#include "calls-log.h"

#include <string.h>
#include <glib.h>
#include <stdio.h>
#include <unistd.h>

#define DEFAULT_DOMAIN_PREFIX "Calls"

static char *domains;
static guint verbosity;
static gboolean any_domain;
static gboolean stderr_is_journal;

static void
log_str_append_log_domain (GString    *log_str,
                           const char *log_domain,
                           gboolean    color)
{
  static const char *colors[] = {
    "\033[1;32m",
    "\033[1;33m",
    "\033[1;35m",
    "\033[1;36m",
    "\033[1;91m",
    "\033[1;92m",
    "\033[1;93m",
    "\033[1;94m",
    "\033[1;95m",
    "\033[1;96m",
  };
  guint i;

  g_assert (log_domain && *log_domain);

  i = g_str_hash (log_domain) % G_N_ELEMENTS (colors);

  if (color)
    g_string_append (log_str, colors[i]);
  g_string_append_printf (log_str, "%20s", log_domain);

  if (color)
    g_string_append (log_str, "\033[0m");
}

static const char *
get_log_level_prefix (GLogLevelFlags log_level,
                      gboolean       use_color)
{
  /* Ignore custom flags set */
  log_level = log_level & ~CALLS_LOG_DETAILED;

  if (use_color) {
    switch ((int) log_level) {       /* Same colors as used in GLib */
    case G_LOG_LEVEL_ERROR:       return "   \033[1;31mERROR\033[0m";
    case G_LOG_LEVEL_CRITICAL:    return "\033[1;35mCRITICAL\033[0m";
    case G_LOG_LEVEL_WARNING:     return " \033[1;33mWARNING\033[0m";
    case G_LOG_LEVEL_MESSAGE:     return " \033[1;32mMESSAGE\033[0m";
    case G_LOG_LEVEL_INFO:        return "    \033[1;32mINFO\033[0m";
    case G_LOG_LEVEL_DEBUG:       return "   \033[1;32mDEBUG\033[0m";
    case CALLS_LOG_LEVEL_TRACE:   return "   \033[1;36mTRACE\033[0m";
    default:                      return " UNKNOWN";
    }
  } else {
    switch ((int) log_level) {
    case G_LOG_LEVEL_ERROR:      return "   ERROR";
    case G_LOG_LEVEL_CRITICAL:   return "CRITICAL";
    case G_LOG_LEVEL_WARNING:    return " WARNING";
    case G_LOG_LEVEL_MESSAGE:    return " MESSAGE";
    case G_LOG_LEVEL_INFO:       return "    INFO";
    case G_LOG_LEVEL_DEBUG:      return "   DEBUG";
    case CALLS_LOG_LEVEL_TRACE:  return "   TRACE";
    default:                     return " UNKNOWN";
    }
  }
}

static GLogWriterOutput
calls_log_write (GLogLevelFlags   log_level,
                 const char      *log_domain,
                 const char      *log_message,
                 const GLogField *fields,
                 gsize            n_fields,
                 gpointer         user_data)
{
  g_autoptr (GString) log_str = NULL;
  FILE *stream;
  gboolean can_color;

  if (stderr_is_journal &&
      g_log_writer_journald (log_level, fields, n_fields, user_data) == G_LOG_WRITER_HANDLED)
    return G_LOG_WRITER_HANDLED;

  if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING))
    stream = stderr;
  else
    stream = stdout;

  log_str = g_string_new (NULL);

  /* Add local time */
  {
    char buffer[32];
    struct tm tm_now;
    time_t sec_now;
    gint64 now;

    now = g_get_real_time ();
    sec_now = now / G_USEC_PER_SEC;
    tm_now = *localtime (&sec_now);
    strftime (buffer, sizeof (buffer), "%H:%M:%S", &tm_now);

    g_string_append_printf (log_str, "%s.%04d ", buffer,
                            (int) ((now % G_USEC_PER_SEC) / 100));
  }

  can_color = g_log_writer_supports_color (fileno (stream));
  log_str_append_log_domain (log_str, log_domain, can_color);
  g_string_append_printf (log_str, "[%5d]:", getpid ());

  g_string_append_printf (log_str, "%s: ", get_log_level_prefix (log_level, can_color));

  if (log_level & CALLS_LOG_DETAILED) {
    const char *code_func = NULL, *code_line = NULL;
    for (guint i = 0; i < n_fields; i++) {
      const GLogField *field = &fields[i];

      if (!code_func && g_strcmp0 (field->key, "CODE_FUNC") == 0)
        code_func = field->value;
      else if (!code_line && g_strcmp0 (field->key, "CODE_LINE") == 0)
        code_line = field->value;

      if (code_func && code_line)
        break;
    }

    if (code_func) {
      g_string_append_printf (log_str, "%s():", code_func);

      if (code_line)
        g_string_append_printf (log_str, "%s:", code_line);
      g_string_append_c (log_str, ' ');
    }
  }

  g_string_append (log_str, log_message);

  fprintf (stream, "%s\n", log_str->str);
  fflush (stream);

  return G_LOG_WRITER_HANDLED;
}

static GLogWriterOutput
calls_log_handler (GLogLevelFlags   log_level,
                   const GLogField *fields,
                   gsize            n_fields,
                   gpointer         user_data)
{
  const char *log_domain = NULL;
  const char *log_message = NULL;

  /* If domain is “all” show logs upto debug regardless of the verbosity */
  switch ((int) log_level) {
  case G_LOG_LEVEL_MESSAGE:
    if (any_domain && domains)
      break;
    if (verbosity < 1)
      return G_LOG_WRITER_HANDLED;
    break;

  case G_LOG_LEVEL_INFO:
    if (any_domain && domains)
      break;
    if (verbosity < 2)
      return G_LOG_WRITER_HANDLED;
    break;

  case G_LOG_LEVEL_DEBUG:
    if (any_domain && domains)
      break;
    if (verbosity < 3)
      return G_LOG_WRITER_HANDLED;
    break;

  case CALLS_LOG_LEVEL_TRACE:
    if (verbosity < 4)
      return G_LOG_WRITER_HANDLED;
    break;

  default:
    break;
  }

  for (guint i = 0; (!log_domain || !log_message) && i < n_fields; i++) {
    const GLogField *field = &fields[i];

    if (g_strcmp0 (field->key, "GLIB_DOMAIN") == 0)
      log_domain = field->value;
    else if (g_strcmp0 (field->key, "MESSAGE") == 0)
      log_message = field->value;
  }

  if (!log_domain)
    log_domain = "**";

  /* Skip logs from other domains if verbosity level is low */
  if (any_domain && !domains &&
      verbosity < 5 &&
      log_level > G_LOG_LEVEL_MESSAGE &&
      !strcasestr (log_domain, DEFAULT_DOMAIN_PREFIX))
    return G_LOG_WRITER_HANDLED;

  /* GdkPixbuf logs are too much verbose, skip unless asked not to. */
  if (log_level >= G_LOG_LEVEL_MESSAGE &&
      verbosity < 7 &&
      g_strcmp0 (log_domain, "GdkPixbuf") == 0 &&
      (!domains || !strcasestr (domains, log_domain)))
    return G_LOG_WRITER_HANDLED;

  if (!log_message)
    log_message = "(NULL) message";

  if (any_domain || strcasestr (domains, log_domain))
    return calls_log_write (log_level, log_domain, log_message,
                            fields, n_fields, user_data);

  return G_LOG_WRITER_HANDLED;
}

static void
calls_log_finalize (void)
{
  g_clear_pointer (&domains, g_free);
}

void
calls_log_init (void)
{
  static gsize initialized = 0;

  if (g_once_init_enter (&initialized)) {
    domains = g_strdup (g_getenv ("G_MESSAGES_DEBUG"));

    if (domains && !*domains)
      g_clear_pointer (&domains, g_free);

    if (!domains || g_str_equal (domains, "all"))
      any_domain = TRUE;

    stderr_is_journal = g_log_writer_is_journald (fileno (stderr));
    g_log_set_writer_func (calls_log_handler, NULL, NULL);
    g_once_init_leave (&initialized, 1);
    atexit (calls_log_finalize);
  }
}

void
calls_log_increase_verbosity (void)
{
  verbosity++;
}

guint
calls_log_get_verbosity (void)
{
  return verbosity;
}


int
calls_log_set_verbosity (guint new_verbosity)
{
  int diff = new_verbosity - verbosity;

  if (new_verbosity == verbosity)
    return 0;

  verbosity = new_verbosity;

  return diff;
}
