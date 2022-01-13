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

enum {
  PROP_0,
  PROP_CALL,
  PROP_DISPLAY_NAME,
  PROP_ID,
  PROP_STATE,
  PROP_ENCRYPTED,
  PROP_CAN_DTMF,
  PROP_AVATAR_ICON,
  PROP_LAST_PROP
};

static GParamSpec *props[PROP_LAST_PROP];


struct _CallsUiCallData
{
  GObject parent_instance;

  CallsCall *call;
  CallsBestMatch *best_match;
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

  return calls_best_match_get_name (self->best_match);
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

  return calls_call_state_to_cui_call_state (calls_call_get_state (self->call));
}


static gboolean
calls_ui_call_data_get_encrypted (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), FALSE);
  g_return_val_if_fail (!!self->call, FALSE);

  return FALSE;
}

static gboolean
calls_ui_call_data_get_can_dtmf (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), FALSE);
  g_return_val_if_fail (!!self->call, FALSE);

  return calls_call_can_dtmf (self->call);
}


static GLoadableIcon *
calls_ui_call_data_get_avatar_icon (CuiCall *call_data)
{
  CallsUiCallData *self = (CallsUiCallData *) call_data;

  g_return_val_if_fail (CALLS_UI_CALL_DATA (self), NULL);

  return calls_best_match_get_avatar (self->best_match);
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

  iface->accept = calls_ui_call_data_accept;
  iface->hang_up = calls_ui_call_data_hang_up;
  iface->send_dtmf = calls_ui_call_data_send_dtmf;
}


static void
on_notify_state (CallsUiCallData *self)
{
  g_assert (CALLS_IS_UI_CALL_DATA (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
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
set_call_data (CallsUiCallData *self,
               CallsCall       *call)
{
  CallsManager *manager;
  CallsContactsProvider *contacts_provider;

  g_return_if_fail (CALLS_IS_UI_CALL_DATA (self));
  g_return_if_fail (CALLS_IS_CALL (call));

  manager = calls_manager_get_default ();
  contacts_provider = calls_manager_get_contacts_provider (manager);

  self->best_match =
    calls_contacts_provider_lookup_id (contacts_provider,
                                       calls_call_get_id (call));

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
  self->call = call;

  g_signal_connect_object (self->call,
                           "notify::state",
                           G_CALLBACK (on_notify_state),
                           self,
                           G_CONNECT_SWAPPED);
}

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

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
calls_ui_call_data_finalize (GObject *object)
{
  CallsUiCallData *self = CALLS_UI_CALL_DATA (object);

  g_object_unref (self->call);
  g_object_unref (self->best_match);

  G_OBJECT_CLASS (calls_ui_call_data_parent_class)->finalize (object);
}

static void
calls_ui_call_data_init (CallsUiCallData *self)
{
}

static void
calls_ui_call_data_class_init (CallsUiCallDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = calls_ui_call_data_set_property;
  object_class->get_property = calls_ui_call_data_get_property;
  object_class->finalize = calls_ui_call_data_finalize;

  props[PROP_CALL] =
    g_param_spec_object ("call",
                         "Call",
                         "The call",
                         CALLS_TYPE_CALL,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_CALL, props[PROP_CALL]);

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
}

CallsUiCallData *
calls_ui_call_data_new (CallsCall *call)
{
  return g_object_new (CALLS_TYPE_UI_CALL_DATA, "call", call, NULL);
}


CallsCall *
calls_ui_call_data_get_call (CallsUiCallData *self)
{
  g_return_val_if_fail (CALLS_IS_UI_CALL_DATA (self), NULL);

  return self->call;
}


CuiCallState
calls_call_state_to_cui_call_state (CallsCallState state)
{
  switch (state) {
  case CALLS_CALL_STATE_ACTIVE:
    return CUI_CALL_STATE_ACTIVE;
  case CALLS_CALL_STATE_HELD:
    return CUI_CALL_STATE_HELD;
  case CALLS_CALL_STATE_DIALING:
    return CUI_CALL_STATE_DIALING;
  case CALLS_CALL_STATE_ALERTING:
    return CUI_CALL_STATE_DIALING;
  case CALLS_CALL_STATE_INCOMING:
    return CUI_CALL_STATE_INCOMING;
  case CALLS_CALL_STATE_WAITING:
    return CUI_CALL_STATE_INCOMING;
  case CALLS_CALL_STATE_DISCONNECTED:
    return CUI_CALL_STATE_DISCONNECTED;
  default:
    return CUI_CALL_STATE_UNKNOWN;
  }
}

