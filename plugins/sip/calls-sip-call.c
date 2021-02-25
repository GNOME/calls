/*
 * Copyright (C) 2021 Purism SPC
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
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define G_LOG_DOMAIN "CallsSipCall"

#include "calls-sip-call.h"

#include "calls-message-source.h"
#include "calls-sip-media-manager.h"
#include "calls-sip-media-pipeline.h"
#include "calls-call.h"

#include <glib/gi18n.h>
#include <sofia-sip/nua.h>


struct _CallsSipCall
{
  GObject parent_instance;
  gchar *number;
  gboolean inbound;
  CallsCallState state;

  CallsSipMediaManager *manager;
  CallsSipMediaPipeline *pipeline;
  nua_handle_t *nh;
};

static void calls_sip_call_message_source_interface_init (CallsCallInterface *iface);
static void calls_sip_call_call_interface_init (CallsCallInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsSipCall, calls_sip_call, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_sip_call_message_source_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_CALL,
                                                calls_sip_call_call_interface_init))

enum {
  PROP_0,
  PROP_CALL_HANDLE,
  PROP_CALL_NUMBER,
  PROP_CALL_INBOUND,
  PROP_CALL_STATE,
  PROP_CALL_NAME,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];


static void
answer (CallsCall *call)
{
  CallsSipCall *self;
  g_autofree gchar *local_sdp = NULL;

  g_assert (CALLS_IS_CALL (call));
  g_assert (CALLS_IS_SIP_CALL (call));

  self = CALLS_SIP_CALL (call);

  g_assert (self->nh);

  if (self->state != CALLS_CALL_STATE_INCOMING) {
    g_warning ("Call must be in 'incoming' state in order to answer");
    return;
  }

  /* XXX dynamically get free ports */
  calls_sip_call_setup_local_media (self, 19042, 19043);

  local_sdp = calls_sip_media_manager_static_capabilities (self->manager,
                                                           19042,
                                                           FALSE);

  g_assert (local_sdp);
  g_debug ("Setting local SDP to string:\n%s", local_sdp);

  nua_respond (self->nh, 200, NULL,
               SOATAG_USER_SDP_STR (local_sdp),
               SOATAG_AF (SOA_AF_IP4_IP6),
               TAG_END ());

  calls_sip_call_set_state (self, CALLS_CALL_STATE_ACTIVE);
}

static void
hang_up (CallsCall *call)
{
  CallsSipCall *self;

  g_assert (CALLS_IS_CALL (call));
  g_assert (CALLS_IS_SIP_CALL (call));

  self = CALLS_SIP_CALL (call);

  switch (self->state) {
  case CALLS_CALL_STATE_DIALING:
    nua_cancel (self->nh, TAG_END ());
    g_debug ("Hanging up on outgoing ringing call");
    break;

  case CALLS_CALL_STATE_ACTIVE:
    nua_bye (self->nh, TAG_END ());

    g_debug ("Hanging up ongoing call");
    break;

  case CALLS_CALL_STATE_INCOMING:
    nua_respond (self->nh, 480, NULL, TAG_END ());
    g_debug ("Hanging up incoming call");
    break;

  case CALLS_CALL_STATE_DISCONNECTED:
    g_warning ("Tried hanging up already disconnected call");
    break;

  default:
    g_warning ("Hanging up not possible in state %d", self->state);
  }

  calls_sip_call_set_state (self, CALLS_CALL_STATE_DISCONNECTED);
}

static void
tone_start (CallsCall *call, gchar key)
{
  g_info ("Beep! (%c)", (int)key);
}

static void
tone_stop (CallsCall *call, gchar key)
{
  g_info ("Beep end (%c)", (int)key);
}


static void
calls_sip_call_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  CallsSipCall *self = CALLS_SIP_CALL (object);

  switch (property_id) {
  case PROP_CALL_HANDLE:
    self->nh = g_value_get_pointer (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_sip_call_get_property (GObject      *object,
                             guint         property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  CallsSipCall *self = CALLS_SIP_CALL (object);

  switch (property_id) {
  case PROP_CALL_INBOUND:
    g_value_set_boolean (value, self->inbound);
    break;

  case PROP_CALL_NUMBER:
    g_value_set_string (value, self->number);
    break;

  case PROP_CALL_STATE:
    g_value_set_enum (value, self->state);
    break;

  case PROP_CALL_NAME:
    g_value_set_string (value, NULL);
    break;

  case PROP_CALL_HANDLE:
    g_value_set_pointer (value, self->nh);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_sip_call_finalize (GObject *object)
{
  CallsSipCall *self = CALLS_SIP_CALL (object);

  g_free (self->number);

  if (self->pipeline) {
    calls_sip_media_pipeline_stop (self->pipeline);
    g_clear_object (&self->pipeline);
  }
  G_OBJECT_CLASS (calls_sip_call_parent_class)->finalize (object);
}


static void
calls_sip_call_class_init (CallsSipCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_sip_call_get_property;
  object_class->set_property = calls_sip_call_set_property;
  object_class->finalize = calls_sip_call_finalize;

  props[PROP_CALL_HANDLE] =
    g_param_spec_pointer ("nua-handle",
                          "NUA handle",
                          "The used NUA handler",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_CALL_HANDLE, props[PROP_CALL_HANDLE]);

#define IMPLEMENTS(ID, NAME) \
  g_object_class_override_property (object_class, ID, NAME);    \
  props[ID] = g_object_class_find_property(object_class, NAME);

  IMPLEMENTS(PROP_CALL_NUMBER, "number");
  IMPLEMENTS(PROP_CALL_INBOUND, "inbound");
  IMPLEMENTS(PROP_CALL_STATE, "state");
  IMPLEMENTS(PROP_CALL_NAME, "name");

#undef IMPLEMENTS

}

static void
calls_sip_call_call_interface_init (CallsCallInterface *iface)
{
  iface->answer = answer;
  iface->hang_up = hang_up;
  iface->tone_start = tone_start;
  iface->tone_stop = tone_stop;
}


static void
calls_sip_call_message_source_interface_init (CallsCallInterface *iface)
{
}


static void
calls_sip_call_init (CallsSipCall *self)
{
  MediaCodecInfo *best_codec;

  self->manager = calls_sip_media_manager_default ();

  best_codec = get_best_codec (self->manager);

  self->pipeline = calls_sip_media_pipeline_new (best_codec);
}


void
calls_sip_call_setup_local_media (CallsSipCall *self,
                                  guint         port_rtp,
                                  guint         port_rtcp)
{
  g_return_if_fail (CALLS_IS_SIP_CALL (self));
  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self->pipeline));

  g_debug ("Setting local ports: RTP/RTCP %u/%u", port_rtp, port_rtcp);
  g_object_set (G_OBJECT (self->pipeline),
                "lport-rtp", port_rtp,
                "lport-rtcp", port_rtcp,
                NULL);
}


void
calls_sip_call_setup_remote_media (CallsSipCall *self,
                                   const char   *remote,
                                   guint         port_rtp,
                                   guint         port_rtcp)
{
  g_return_if_fail (CALLS_IS_SIP_CALL (self));
  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self->pipeline));

  g_debug ("Setting remote ports: RTP/RTCP %u/%u", port_rtp, port_rtcp);
  g_object_set (G_OBJECT (self->pipeline),
                "remote", remote,
                "rport-rtp", port_rtp,
                "rport-rtcp", port_rtcp,
                NULL);
}


void
calls_sip_call_activate_media (CallsSipCall *self,
                               gboolean      enabled)
{
  g_return_if_fail (CALLS_IS_SIP_CALL (self));
  g_return_if_fail (CALLS_IS_SIP_MEDIA_PIPELINE (self->pipeline));

  if (enabled) {
    calls_sip_media_pipeline_start (self->pipeline);
  } else {
    calls_sip_media_pipeline_stop (self->pipeline);
  }
}


CallsSipCall *
calls_sip_call_new (const gchar  *number,
                    gboolean      inbound,
                    nua_handle_t *handle)
{
  CallsSipCall *call;

  g_return_val_if_fail (number != NULL, NULL);

  call = g_object_new (CALLS_TYPE_SIP_CALL,
                       "nua-handle", handle,
                       NULL);

  call->number = g_strdup (number);
  call->inbound = inbound;

  if (inbound)
    call->state = CALLS_CALL_STATE_INCOMING;
  else
    call->state = CALLS_CALL_STATE_DIALING;

  return call;
}


void
calls_sip_call_set_state (CallsSipCall   *self,
                          CallsCallState  state)
{
  CallsCallState old_state;

  g_return_if_fail (CALLS_IS_CALL (self));
  g_return_if_fail (CALLS_IS_SIP_CALL (self));

  old_state = self->state;

  if (old_state == state) {
    return;
  }

  self->state = state;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CALL_STATE]);
  g_signal_emit_by_name (CALLS_CALL (self),
                         "state-changed",
                         state,
                         old_state);
}

