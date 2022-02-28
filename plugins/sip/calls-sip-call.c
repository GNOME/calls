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
  PROP_IP,
  PROP_PIPELINE,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

struct _CallsSipCall
{
  GObject parent_instance;

  CallsSipMediaManager *manager;
  CallsSipMediaPipeline *pipeline;

  char *ip;

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

  if (!self->codecs)
    return FALSE;

  if (calls_sip_media_pipeline_get_state (self->pipeline) ==
      CALLS_MEDIA_PIPELINE_STATE_NEED_CODEC) {
    MediaCodecInfo *codec = (MediaCodecInfo *) self->codecs->data;

    g_debug ("Setting codec '%s' for pipeline", codec->name);
    calls_sip_media_pipeline_set_codec (self->pipeline, codec);
  }

  g_debug ("Setting remote ports: RTP/RTCP %u/%u",
           self->rport_rtp, self->rport_rtcp);

  g_object_set (self->pipeline,
                "remote", self->remote,
                "rport-rtp", self->rport_rtp,
                "rport-rtcp", self->rport_rtcp,
                NULL);

  return TRUE;
}


static void
calls_sip_call_answer (CallsCall *call)
{
  CallsSipCall *self;
  g_autofree gchar *local_sdp = NULL;
  guint rtp_port, rtcp_port;

  g_assert (CALLS_IS_CALL (call));
  g_assert (CALLS_IS_SIP_CALL (call));

  self = CALLS_SIP_CALL (call);

  g_assert (self->nh);

  if (calls_call_get_state (CALLS_CALL (self)) != CALLS_CALL_STATE_INCOMING) {
    g_warning ("Call must be in 'incoming' state in order to answer");
    return;
  }

  rtp_port = calls_sip_media_pipeline_get_rtp_port (self->pipeline);
  rtcp_port = calls_sip_media_pipeline_get_rtcp_port (self->pipeline);

  calls_sip_call_setup_local_media_connection (self);

  local_sdp = calls_sip_media_manager_get_capabilities (self->manager,
                                                        self->ip,
                                                        rtp_port,
                                                        rtcp_port,
                                                        FALSE,
                                                        self->codecs);

  g_assert (local_sdp);
  g_debug ("Setting local SDP to string:\n%s", local_sdp);

  nua_respond (self->nh, 200, NULL,
               SOATAG_USER_SDP_STR (local_sdp),
               SOATAG_AF (SOA_AF_IP4_IP6),
               TAG_END ());

  calls_call_set_state (CALLS_CALL (self), CALLS_CALL_STATE_ACTIVE);
}


static void
calls_sip_call_hang_up (CallsCall *call)
{
  CallsSipCall *self;

  g_assert (CALLS_IS_CALL (call));
  g_assert (CALLS_IS_SIP_CALL (call));

  self = CALLS_SIP_CALL (call);

  switch (calls_call_get_state (call)) {
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
    g_warning ("Hanging up not possible in state %d",
               calls_call_get_state (call));
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

  case PROP_IP:
    g_free (self->ip);
    self->ip = g_value_dup_string (value);
    break;

  case PROP_PIPELINE:
    self->pipeline = g_value_dup_object (value);
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

  calls_sip_media_pipeline_stop (self->pipeline);

  g_clear_object (&self->pipeline);
  g_clear_pointer (&self->codecs, g_list_free);
  g_clear_pointer (&self->remote, g_free);
  g_clear_pointer (&self->ip, g_free);

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

  call_class->answer = calls_sip_call_answer;
  call_class->hang_up = calls_sip_call_hang_up;

  props[PROP_CALL_HANDLE] =
    g_param_spec_pointer ("nua-handle",
                          "NUA handle",
                          "The used NUA handler",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  props[PROP_IP] =
    g_param_spec_string ("own-ip",
                         "Own IP",
                         "Own IP for media and SDP",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);

  props[PROP_PIPELINE] =
    g_param_spec_object ("pipeline",
                         "Pipeline",
                         "Media pipeline for this call",
                         CALLS_TYPE_SIP_MEDIA_PIPELINE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
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
 */
void
calls_sip_call_setup_local_media_connection (CallsSipCall *self)
{
  g_return_if_fail (CALLS_IS_SIP_CALL (self));

  /* XXX maybe we can get rid of this completely */
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
calls_sip_call_new (const gchar           *id,
                    gboolean               inbound,
                    const char            *own_ip,
                    CallsSipMediaPipeline *pipeline,
                    nua_handle_t          *handle)
{
  g_return_val_if_fail (id, NULL);

  return g_object_new (CALLS_TYPE_SIP_CALL,
                       "id", id,
                       "inbound", inbound,
                       "own-ip", own_ip,
                       "pipeline", pipeline,
                       "nua-handle", handle,
                       "call-type", CALLS_CALL_TYPE_SIP_VOICE,
                       NULL);
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
  self->codecs = g_list_copy (codecs);
}
