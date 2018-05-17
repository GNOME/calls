/*
 * Copyright (C) 2018 Purism SPC
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

#include "calls-call.h"
#include "calls-message-source.h"
#include "enum-types.h"
#include "util.h"


void
calls_call_state_to_string (GString *string,
                            CallsCallState state)
{
  GEnumClass *klass;
  GEnumValue *value;

  klass = g_type_class_ref (CALLS_TYPE_CALL_STATE);
  value = g_enum_get_value (klass, (gint)state);

  g_string_assign (string, value->value_nick);
  string->str[0] = g_ascii_toupper (string->str[0]);

  g_type_class_unref (klass);
}


gboolean
calls_call_state_parse_nick (CallsCallState *state,
                             const gchar    *nick)
{
  GEnumClass *klass;
  GEnumValue *value;
  gboolean ret;

  g_return_val_if_fail (state != NULL, FALSE);
  g_return_val_if_fail (nick != NULL, FALSE);

  klass = g_type_class_ref (CALLS_TYPE_CALL_STATE);
  value = g_enum_get_value_by_nick (klass, nick);

  if (value)
    {
      *state = (CallsCallState) value->value;
      ret = TRUE;
    }
  else
    {
      ret = FALSE;
    }

  g_type_class_unref (klass);
  return ret;
}


/**
 * SECTION:calls-call
 * @short_description: A call.
 * @Title: CallsCall
 */


G_DEFINE_INTERFACE (CallsCall, calls_call, CALLS_TYPE_MESSAGE_SOURCE);

enum {
  SIGNAL_STATE_CHANGED,
  SIGNAL_LAST_SIGNAL,
};
static guint signals [SIGNAL_LAST_SIGNAL];

static void
calls_call_default_init (CallsCallInterface *iface)
{
  GType arg_types = CALLS_TYPE_CALL_STATE;

  signals[SIGNAL_STATE_CHANGED] =
    g_signal_newv ("state-changed",
		   G_TYPE_FROM_INTERFACE (iface),
		   G_SIGNAL_RUN_LAST,
		   NULL, NULL, NULL, NULL,
		   G_TYPE_NONE,
		   1, &arg_types);
}


#define DEFINE_CALL_FUNC(function,rettype,errval)       \
  CALLS_DEFINE_IFACE_FUNC(call, Call, CALL,             \
                          function, rettype, errval)

#define DEFINE_CALL_FUNC_VOID(function)                         \
  CALLS_DEFINE_IFACE_FUNC_VOID(call, Call, CALL, function)


DEFINE_CALL_FUNC(get_number, const gchar *, NULL);
DEFINE_CALL_FUNC(get_name, const gchar *, NULL);

/**
 * calls_call_get_state:
 * @self: a #CallsCall
 *
 * Get the current state of the call
 *
 * Returns: the state
 */
DEFINE_CALL_FUNC(get_state, CallsCallState, ((CallsCallState)0));

DEFINE_CALL_FUNC_VOID(answer);
DEFINE_CALL_FUNC_VOID(hang_up);


static inline gboolean
tone_key_is_valid (gchar key)
{
  return
       (key >= '0' && key <= '9')
    || (key >= 'A' && key <= 'D')
    ||  key == '*'
    ||  key == '#';
}

#define DEFINE_CALL_TONE_FUNC(which)                    \
  void                                                  \
  calls_call_tone_##which (CallsCall *self,             \
                           gchar      key)              \
  {                                                     \
    CallsCallInterface *iface;                          \
                                                        \
    g_return_if_fail (CALLS_IS_CALL (self));            \
    g_return_if_fail (tone_key_is_valid (key));         \
                                                        \
    iface = CALLS_CALL_GET_IFACE (self);                \
    g_return_if_fail (iface->tone_##which != NULL);     \
                                                        \
    return iface->tone_##which (self, key);             \
  }

DEFINE_CALL_TONE_FUNC (start);
DEFINE_CALL_TONE_FUNC (stop);
