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


#include "calls-call.h"
#include "calls-message-source.h"
#include "calls-sip-call.h"
#include "calls-sip-media-manager.h"
#include "calls-sip-media-pipeline.h"
#include "calls-sip-util.h"
#include "util.h"

#include <glib/gi18n.h>

#include <sofia-sip/nua.h>

/**
 * SECTION:sip-call
 * @short_description: A #CallsCall for the SIP protocol
 * @Title: CallsSipCall
 *
 * #CallsSipCall derives from #CallsCall. Apart from allowing call control
 * like answering and hanging up it also coordinates with #CallsSipMediaManager
 * to prepare and control appropriate #CallsSipMediaPipeline objects.
 */

enum {
  PROP_0,
  PROP_CALL_HANDLE,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

struct _CallsSipCall
{
  GObject parent_instance;
  gchar *number;
  gboolean inbound;
  CallsCallState state;

  CallsSipMediaManager *manager;
  CallsSipMediaPipeline *pipeline;

  guint lport_rtp;
  guint lport_rtcp;
  guint rport_rtp;
  guint rport_rtcp;
  gchar *remote;

  nua_handle_t *nh;
  GList *codecs;
};

static void calls_sip_call_message_source_interface_init (CallsMessageSourceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsSipCall, calls_sip_call, CALLS_TYPE_CALL,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_sip_call_message_source_interface_init))

static gboolean
try_setting_up_media_pipeline (CallsSipCall *self)
{
  g_assert (CALLS_SIP_CALL (self));

  if (self->codecs == NULL)
    return FALSE;

  if (self->pipeline == NULL) {
    MediaCodecInfo *codec = (MediaCodecInfo *) self->codecs->data;
    self->pipeline = calls_sip_media_pipeline_new (codec);
  }

  if (!self->lport_rtp || !self->lport_rtcp || !self->remote ||
      !self->rport_rtp || !self->rport_rtcp)
    return FALSE;

  g_debug ("Setting local ports: RTP/RTCP %u/%u",
           self->lport_rtp, self->lport_rtcp);

  g_object_set (G_OBJECT (self->pipeline),
                "lport-rtp", self->lport_rtp,
                "lport-rtcp", self->lport_rtcp,
                NULL);

  g_debug ("Setting remote ports: RTP/RTCP %u/%u",
           self->rport_rtp, self->rport_rtcp);

  g_object_set (G_OBJECT (self->pipeline),
                "remote", self->remote,
                "rport-rtp", self->rport_rtp,
                "rport-rtcp", self->rport_rtcp,
                NULL);

  return TRUE;
}

static const char *
calls_sip_call_get_number (CallsCall *call)
{
  CallsSipCall *self = CALLS_SIP_CALL (call);

  return self->number;
}


static CallsCallState
calls_sip_call_get_state (CallsCall *call)
{
  CallsSipCall *self = CALLS_SIP_CALL (call);

  return self->state;
}


static gboolean
calls_sip_call_get_inbound (CallsCall *call)
{
  CallsSipCall *self = CALLS_SIP_CALL (call);

  return self->inbound;
}


static const char *
calls_sip_call_get_protocol (CallsCall *call)
{
  CallsSipCall *self = CALLS_SIP_CALL (call);

  return get_protocol_from_address (self->number);
}


static void
calls_sip_call_answer (CallsCall *call)
{
  CallsSipCall *self;
  g_autofree gchar *local_sdp = NULL;
  guint local_port = get_port_for_rtp ();

  g_assert (CALLS_IS_CALL (call));
  g_assert (CALLS_IS_SIP_CALL (call));

  self = CALLS_SIP_CALL (call);

  g_assert (self->nh);

  if (self->state != CALLS_CALL_STATE_INCOMING) {
    g_warning ("Call must be in 'incoming' state in order to answer");
    return;
  }

  /* TODO get free port by creating GSocket and passing that to the pipeline */
  calls_sip_call_setup_local_media_connection (self, local_port, local_port + 1);

  local_sdp = calls_sip_media_manager_get_capabilities (self->manager,
                                                        local_port,
                                                        FALSE,
                                                        self->codecs);

  g_assert (local_sdp);
  g_debug ("Setting local SDP to string:\n%s", local_sdp);

  nua_respond (self->nh, 200, NULL,
               SOATAG_USER_SDP_STR (local_sdp),
               SOATAG_AF (SOA_AF_IP4_IP6),
               TAG_END ());

  calls_sip_call_set_state (self, CALLS_CALL_STATE_ACTIVE);
}


static void
calls_sip_call_hang_up (CallsCall *call)
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
  g_clear_pointer (&self->codecs, g_list_free);
  g_clear_pointer (&self->remote, g_free);

  G_OBJECT_CLASS (calls_sip_call_parent_class)->finalize (object);
}


static void
calls_sip_call_class_init (CallsSipCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsCallClass *call_class = CALLS_CALL_CLASS (klass);

  object_class->get_property = calls_sip_call_get_property;
  object_class->set_property = calls_sip_call_set_property;
  object_class->finalize = calls_sip_call_finalize;

  call_class->get_number = calls_sip_call_get_number;
  call_class->get_state = calls_sip_call_get_state;
  call_class->get_inbound = calls_sip_call_get_inbound;
  call_class->get_protocol = calls_sip_call_get_protocol;
  call_class->answer = calls_sip_call_answer;
  call_class->hang_up = calls_sip_call_hang_up;

  props[PROP_CALL_HANDLE] =
    g_param_spec_pointer ("nua-handle",
                          "NUA handle",
                          "The used NUA handler",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_CALL_HANDLE, props[PROP_CALL_HANDLE]);
}


static void
calls_sip_call_message_source_interface_init (CallsMessageSourceInterface *iface)
{
}


static void
calls_sip_call_init (CallsSipCall *self)
{
  self->manager = calls_sip_media_manager_default ();
}

/**
 * calls_sip_call_setup_local_media_connection:
 * @self: A #CallsSipCall
 * @port_rtp: The RTP port on the the local host
 * @port_rtcp: The RTCP port on the local host
 */
void
calls_sip_call_setup_local_media_connection (CallsSipCall *self,
                                             guint         port_rtp,
                                             guint         port_rtcp)
{
  g_return_if_fail (CALLS_IS_SIP_CALL (self));

  self->lport_rtp = port_rtp;
  self->lport_rtcp = port_rtcp;

  try_setting_up_media_pipeline (self);
}

/**
 * calls_sip_call_setup_remote_media_connection:
 * @self: A #CallsSipCall
 * @remote: The remote host
 * @port_rtp: The RTP port on the remote host
 * @port_rtcp: The RTCP port on the remote host
 */
void
calls_sip_call_setup_remote_media_connection (CallsSipCall *self,
                                              const char   *remote,
                                              guint         port_rtp,
                                              guint         port_rtcp)
{
  g_return_if_fail (CALLS_IS_SIP_CALL (self));

  g_free (self->remote);
  self->remote = g_strdup (remote);
  self->rport_rtp = port_rtp;
  self->rport_rtcp = port_rtcp;

  try_setting_up_media_pipeline (self);
}

/**
 * calls_sip_call_activate_media:
 * @self: A #CallsSipCall
 * @enabled: %TRUE to enable the media pipeline, %FALSE to disable
 *
 * Controls the state of the #CallsSipMediaPipeline
 */
void
calls_sip_call_activate_media (CallsSipCall *self,
                               gboolean      enabled)
{
  g_return_if_fail (CALLS_IS_SIP_CALL (self));

  /* when hanging up an incoming call the pipeline has not yet been setup */
  if (self->pipeline == NULL && !enabled)
    return;
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

/**
 * calls_sip_call_set_state:
 * @self: A #CallsSipCall
 * @state: The new #CallsCallState to set
 *
 * Sets the new call state and emits the state-changed signal
 */
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
  g_object_notify (G_OBJECT (self), "state");
  g_signal_emit_by_name (CALLS_CALL (self),
                         "state-changed",
                         state,
                         old_state);
}

/**
 * calls_sip_call_set_codecs:
 * @self: A #CallsSipCall
 * @codecs: A #GList of #MediaCodecInfo elements
 *
 * Set the supported codecs. This is used when answering the call
 */
void
calls_sip_call_set_codecs (CallsSipCall *self,
                           GList        *codecs)
{
  g_return_if_fail (CALLS_IS_SIP_CALL (self));
  g_return_if_fail (codecs);

  g_list_free (self->codecs);
  self->codecs = codecs;
}
