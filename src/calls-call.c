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
#include "calls-manager.h"
#include "enum-types.h"
#include "util.h"

#include <glib/gi18n.h>


/**
 * SECTION:calls-call
 * @short_description: A call.
 * @Title: CallsCall
 *
 * This is the interface to a call.  It has a id, name and a
 * state.  Only the state changes after creation.  If the state is
 * #CALL_CALL_STATE_INCOMING, the call can be answered with #answer.
 * The call can also be hung up at any time with #hang_up.
 *
 * DTMF tones can be played the call using #send_dtmf
 * Valid characters for the key are 0-9, '*', '#', 'A',
 * 'B', 'C' and 'D'.
 */


enum {
  PROP_0,
  PROP_INBOUND,
  PROP_ID,
  PROP_NAME,
  PROP_STATE,
  PROP_PROTOCOL,
  PROP_SILENCED,
  N_PROPS,
};

enum {
  STATE_CHANGED,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

typedef struct {
  gboolean silenced;
} CallsCallPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (CallsCall, calls_call, G_TYPE_OBJECT)


static const char *
calls_call_real_get_id (CallsCall *self)
{
  return NULL;
}

static const char *
calls_call_real_get_name (CallsCall *self)
{
  return NULL;
}

static CallsCallState
calls_call_real_get_state (CallsCall *self)
{
  return 0;
}

static gboolean
calls_call_real_get_inbound (CallsCall *self)
{
  return FALSE;
}

static const char *
calls_call_real_get_protocol (CallsCall *self)
{
  return NULL;
}

static void
calls_call_real_answer (CallsCall *self)
{
}

static void
calls_call_real_hang_up (CallsCall *self)
{
}

static void
calls_call_real_send_dtmf_tone (CallsCall *self,
                                char       key)
{
  g_info ("Beep! (%c)", key);
}

static void
calls_call_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  CallsCall *self = CALLS_CALL (object);

  switch (prop_id)
    {
    case PROP_INBOUND:
      g_value_set_boolean (value, calls_call_get_inbound (self));
      break;

    case PROP_ID:
      g_value_set_string (value, calls_call_get_id (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, calls_call_get_name (self));
      break;

    case PROP_STATE:
      g_value_set_enum (value, calls_call_get_state (self));
      break;

    case PROP_PROTOCOL:
      g_value_set_string (value, calls_call_get_protocol (self));
      break;

    case PROP_SILENCED:
      g_value_set_boolean (value, calls_call_get_silenced (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
calls_call_class_init (CallsCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_call_get_property;

  klass->get_id = calls_call_real_get_id;
  klass->get_name = calls_call_real_get_name;
  klass->get_state = calls_call_real_get_state;
  klass->get_inbound = calls_call_real_get_inbound;
  klass->get_protocol = calls_call_real_get_protocol;
  klass->answer = calls_call_real_answer;
  klass->hang_up = calls_call_real_hang_up;
  klass->send_dtmf_tone = calls_call_real_send_dtmf_tone;

  properties[PROP_INBOUND] =
    g_param_spec_boolean ("inbound",
                          "Inbound",
                          "Whether the call is inbound",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The id the call is connected to if known",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name of the party the call is connected to, if the network provides it",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_STATE] =
    g_param_spec_enum ("state",
                       "State",
                       "The current state of the call",
                       CALLS_TYPE_CALL_STATE,
                       CALLS_CALL_STATE_ACTIVE,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_PROTOCOL] =
    g_param_spec_string ("protocol",
                         "Protocol",
                         "The protocol used for this call",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SILENCED] =
    g_param_spec_boolean ("silenced",
                          "Silenced",
                          "Whether the call ringing should be silenced",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * CallsCall::state-changed:
   * @self: The #CallsCall instance.
   * @new_state: The new state of the call.
   * @old_state: The old state of the call.
   *
   * This signal is emitted when the state of the call changes, for
   * example when it's answered or when the call is disconnected.
   */
  signals[STATE_CHANGED] =
    g_signal_new ("state-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2, CALLS_TYPE_CALL_STATE, CALLS_TYPE_CALL_STATE);
}

static void
calls_call_init (CallsCall *self)
{
}

/**
 * calls_call_get_id:
 * @self: a #CallsCall
 *
 * Get the id the call is connected to.  It is possible that this
 * could return NULL if the id is not known, for example if an
 * incoming PTSN call has no caller ID information.
 *
 * Returns: (transfer none): the id, or NULL
 */
const char *
calls_call_get_id (CallsCall *self)
{
  g_return_val_if_fail (CALLS_IS_CALL (self), NULL);

  return CALLS_CALL_GET_CLASS (self)->get_id (self);
}

/**
 * calls_call_get_name:
 * @self: a #CallsCall
 *
 * Get the name of the party the call is connected to, if the network
 * provides it.
 *
 * Returns the id, or NULL
 */
const char *
calls_call_get_name (CallsCall *self)
{
  g_return_val_if_fail (CALLS_IS_CALL (self), NULL);

  return CALLS_CALL_GET_CLASS (self)->get_name (self);
}

/**
 * calls_call_get_state:
 * @self: a #CallsCall
 *
 * Get the current state of the call.
 *
 * Returns: the state
 */
CallsCallState
calls_call_get_state (CallsCall *self)
{
  g_return_val_if_fail (CALLS_IS_CALL (self), 0);

  return CALLS_CALL_GET_CLASS (self)->get_state (self);
}

/**
 * calls_call_answer:
 * @self: a #CallsCall
 *
 * If the call is incoming, answer it.
 *
 */
void
calls_call_answer (CallsCall *self)
{
  g_return_if_fail (CALLS_IS_CALL (self));

  CALLS_CALL_GET_CLASS (self)->answer (self);
}

/**
 * calls_call_hang_up:
 * @self: a #CallsCall
 *
 * Hang up the call.
 *
 */
void
calls_call_hang_up (CallsCall *self)
{
  g_return_if_fail (CALLS_IS_CALL (self));

  CALLS_CALL_GET_CLASS (self)->hang_up (self);
}


/**
 * calls_call_get_inbound:
 * @self: a #CallsCall
 *
 * Get the direction of the call.
 *
 * Returns: TRUE if inbound, FALSE if outbound.
 */
gboolean
calls_call_get_inbound (CallsCall *self)
{
  g_return_val_if_fail (CALLS_IS_CALL (self), FALSE);

  return CALLS_CALL_GET_CLASS (self)->get_inbound (self);
}

/**
 * calls_call_get_protocol:
 * @self: a #CallsCall
 *
 * Get the protocol of the call (i.e. "tel", "sip")
 *
 * Returns: The protocol used or %NULL when unknown
 */
const char *
calls_call_get_protocol (CallsCall *self)
{
  g_return_val_if_fail (CALLS_IS_CALL (self), NULL);

  return CALLS_CALL_GET_CLASS (self)->get_protocol (self);
}

/**
 * calls_call_can_dtmf:
 * @self: a #CallsCall
 *
 * Returns: %TRUE if this call supports DTMF, %FALSE otherwise
 */
gboolean
calls_call_can_dtmf (CallsCall *self)
{
  g_return_val_if_fail (CALLS_IS_CALL (self), FALSE);

  return CALLS_CALL_GET_CLASS (self)->send_dtmf_tone != calls_call_real_send_dtmf_tone;
}

/**
 * calls_call_send_dtmf_tone:
 * @self: a #CallsCall
 * @key: which tone to start
 *
 * Start playing a DTMF tone for the specified key. Implementations
 * will stop playing the tone either after an implementation-specific
 * timeout.
 *
 */
void
calls_call_send_dtmf_tone (CallsCall *self,
                           gchar      key)
{
  g_return_if_fail (CALLS_IS_CALL (self));
  g_return_if_fail (dtmf_tone_key_is_valid (key));

  CALLS_CALL_GET_CLASS (self)->send_dtmf_tone (self, key);
}

/**
 * calls_call_get_contact:
 * @self: a #CallsCall
 *
 * This a convenience function to optain the #CallsBestMatch matching the
 * phone id of the #CallsCall.
 *
 * Returns: (transfer full): A #CallsBestMatch
 */
CallsBestMatch *
calls_call_get_contact (CallsCall *self)
{
  CallsContactsProvider *contacts_provider;

  g_return_val_if_fail (CALLS_IS_CALL (self), NULL);

  contacts_provider =
    calls_manager_get_contacts_provider (calls_manager_get_default ());

  return calls_contacts_provider_lookup_id (contacts_provider,
                                            calls_call_get_id (self));
}

/**
 * calls_call_silence_ring:
 * @self: a #CallsCall
 *
 * Inhibit ringing
 */
void
calls_call_silence_ring (CallsCall *self)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_if_fail (CALLS_IS_CALL (self));
  g_return_if_fail (calls_call_get_state (self) == CALLS_CALL_STATE_INCOMING);

  if (priv->silenced)
    return;

  priv->silenced = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SILENCED]);
}

/**
 * calls_call_get_silenced:
 * @self: a #CallsCall
 *
 * Returns: %TRUE if call has been silenced to not ring, %FALSE otherwise
 */
gboolean
calls_call_get_silenced (CallsCall *self)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_val_if_fail (CALLS_IS_CALL (self), FALSE);

  return priv->silenced;
}

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
                             const char     *nick)
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
