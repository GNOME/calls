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

#include <glib/gi18n.h>


void
calls_call_state_to_string (GString        *string,
                            CallsCallState  state)
{
  GEnumClass *klass;
  GEnumValue *value;

  klass = g_type_class_ref (CALLS_TYPE_CALL_STATE);

  value = g_enum_get_value (klass, (gint)state);
  if (!value)
    {
      return g_string_printf (string,
                              "Unknown call state (%d)",
                              (gint)state);
    }

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
 *
 * This is the interface to a call.  It has a number, name and a
 * state.  Only the state changes after creation.  If the state is
 * #CALL_CALL_STATE_INCOMING, the call can be answered with #answer.
 * The call can also be hung up at any time with #hang_up.
 *
 * DTMF tones can be played the call using #tone_start and
 * #tone_stop.  Valid characters for the key are 0-9, '*', '#', 'A',
 * 'B', 'C' and 'D'.
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
  GType arg_types[2] =
    {
      CALLS_TYPE_CALL_STATE,
      CALLS_TYPE_CALL_STATE
    };

  g_object_interface_install_property (
    iface,
    g_param_spec_boolean ("inbound",
                          "Inbound",
                          "Whether the call is inbound",
                          FALSE,
                          G_PARAM_READABLE));

  g_object_interface_install_property (
    iface,
    g_param_spec_string ("number",
                         "Number",
                         "The number the call is connected to if known",
                         NULL,
                         G_PARAM_READABLE));

  g_object_interface_install_property (
    iface,
    g_param_spec_string ("name",
                         "Name",
                         "The name of the party the call is connected to, if the network provides it",
                         NULL,
                         G_PARAM_READABLE));

  g_object_interface_install_property (
    iface,
    g_param_spec_enum ("state",
                       "State",
                       "The current state of the call",
                       CALLS_TYPE_CALL_STATE,
                       CALLS_CALL_STATE_ACTIVE,
                       G_PARAM_READABLE));

  /**
   * CallsCall::state-changed:
   * @self: The #CallsCall instance.
   * @new_state: The new state of the call.
   * @old_state: The old state of the call.
   *
   * This signal is emitted when the state of the call changes, for
   * example when it's answered or when the call is disconnected.
   */
  signals[SIGNAL_STATE_CHANGED] =
    g_signal_newv ("state-changed",
		   G_TYPE_FROM_INTERFACE (iface),
		   G_SIGNAL_RUN_LAST,
		   NULL, NULL, NULL, NULL,
		   G_TYPE_NONE,
		   2, arg_types);
}


#define DEFINE_CALL_GETTER(prop,rettype,errval) \
  CALLS_DEFINE_IFACE_GETTER(call, Call, CALL, prop, rettype, errval)

#define DEFINE_CALL_FUNC_VOID(function)                         \
  CALLS_DEFINE_IFACE_FUNC_VOID(call, Call, CALL, function)


/**
 * calls_call_get_number:
 * @self: a #CallsCall
 *
 * Get the number the call is connected to.  It is possible that this
 * could return NULL if the number is not known, for example if an
 * incoming PTSN call has no caller ID information.
 *
 * Returns: the number, or NULL
 */
DEFINE_CALL_GETTER(number, const gchar *, NULL);

/**
 * calls_call_get_name:
 * @self: a #CallsCall
 *
 * Get the name of the party the call is connected to, if the network
 * provides it.
 *
 * Returns: the number, or NULL
 */
DEFINE_CALL_GETTER(name, const gchar *, NULL);

/**
 * calls_call_get_state:
 * @self: a #CallsCall
 *
 * Get the current state of the call.
 *
 * Returns: the state
 */
DEFINE_CALL_GETTER(state, CallsCallState, ((CallsCallState)0));

/**
 * calls_call_answer:
 * @self: a #CallsCall
 *
 * If the call is incoming, answer it.
 *
 */
DEFINE_CALL_FUNC_VOID(answer);

/**
 * calls_call_hang_up:
 * @self: a #CallsCall
 *
 * Hang up the call.
 *
 */
DEFINE_CALL_FUNC_VOID(hang_up);


/**
 * calls_call_get_inbound:
 * @self: a #CallsCall
 *
 * Get the direction of the call.
 *
 * Returns: TRUE if inbound, FALSE if outbound.
 */
DEFINE_CALL_GETTER(inbound, gboolean, FALSE);

static inline gboolean
tone_key_is_valid (gchar key)
{
  return
       (key >= '0' && key <= '9')
    || (key >= 'A' && key <= 'D')
    ||  key == '*'
    ||  key == '#';
}


/**
 * calls_call_tone_start:
 * @self: a #CallsCall
 * @key: which tone to start
 *
 * Start playing a DTMF tone for the specified key.  Implementations
 * will stop playing the tone either after an implementation-specific
 * timeout, or after #calls_call_tone_stop is called with the same
 * value for @key.
 *
 */
void
calls_call_tone_start (CallsCall *self,
                       gchar      key)
{
  CallsCallInterface *iface;

  g_return_if_fail (CALLS_IS_CALL (self));
  g_return_if_fail (tone_key_is_valid (key));

  iface = CALLS_CALL_GET_IFACE (self);
  g_return_if_fail (iface->tone_start != NULL);

  iface->tone_start (self, key);
}

/**
 * calls_call_tone_stoppable:
 * @self: a #CallsCall
 *
 * Determine whether tones for this call can be stopped by calling
 * #calls_call_tone_stop.  Some implementations will only allow
 * fixed-length tones to be played.  In that case, this function
 * should return FALSE.
 *
 * Returns: whether calls to #calls_call_tone_stop will do anything
 *
 */
gboolean
calls_call_tone_stoppable (CallsCall *self)
{
  CallsCallInterface *iface;

  g_return_val_if_fail (CALLS_IS_CALL (self), FALSE);

  iface = CALLS_CALL_GET_IFACE (self);

  return iface->tone_stop != NULL;
}

/**
 * calls_call_tone_stop:
 * @self: a #CallsCall
 * @key: which tone to stop
 *
 * Stop playing a DTMF tone previously started with
 * #calls_call_tone_start.
 *
 */
void
calls_call_tone_stop (CallsCall *self,
                      gchar      key)
{
  CallsCallInterface *iface;

  g_return_if_fail (CALLS_IS_CALL (self));
  g_return_if_fail (tone_key_is_valid (key));

  iface = CALLS_CALL_GET_IFACE (self);
  if (!iface->tone_stop)
    {
      return;
    }

  iface->tone_stop (self, key);
}
