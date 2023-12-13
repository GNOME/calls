/*
 * Copyright (C) 2021, 2022 Purism SPC
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

#include "calls-ui-call-data.h"
#include "calls-contacts-provider.h"
#include "calls-manager.h"

#include <cui-call.h>
#include <cui-enums.h>

enum {
  PROP_0,
  PROP_CALL,
  PROP_ORIGIN_ID,
  PROP_INBOUND,
  PROP_PROTOCOL,
  PROP_DISPLAY_NAME,
  PROP_ID,
  PROP_STATE,
  PROP_ENCRYPTED,
  PROP_CAN_DTMF,
  PROP_AVATAR_ICON,
  PROP_ACTIVE_TIME,
  PROP_SILENCED,
  PROP_UI_ACTIVE,
  PROP_LAST_PROP
};

enum {
  STATE_CHANGED,
  N_SIGNALS,
};

static GParamSpec *props[PROP_LAST_PROP];
static guint signals[N_SIGNALS];

struct _CallsUiCallData {
  GObject         parent_instance;

  CallsCall      *call;
  CallsBestMatch *best_match;

  GTimer         *timer;
  gdouble         active_time;
  guint           timer_id;

  CuiCallState    state;
  char           *origin_id;
  gboolean        silenced;

  gboolean        ui_active; /* whether a UI should be shown (or the ringer should ring) */
  guint           set_active_id;
};

static void calls_ui_call_data_cui_call_interface_init (CuiCallInterface *iface);
G_DEFINE_TYPE_WITH_CODE (CallsUiCallData, calls_ui_call_data, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CUI_TYPE_CALL,
                                                calls_ui_call_data_cui_call_interface_init))

static const char *
calls_ui_call_data_get_display_name (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), NULL);
  g_return_val_if_fail (!!self->call, NULL);

  if (calls_best_match_has_individual (self->best_match))
    return calls_best_match_get_name (self->best_match);
  else
    return calls_call_get_name (self->call);
}

static const char *
calls_ui_call_data_get_id (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), NULL);
  g_return_val_if_fail (!!self->call, NULL);

  return calls_call_get_id (self->call);
}

static CuiCallState
calls_ui_call_data_get_state (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), CUI_CALL_STATE_UNKNOWN);
  g_return_val_if_fail (!!self->call, CUI_CALL_STATE_UNKNOWN);

  return self->state;
}


static gboolean
calls_ui_call_data_get_encrypted (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), FALSE);
  g_return_val_if_fail (!!self->call, FALSE);

  return calls_call_get_encrypted (self->call);
}

static gboolean
calls_ui_call_data_get_can_dtmf (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), FALSE);
  g_return_val_if_fail (!!self->call, FALSE);

  return calls_call_can_dtmf (self->call);
}


static GdkPaintable *
calls_ui_call_data_get_avatar_icon (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_val_if_fail (CALLS_UI_CALL_DATA (self), NULL);

  return GDK_PAINTABLE (calls_best_match_get_avatar (self->best_match));
}


static gdouble
calls_ui_call_data_get_active_time (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), 0.0);

  return self->active_time;
}


static gboolean
calls_ui_call_data_get_inbound (CallsUiCallData *self)
{
  g_assert (CALLS_IS_UI_CALL_DATA (self));

  return calls_call_get_inbound (self->call);
}


static const char *
calls_ui_call_data_get_protocol (CallsUiCallData *self)
{
  g_assert (CALLS_IS_UI_CALL_DATA (self));

  return calls_call_get_protocol (self->call);
}


static void
calls_ui_call_data_accept (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_if_fail (CALLS_IS_UI_CALL_DATA (self));
  g_return_if_fail (!!self->call);

  calls_call_answer (self->call);
}


static void
calls_ui_call_data_hang_up (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_if_fail (CALLS_IS_UI_CALL_DATA (self));
  g_return_if_fail (!!self->call);

  calls_call_hang_up (self->call);
}


static void
calls_ui_call_data_send_dtmf (CuiCall    *call_data,
                              const char *dtmf)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_if_fail (CALLS_IS_UI_CALL_DATA (self));
  g_return_if_fail (!!self->call);

  calls_call_send_dtmf_tone (self->call, *dtmf);
}

static void
calls_ui_call_data_cui_call_interface_init (CuiCallInterface *iface)
{
  iface->get_id = calls_ui_call_data_get_id;
  iface->get_display_name = calls_ui_call_data_get_display_name;
  iface->get_state = calls_ui_call_data_get_state;
  iface->get_encrypted = calls_ui_call_data_get_encrypted;
  iface->get_can_dtmf = calls_ui_call_data_get_can_dtmf;
  iface->get_avatar_icon = calls_ui_call_data_get_avatar_icon;
  iface->get_active_time = calls_ui_call_data_get_active_time;

  iface->accept = calls_ui_call_data_accept;
  iface->hang_up = calls_ui_call_data_hang_up;
  iface->send_dtmf = calls_ui_call_data_send_dtmf;
}


static gboolean
on_timer_ticked (CallsUiCallData *self)
{
  g_assert (CALLS_IS_UI_CALL_DATA (self));

  self->active_time = g_timer_elapsed (self->timer, NULL);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE_TIME]);

  return G_SOURCE_CONTINUE;
}


static void
set_state (CallsUiCallData *self,
           CuiCallState     new_state)
{
  CuiCallState old_state;

  g_assert (CALLS_IS_UI_CALL_DATA (self));

  if (self->state == new_state)
    return;

  old_state = self->state;
  self->state = new_state;

  g_debug ("Setting UI call state from %s to %s",
           cui_call_state_to_string (old_state),
           cui_call_state_to_string (new_state));

  /* Check for started timer, because state could have changed like this:
   * ACTIVE -> HELD -> ACTIVE
   * and we don't want to start the timer multiple times
   */
  if (new_state == CUI_CALL_STATE_ACTIVE && !self->timer) {

    self->timer = g_timer_new ();
    self->timer_id = g_timeout_add (500,
                                    G_SOURCE_FUNC (on_timer_ticked),
                                    self);
    g_source_set_name_by_id (self->timer_id, "ui-call: active_time_timer");

    g_debug ("Start tracking active time; source id %u", self->timer_id);
  } else if (new_state == CUI_CALL_STATE_DISCONNECTED) {
    g_debug ("Stop tracking active time; source id %u", self->timer_id);

    g_clear_handle_id (&self->timer_id, g_source_remove);
    g_clear_pointer (&self->timer, g_timer_destroy);
    g_clear_handle_id (&self->set_active_id, g_source_remove);
  }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);

  g_signal_emit (self, signals[STATE_CHANGED], 0, new_state, old_state);
}


static void
on_notify_state (CallsUiCallData *self)
{
  CallsCallState state;

  g_assert (CALLS_IS_UI_CALL_DATA (self));

  state = calls_call_get_state (self->call);
  set_state (self, calls_call_state_to_cui_call_state (state));
}


static void
on_notify_name (CallsUiCallData *self)
{
  g_assert (CALLS_IS_UI_CALL_DATA (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DISPLAY_NAME]);
}


static void
on_notify_avatar (CallsUiCallData *self)
{
  g_assert (CALLS_IS_UI_CALL_DATA (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_AVATAR_ICON]);
}


static void
on_notify_encrypted (CallsUiCallData *self)
{
  g_assert (CALLS_IS_UI_CALL_DATA (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ENCRYPTED]);
}


static void
set_call_data (CallsUiCallData *self,
               CallsCall       *call)
{
  CallsManager *manager;
  CallsContactsProvider *contacts_provider;

  g_return_if_fail (CALLS_IS_UI_CALL_DATA (self));
  g_return_if_fail (CALLS_IS_CALL (call));

  self->call = call;
  g_signal_connect_object (self->call,
                           "notify::state",
                           G_CALLBACK (on_notify_state),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->call,
                           "notify::encrypted",
                           G_CALLBACK (on_notify_encrypted),
                           self,
                           G_CONNECT_SWAPPED);

  on_notify_state (self);

  manager = calls_manager_get_default ();
  contacts_provider = calls_manager_get_contacts_provider (manager);

  self->best_match =
    calls_contacts_provider_lookup_id (contacts_provider,
                                       calls_call_get_id (call));
  g_assert (self->best_match);

  g_signal_connect_object (self->best_match,
                           "notify::name",
                           G_CALLBACK (on_notify_name),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->best_match,
                           "notify::has-individual",
                           G_CALLBACK (on_notify_name),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->best_match,
                           "notify::avatar",
                           G_CALLBACK (on_notify_avatar),
                           self,
                           G_CONNECT_SWAPPED);

}


static void
set_ui_active (CallsUiCallData *self,
               gboolean         active)
{
  g_assert (CALLS_IS_UI_CALL_DATA (self));

  if (self->ui_active == active)
    return;

  self->ui_active = active;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_UI_ACTIVE]);
}


static gboolean
on_delay_set_active (CallsUiCallData *self)
{
  g_assert (CALLS_IS_UI_CALL_DATA (self));

  set_ui_active (self, TRUE);

  self->set_active_id = 0;
  g_object_unref (self);

  return G_SOURCE_REMOVE;
}


#define DELAY_UI_MS 100
static void
calls_ui_call_data_constructed (GObject *object)
{
  CallsUiCallData *self = CALLS_UI_CALL_DATA (object);

  G_OBJECT_CLASS (calls_ui_call_data_parent_class)->constructed (object);

  if (!calls_call_get_inbound (self->call) || self->state != CUI_CALL_STATE_INCOMING) {
    set_ui_active (self, TRUE);
    return;
  }

  set_ui_active (self, FALSE);
  self->set_active_id = g_timeout_add (DELAY_UI_MS,
                                       G_SOURCE_FUNC (on_delay_set_active),
                                       g_object_ref (self));
}
#undef DELAY_UI_MS


static void
calls_ui_call_data_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  CallsUiCallData *self = CALLS_UI_CALL_DATA (object);

  switch (property_id) {
  case PROP_CALL:
    // construct only
    set_call_data (self, g_value_dup_object (value));
    break;

  case PROP_ORIGIN_ID:
    self->origin_id = g_value_dup_string (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_ui_call_data_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  CallsUiCallData *self = CALLS_UI_CALL_DATA (object);
  CuiCall *cui_call = CUI_CALL (object);

  switch (property_id) {
  case PROP_CALL:
    g_value_set_object (value, self->call);
    break;

  case PROP_ORIGIN_ID:
    g_value_set_string (value, calls_ui_call_data_get_origin_id (self));
    break;

  case PROP_INBOUND:
    g_value_set_boolean (value, calls_ui_call_data_get_inbound (self));
    break;

  case PROP_PROTOCOL:
    g_value_set_string (value, calls_ui_call_data_get_protocol (self));
    break;

  case PROP_DISPLAY_NAME:
    g_value_set_string (value, calls_ui_call_data_get_display_name (cui_call));
    break;

  case PROP_ID:
    g_value_set_string (value, calls_ui_call_data_get_id (cui_call));
    break;

  case PROP_STATE:
    g_value_set_enum (value, calls_ui_call_data_get_state (cui_call));
    break;

  case PROP_ENCRYPTED:
    g_value_set_boolean (value, calls_ui_call_data_get_encrypted (cui_call));
    break;

  case PROP_CAN_DTMF:
    g_value_set_boolean (value, calls_ui_call_data_get_can_dtmf (cui_call));
    break;

  case PROP_AVATAR_ICON:
    g_value_set_object (value, calls_ui_call_data_get_avatar_icon (cui_call));
    break;

  case PROP_ACTIVE_TIME:
    g_value_set_double (value, self->active_time);

  case PROP_SILENCED:
    g_value_set_boolean (value, calls_ui_call_data_get_silenced (self));
    break;

  case PROP_UI_ACTIVE:
    g_value_set_boolean (value, calls_ui_call_data_get_ui_active (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
calls_ui_call_data_dispose (GObject *object)
{
  CallsUiCallData *self = CALLS_UI_CALL_DATA (object);

  g_clear_object (&self->call);
  g_clear_object (&self->best_match);

  g_clear_pointer (&self->origin_id, g_free);

  g_clear_handle_id (&self->timer_id, g_source_remove);
  g_clear_pointer (&self->timer, g_timer_destroy);

  G_OBJECT_CLASS (calls_ui_call_data_parent_class)->dispose (object);
}

static void
calls_ui_call_data_init (CallsUiCallData *self)
{
}

static void
calls_ui_call_data_class_init (CallsUiCallDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = calls_ui_call_data_constructed;
  object_class->set_property = calls_ui_call_data_set_property;
  object_class->get_property = calls_ui_call_data_get_property;
  object_class->dispose = calls_ui_call_data_dispose;

  props[PROP_CALL] =
    g_param_spec_object ("call",
                         "Call",
                         "The call",
                         CALLS_TYPE_CALL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_CALL, props[PROP_CALL]);

  props[PROP_ORIGIN_ID] =
    g_param_spec_string ("origin-id",
                         "Origin ID",
                         "ID of the origin used for the call",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_ORIGIN_ID, props[PROP_ORIGIN_ID]);

  props[PROP_INBOUND] =
    g_param_spec_boolean ("inbound",
                          "Inbound",
                          "Whether the call is inbound",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_INBOUND, props[PROP_INBOUND]);

  props[PROP_PROTOCOL] =
    g_param_spec_string ("protocol",
                         "Protocol",
                         "The protocol for the call, e.g. tel, sip",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_PROTOCOL, props[PROP_PROTOCOL]);

  props[PROP_SILENCED] =
    g_param_spec_boolean ("silenced",
                          "Silenced",
                          "Whether the call ringing should be silenced",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_SILENCED, props[PROP_SILENCED]);

  props[PROP_UI_ACTIVE] =
    g_param_spec_boolean ("ui-active",
                          "UI active",
                          "Whether the UI should be shown",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_property (object_class, PROP_UI_ACTIVE, props[PROP_UI_ACTIVE]);

  g_object_class_override_property (object_class, PROP_ID, "id");
  props[PROP_ID] = g_object_class_find_property (object_class, "id");

  g_object_class_override_property (object_class, PROP_DISPLAY_NAME, "display-name");
  props[PROP_DISPLAY_NAME] = g_object_class_find_property (object_class, "display-name");

  g_object_class_override_property (object_class, PROP_STATE, "state");
  props[PROP_STATE] = g_object_class_find_property (object_class, "state");

  g_object_class_override_property (object_class, PROP_ENCRYPTED, "encrypted");
  props[PROP_ENCRYPTED] = g_object_class_find_property (object_class, "encrypted");

  g_object_class_override_property (object_class, PROP_CAN_DTMF, "can-dtmf");
  props[PROP_CAN_DTMF] = g_object_class_find_property (object_class, "can-dtmf");

  g_object_class_override_property (object_class, PROP_AVATAR_ICON, "avatar-icon");
  props[PROP_AVATAR_ICON] = g_object_class_find_property (object_class, "avatar-icon");

  g_object_class_override_property (object_class, PROP_ACTIVE_TIME, "active-time");
  props[PROP_ACTIVE_TIME] = g_object_class_find_property (object_class, "active-time");

  /**
   * CallsUiCallData::state-changed:
   * @self: The #CallsUiCallData instance.
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
                  2, CUI_TYPE_CALL_STATE, CUI_TYPE_CALL_STATE);

}

CallsUiCallData *
calls_ui_call_data_new (CallsCall  *call,
                        const char *origin_id)
{
  return g_object_new (CALLS_TYPE_UI_CALL_DATA,
                       "call", call,
                       "origin-id", origin_id,
                       NULL);
}

/**
 * calls_ui_call_data_silence_ring:
 * @self: a #CallsUiCallData
 *
 * Inhibit ringing
 */
void
calls_ui_call_data_silence_ring (CallsUiCallData *self)
{
  g_return_if_fail (CALLS_IS_UI_CALL_DATA (self));
  g_return_if_fail (self->state == CUI_CALL_STATE_INCOMING);

  if (self->silenced)
    return;

  self->silenced = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SILENCED]);
}

/**
 * calls_ui_call_data_get_silenced:
 * @self: a #CallsUiCallData
 *
 * Returns: %TRUE if call has been silenced to not ring, %FALSE otherwise
 */
gboolean
calls_ui_call_data_get_silenced (CallsUiCallData *self)
{
  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), FALSE);

  return self->silenced;
}

/**
 * calls_ui_call_data_get_ui_active:
 * @self: a #CallsUiCallData
 *
 * Returns: %TRUE if the UI should be shown, %FALSE otherwise
 */
gboolean
calls_ui_call_data_get_ui_active (CallsUiCallData *self)
{
  g_return_val_if_fail (CALLS_UI_CALL_DATA (self), FALSE);

  return self->ui_active;
}

/**
 * calls_ui_call_data_get_call_type:
 * @self: a #CallsUiCallData
 *
 * Returns: The type of call, or #CALLS_CALL_TYPE_UNKNOWN if not known.
 */
CallsCallType
calls_ui_call_data_get_call_type (CallsUiCallData *self)
{
  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), CALLS_CALL_TYPE_UNKNOWN);
  g_return_val_if_fail (CALLS_CALL (self->call), CALLS_CALL_TYPE_UNKNOWN);

  return calls_call_get_call_type (self->call);
}

/**
 * calls_ui_call_data_get_origin_id:
 * @self: a #CallsUiCallData
 *
 * Returns: (transfer none): The id of the origin this call was placed from
 * or %NULL, if unknown.
 */
const char *
calls_ui_call_data_get_origin_id (CallsUiCallData *self)
{
  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), NULL);

  return self->origin_id;
}

/**
 * calls_ui_call_data_dup_origin_name:
 * @self: a #CallsUiCallData
 *
 * Returns: (transfer full): The name of the origin this call was placed from
 * or %NULL, if unknown.
 */
char *
calls_ui_call_data_dup_origin_name (CallsUiCallData *self)
{
  CallsOrigin *origin;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), NULL);

  origin = calls_manager_get_origin_by_id (calls_manager_get_default (),
                                           self->origin_id);

  if (origin)
    return calls_origin_get_name (origin);

  return NULL;
}

/**
 * calls_call_state_to_cui_call_state:
 * @state: A #CallsCallState
 *
 * Returns: a #CuiCallState @state is mapped to.
 */
CuiCallState
calls_call_state_to_cui_call_state (CallsCallState state)
{
  switch (state) {
  case CALLS_CALL_STATE_ACTIVE:
    return CUI_CALL_STATE_ACTIVE;
  case CALLS_CALL_STATE_HELD:
    return CUI_CALL_STATE_HELD;
  case CALLS_CALL_STATE_DIALING:
  case CALLS_CALL_STATE_ALERTING:
    return CUI_CALL_STATE_CALLING;
  case CALLS_CALL_STATE_INCOMING:
    return CUI_CALL_STATE_INCOMING;
  case CALLS_CALL_STATE_DISCONNECTED:
    return CUI_CALL_STATE_DISCONNECTED;
  default:
    return CUI_CALL_STATE_UNKNOWN;
  }
}
