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
#include "calls-util.h"

#include <glib/gi18n.h>


/**
 * SECTION:calls-call
 * @short_description: A call.
 * @Title: CallsCall
 *
 * This is the interface to a call.  It has a id, name and a
 * state.  Only the state changes after creation.  If the state is
 * #CALLS_CALL_STATE_INCOMING, the call can be answered with #calls_call_answer.
 * The call can also be hung up at any time with #calls_call_hang_up.
 *
 * DTMF tones can be played to the call using #calls_call_send_dtmf_tone
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
  PROP_CALL_TYPE,
  PROP_ENCRYPTED,
  N_PROPS,
};

enum {
  STATE_CHANGED,
  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

typedef struct {
  char          *id;
  char          *name;
  CallsCallState state;
  gboolean       inbound;
  gboolean       encrypted;
  CallsCallType  call_type;
  gboolean       hung_up;
} CallsCallPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (CallsCall, calls_call, G_TYPE_OBJECT)


static const char *
calls_call_real_get_protocol (CallsCall *self)
{
  return get_protocol_from_address_with_fallback (calls_call_get_id (self));
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
calls_call_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  CallsCall *self = CALLS_CALL (object);
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  switch (prop_id) {
  case PROP_INBOUND:
    priv->inbound = g_value_get_boolean (value);
    if (priv->inbound)
      calls_call_set_state (self, CALLS_CALL_STATE_INCOMING);
    else
      calls_call_set_state (self, CALLS_CALL_STATE_DIALING);
    break;

  case PROP_ID:
    calls_call_set_id (self, g_value_get_string (value));
    break;

  case PROP_NAME:
    calls_call_set_name (self, g_value_get_string (value));
    break;

  case PROP_STATE:
    calls_call_set_state (self, g_value_get_enum (value));
    break;

  case PROP_CALL_TYPE:
    priv->call_type = g_value_get_enum (value);
    break;

  case PROP_ENCRYPTED:
    calls_call_set_encrypted (self, g_value_get_boolean (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}


static void
calls_call_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  CallsCall *self = CALLS_CALL (object);

  switch (prop_id) {
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

  case PROP_CALL_TYPE:
    g_value_set_enum (value, calls_call_get_call_type (self));
    break;

  case PROP_ENCRYPTED:
    g_value_set_boolean (value, calls_call_get_encrypted (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}


static void
calls_call_dispose (GObject *object)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (CALLS_CALL (object));

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (calls_call_parent_class)->dispose (object);
}

static void
calls_call_class_init (CallsCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_call_get_property;
  object_class->set_property = calls_call_set_property;
  object_class->dispose = calls_call_dispose;

  klass->get_protocol = calls_call_real_get_protocol;
  klass->answer = calls_call_real_answer;
  klass->hang_up = calls_call_real_hang_up;
  klass->send_dtmf_tone = calls_call_real_send_dtmf_tone;

  /**
   * CallsCall:inbound: (attributes org.gtk.Property.get=calls_call_get_inbound)
   *
   * %TRUE if the call is inbound, %FALSE otherwise.
   */
  properties[PROP_INBOUND] =
    g_param_spec_boolean ("inbound",
                          "Inbound",
                          "Whether the call is inbound",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  /**
   * CallsCall:id: (attributes org.gtk.Property.get=calls_call_get_id org.gtk.Property.set=calls_call_set_id)

   * Get the id (number, SIP address)  the call is connected to.  It is possible that this
   * could return %NULL if the id is not known, for example if an
   * incoming PTSN call has no caller ID information.
   */
  properties[PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The id the call is connected to if known",
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * CallsCall:name: (attributes org.gtk.Property.get=calls_call_get_name org.gtk.Property.set=calls_call_set_name)
   *
   * The (network provided) name of the call. For the name of a contact, see #calls_best_match_get_name
   */
  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name of the party the call is connected to, if the network provides it",
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * CallsCall:state: (attributes org.gtk.Property.get=calls_call_get_state org.gtk.Property.set=calls_call_set_state)
   *
   * The state of the call or %CALLS_CALL_STATE_UNKNOWN if unknown.
   */
  properties[PROP_STATE] =
    g_param_spec_enum ("state",
                       "State",
                       "The current state of the call",
                       CALLS_TYPE_CALL_STATE,
                       CALLS_CALL_STATE_UNKNOWN,
                       G_PARAM_READWRITE |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);

  /**
   * CallsCall:protocol: (attributes org.gtk.Property.get=calls_call_get_protocol)
   *
   * The protocol for this call, f.e. tel or sip
   */
  properties[PROP_PROTOCOL] =
    g_param_spec_string ("protocol",
                         "Protocol",
                         "The protocol used for this call",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * CallsCall:call-type: (attributes org.gtk.Property.get=calls_call_get_call_type)
   *
   * The type of this call or #CALLS_CALL_TYPE_UNKNOWN if unknown
   */
  properties[PROP_CALL_TYPE] =
    g_param_spec_enum ("call-type",
                       "Call type",
                       "The type of call (f.e. cellular, sip voice)",
                       CALLS_TYPE_CALL_TYPE,
                       CALLS_CALL_TYPE_UNKNOWN,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_STATIC_STRINGS);

  /**
   * CallsCall:encrypted:
   *
   * If the call is encrypted
   */

  properties[PROP_ENCRYPTED] =
    g_param_spec_boolean ("encrypted",
                          "encrypted",
                          "If the call is encrypted",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

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
 * calls_call_get_id: (attributes org.gtk.Method.get_property=id)
 * @self: a #CallsCall
 *
 * Returns: (transfer none) (nullable): the id, or %NULL if the id is unknown
 */
const char *
calls_call_get_id (CallsCall *self)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_val_if_fail (CALLS_IS_CALL (self), NULL);

  return priv->id;
}

/**
 * calls_call_set_id: (attributes org.gtk.Method.set_property=id)
 * @self: a #CallsCall
 * @id: the id of the remote party
 *
 * Set the id of the call. Some implementations might only be able to set
 * the id after construction.
 */
void
calls_call_set_id (CallsCall  *self,
                   const char *id)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_if_fail (CALLS_IS_CALL (self));

  if (g_strcmp0 (id, priv->id) == 0)
    return;

  g_free (priv->id);
  priv->id = g_strdup (id);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
}

/**
 * calls_call_get_name: (attributes org.gtk.Method.get_property=name)
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
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_val_if_fail (CALLS_IS_CALL (self), NULL);

  return priv->name;
}

/**
 * calls_call_set_name: (attributes org.gtk.Method.set_property=name)
 * @self: a #CallsCall
 * @name: the name to set
 *
 * Sets the name of the call as provided by the network.
 */
void
calls_call_set_name (CallsCall  *self,
                     const char *name)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_if_fail (CALLS_IS_CALL (self));

  g_clear_pointer (&priv->name, g_free);
  if (name)
    priv->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
}

/**
 * calls_call_get_state: (attributes org.gtk.Method.get_property=state)
 * @self: a #CallsCall
 *
 * Get the current state of the call.
 *
 * Returns: the state
 */
CallsCallState
calls_call_get_state (CallsCall *self)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_val_if_fail (CALLS_IS_CALL (self), CALLS_CALL_STATE_UNKNOWN);

  return priv->state;
}

/**
 * calls_call_set_state: (attributes org.gtk.Method.set_property=state org.gtk.Method.signal=state-changed)
 * @self: a #CallsCall
 * @state: a #CallsCallState
 *
 * Set the current state of the call.
 */
void
calls_call_set_state (CallsCall     *self,
                      CallsCallState state)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);
  CallsCallState old_state;

  g_return_if_fail (CALLS_IS_CALL (self));

  old_state = priv->state;

  if (old_state == state) {
    return;
  }

  priv->state = state;

  g_object_ref (G_OBJECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
  g_signal_emit_by_name (CALLS_CALL (self),
                         "state-changed",
                         state,
                         old_state);

  g_object_unref (G_OBJECT (self));
}

/**
 * calls_call_get_call_type: (attributes org.gtk.Method.get_property=call-type)
 * @self: a #CallsCall
 *
 * Returns: The type of call, or #CALLS_CALL_TYPE_UNKNOWN if not known.
 */
CallsCallType
calls_call_get_call_type (CallsCall *self)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_val_if_fail (CALLS_IS_CALL (self), CALLS_CALL_TYPE_UNKNOWN);

  return priv->call_type;
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
  CallsCallPrivate *priv;

  g_return_if_fail (CALLS_IS_CALL (self));
  priv = calls_call_get_instance_private (self);

  priv->hung_up = TRUE;

  CALLS_CALL_GET_CLASS (self)->hang_up (self);
}


/**
 * calls_call_get_inbound: (attributes org.gtk.Method.get_property=inbound)
 * @self: a #CallsCall
 *
 * Get the direction of the call.
 *
 * Returns: TRUE if inbound, FALSE if outbound.
 */
gboolean
calls_call_get_inbound (CallsCall *self)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_val_if_fail (CALLS_IS_CALL (self), FALSE);

  return priv->inbound;
}

/**
 * calls_call_get_protocol: (attributes org.gtk.Method.get_property=protocol)
 * @self: a #CallsCall
 *
 * Get the protocol of the call (i.e. "tel", "sip")
 *
 * Returns: (transfer none): The protocol used or %NULL when unknown
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

  if (value) {
    *state = (CallsCallState) value->value;
    ret = TRUE;
  } else {
    ret = FALSE;
  }

  g_type_class_unref (klass);
  return ret;
}

/**
 * calls_call_get_encrypted:
 * @self: A #CallsCall
 *
 * Returns: %TRUE if the call is encrypted, %FALSE otherwise.
 */
gboolean
calls_call_get_encrypted (CallsCall *self)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_val_if_fail (CALLS_IS_CALL (self), FALSE);

  return priv->encrypted;
}

/**
 * calls_call_set_encrypted:
 * @self: A #CallsCall
 * @encrypted: A boolean indicating if the call is encrypted
 *
 * Set whether the call is encrypted or not.
 */
void
calls_call_set_encrypted (CallsCall *self,
                          gboolean   encrypted)
{
  CallsCallPrivate *priv = calls_call_get_instance_private (self);

  g_return_if_fail (CALLS_IS_CALL (self));

  if (priv->encrypted == encrypted)
    return;

  g_debug ("Encryption %sabled", encrypted ? "en" : "dis");

  priv->encrypted = encrypted;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENCRYPTED]);
}

/**
 * calls_call_get_we_hung_up:
 * @self: The call
 *
 * Get whether we hung up or the remote side.
 *
 * Returns: `TRUE` if we hung up, otherwise `FALSE`
 */
gboolean
calls_call_get_we_hung_up (CallsCall *self)
{
  CallsCallPrivate *priv;

  g_return_val_if_fail (CALLS_IS_CALL (self), FALSE);
  priv = calls_call_get_instance_private (self);

  return priv->hung_up;
}
