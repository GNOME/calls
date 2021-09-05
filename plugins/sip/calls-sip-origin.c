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

#define G_LOG_DOMAIN "CallsSipOrigin"


#include "calls-account.h"
#include "calls-message-source.h"
#include "calls-origin.h"
#include "calls-sip-call.h"
#include "calls-sip-enums.h"
#include "calls-sip-origin.h"
#include "calls-sip-util.h"
#include "calls-sip-media-manager.h"
#include "calls-network-watch.h"
#include "config.h"
#include "enum-types.h"

#include <glib/gi18n.h>
#include <glib-object.h>

#include <sofia-sip/sofia_features.h>
#include <sofia-sip/nua.h>
#include <sofia-sip/su_tag.h>
#include <sofia-sip/su_tag_io.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sdp.h>

/**
 * SECTION:sip-origin
 * @short_description: A #CallsOrigin for the SIP protocol
 * @Title: CallsSipOrigin
 *
 * #CallsSipOrigin implements the #CallsOriginInterface and is mainly
 * responsible for managing the sofia-sip callbacks, keeping track of #CallsSipCall
 * objects and coordinating with #CallsSipMediaManager.
 */

enum {
  PROP_0,
  PROP_NAME,
  PROP_ACC_HOST,
  PROP_ACC_USER,
  PROP_ACC_PASSWORD,
  PROP_ACC_DISPLAY_NAME,
  PROP_ACC_PORT,
  PROP_ACC_PROTOCOL,
  PROP_ACC_AUTO_CONNECT,
  PROP_ACC_DIRECT,
  PROP_ACC_LOCAL_PORT,
  PROP_SIP_CONTEXT,
  PROP_ACC_STATE,
  PROP_ACC_ADDRESS,
  PROP_CALLS,
  PROP_COUNTRY_CODE,
  PROP_NUMERIC,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

static gboolean set_contact_header = FALSE;

struct _CallsSipOrigin
{
  GObject parent_instance;

  CallsSipContext *ctx;
  nua_t *nua;
  CallsSipHandles *oper;
  char *contact_header; /* Needed for sofia SIP >= 1.13 */

  /* Direct connection mode is useful for debugging purposes */
  gboolean use_direct_connection;

  /* Needed to handle shutdown correctly. See sip_callback and dispose method */
  gboolean is_nua_shutdown;
  gboolean is_shutdown_success;

  CallsAccountState state;

  CallsSipMediaManager *media_manager;

  /* Account information */
  char *host;
  char *user;
  char *password;
  char *display_name;
  gint port;
  char *transport_protocol;
  gboolean auto_connect;
  gboolean direct_mode;
  gint local_port;

  const char *protocol_prefix;
  char *address;
  char *name;

  GList *calls;
  GHashTable *call_handles;
};

static void calls_sip_origin_message_source_interface_init (CallsOriginInterface *iface);
static void calls_sip_origin_origin_interface_init (CallsOriginInterface *iface);
static void calls_sip_origin_accounts_interface_init (CallsAccountInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsSipOrigin, calls_sip_origin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_MESSAGE_SOURCE,
                                                calls_sip_origin_message_source_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_ORIGIN,
                                                calls_sip_origin_origin_interface_init)
                         G_IMPLEMENT_INTERFACE (CALLS_TYPE_ACCOUNT,
                                                calls_sip_origin_accounts_interface_init))

static void
remove_call (CallsSipOrigin *self,
             CallsCall      *call,
             const gchar    *reason)
{
  CallsOrigin *origin;
  CallsSipCall *sip_call;
  gboolean inbound;
  nua_handle_t *nh;

  origin = CALLS_ORIGIN (self);
  sip_call = CALLS_SIP_CALL (call);

  self->calls = g_list_remove (self->calls, call);

  g_object_get (sip_call,
                "inbound", &inbound,
                "nua-handle", &nh,
                NULL);

  /* TODO support multiple simultaneous calls */
  if (self->oper->call_handle == nh)
    self->oper->call_handle = NULL;

  g_signal_emit_by_name (origin, "call-removed", call, reason);
  g_object_unref (call);
}


static void
remove_calls (CallsSipOrigin *self,
              const gchar    *reason)
{
  CallsCall *call;
  GList *next;

  while (self->calls != NULL) {
    call = self->calls->data;
    next = self->calls->next;

    calls_call_hang_up (call);

    g_list_free_1 (self->calls);
    self->calls = next;

    g_signal_emit_by_name (self, "call-removed", call, reason);
    g_object_unref (call);
  }

  g_hash_table_remove_all (self->call_handles);

  g_clear_pointer (&self->oper->call_handle, nua_handle_unref);
}


static void
on_call_state_changed (CallsSipOrigin *self,
                       CallsCallState  new_state,
                       CallsCallState  old_state,
                       CallsCall      *call)
{
  g_assert (CALLS_IS_SIP_ORIGIN (self));
  g_assert (CALLS_IS_CALL (call));

  if (new_state != CALLS_CALL_STATE_DISCONNECTED)
    {
      return;
    }

  remove_call (self, call, "Disconnected");
}


static void
add_call (CallsSipOrigin *self,
          const gchar    *address,
          gboolean        inbound,
          nua_handle_t   *handle)
{
  CallsSipCall *sip_call;
  CallsCall *call;
  g_autofree gchar *local_sdp = NULL;

  /* TODO get free port by creating GSocket and passing that to the pipeline */
  guint local_port = get_port_for_rtp ();

  sip_call = calls_sip_call_new (address, inbound, handle);
  g_assert (sip_call != NULL);

  if (self->oper->call_handle)
    nua_handle_unref (self->oper->call_handle);

  self->oper->call_handle = handle;

  self->calls = g_list_append (self->calls, sip_call);
  g_hash_table_insert (self->call_handles, handle, sip_call);

  call = CALLS_CALL (sip_call);

  g_signal_emit_by_name (CALLS_ORIGIN (self), "call-added", call);
  g_signal_connect_swapped (call, "state-changed",
                            G_CALLBACK (on_call_state_changed),
                            self);

  if (!inbound) {
    calls_sip_call_setup_local_media_connection (sip_call, local_port, local_port + 1);

    local_sdp = calls_sip_media_manager_static_capabilities (self->media_manager,
                                                             local_port,
                                                             check_sips (address));

    g_assert (local_sdp);

    g_debug ("Setting local SDP for outgoing call to %s:\n%s", address, local_sdp);

    /* TODO transform tel URI according to https://tools.ietf.org/html/rfc3261#section-19.1.6 */

    /* TODO handle IPv4 vs IPv6 for nua_invite (SOATAG_TAG) */
    nua_invite (self->oper->call_handle,
                SOATAG_AF (SOA_AF_IP4_IP6),
                SOATAG_USER_SDP_STR (local_sdp),
                SIPTAG_TO_STR (address),
                TAG_IF (!!self->contact_header, SIPTAG_CONTACT_STR (self->contact_header)),
                SOATAG_RTP_SORT (SOA_RTP_SORT_REMOTE),
                SOATAG_RTP_SELECT (SOA_RTP_SELECT_ALL),
                TAG_END ());
  }
}


static void
dial (CallsOrigin *origin,
      const gchar *address)
{
  CallsSipOrigin *self;
  nua_handle_t *nh;
  g_autofree char *name = NULL;

  g_assert (CALLS_ORIGIN (origin));
  g_assert (CALLS_IS_SIP_ORIGIN (origin));

  name = calls_origin_get_name (origin);

  if (address == NULL) {
    g_warning ("Tried dialing on origin '%s' without an address",
               name);
    return;
  }

  self = CALLS_SIP_ORIGIN (origin);

  nh = nua_handle (self->nua, self->oper,
                   NUTAG_MEDIA_ENABLE (1),
                   SOATAG_ACTIVE_AUDIO (SOA_ACTIVE_SENDRECV),
                   TAG_END ());

  g_debug ("Calling `%s' from origin '%s'", address, name);

  /* We don't require the user to input the prefix */
  if (!g_str_has_prefix (address, "sip:") &&
      !g_str_has_prefix (address, "sips:")) {
    g_autofree char * address_prefix =
      g_strconcat (self->protocol_prefix, ":", address, NULL);

    add_call (CALLS_SIP_ORIGIN (origin), address_prefix, FALSE, nh);
  } else {
    add_call (CALLS_SIP_ORIGIN (origin), address, FALSE, nh);
  }
}

static void
create_inbound (CallsSipOrigin *self,
                const gchar    *address,
                nua_handle_t   *handle)
{
  g_assert (CALLS_IS_SIP_ORIGIN (self));
  g_assert (address != NULL);

  /* TODO support multiple calls */
  if (self->oper->call_handle)
    nua_handle_unref (self->oper->call_handle);

  self->oper->call_handle = handle;

  add_call (self, address, TRUE, handle);
}

static void
update_nua (CallsSipOrigin *self)
{
  g_autofree char *from_str = NULL;

  g_assert (CALLS_IS_SIP_ORIGIN (self));
  if (!self->nua) {
    g_warning ("Cannot update nua stack, aborting");
    return;
  }

  self->protocol_prefix = get_protocol_prefix (self->transport_protocol);

  g_free (self->address);
  self->address = g_strconcat (self->user, "@", self->host, NULL);
  from_str = g_strconcat (self->protocol_prefix, ":", self->address, NULL);

  nua_set_params (self->nua,
                  SIPTAG_FROM_STR (from_str),
                  TAG_IF (self->display_name, NUTAG_M_DISPLAY (self->display_name)),
                  TAG_NULL ());
}


static void
sip_authenticate (CallsSipOrigin *self,
                  nua_handle_t   *nh,
                  sip_t const    *sip)
{
  const gchar *scheme = NULL;
  const gchar *realm = NULL;
  g_autofree gchar *auth = NULL;
  sip_www_authenticate_t *www_auth = sip->sip_www_authenticate;
  sip_proxy_authenticate_t *proxy_auth = sip->sip_proxy_authenticate;

  if (www_auth) {
    scheme = www_auth->au_scheme;
    realm = msg_params_find (www_auth->au_params, "realm=");
  }
  else if (proxy_auth) {
    scheme = proxy_auth->au_scheme;
    realm = msg_params_find (proxy_auth->au_params, "realm=");
  }
  else {
    g_warning ("No authentication context found");
    return;
  }
  g_debug ("need to authenticate to realm %s", realm);

  /* TODO handle authentication to different realms
   * https://source.puri.sm/Librem5/calls/-/issues/266
   */
  auth = g_strdup_printf ("%s:%s:%s:%s",
                          scheme, realm, self->user, self->password);
  nua_authenticate (nh, NUTAG_AUTH (auth), TAG_END ());
}


static void
sip_r_invite (int              status,
              char const      *phrase,
              nua_t           *nua,
              CallsSipOrigin  *origin,
              nua_handle_t    *nh,
              CallsSipHandles *op,
              sip_t const     *sip,
              tagi_t           tags[])
{
    g_debug ("response to outgoing INVITE: %03d %s", status, phrase);

    /* TODO call states (see i_state) */
    if (status == 401 || status == 407) {
      sip_authenticate (origin, nh, sip);
    }
    else if (status == 403) {
      g_warning ("Response to outgoing INVITE: 403 wrong credentials?");
    }
    else if (status == 904) {
      g_warning ("Response to outgoing INVITE: 904 unmatched challenge."
                 "Possibly the challenge was already answered?");
    }
    else if (status == 180) {
    }
    else if (status == 100) {
    }
    else if (status == 200) {
    }
}


static void
sip_r_register (int              status,
                char const      *phrase,
                nua_t           *nua,
                CallsSipOrigin  *origin,
                nua_handle_t    *nh,
                CallsSipHandles *op,
                sip_t const     *sip,
                tagi_t           tags[])
{
  g_debug ("response to REGISTER: %03d %s", status, phrase);

  if (status == 200) {
    g_debug ("REGISTER successful");
    origin->state = CALLS_ACCOUNT_ONLINE;
    nua_get_params (nua, TAG_ANY (), TAG_END());

  } else if (status == 401 || status == 407) {
    sip_authenticate (origin, nh, sip);
    origin->state = CALLS_ACCOUNT_AUTHENTICATING;

  } else if (status == 403) {
    g_warning ("wrong credentials?");
    origin->state = CALLS_ACCOUNT_AUTHENTICATION_FAILURE;

  } else if (status == 904) {
    g_warning ("unmatched challenge");
    origin->state = CALLS_ACCOUNT_AUTHENTICATION_FAILURE;
  }

  g_object_notify_by_pspec (G_OBJECT (origin), props[PROP_ACC_STATE]);
}


static void
sip_r_unregister (int              status,
                  char const      *phrase,
                  nua_t           *nua,
                  CallsSipOrigin  *origin,
                  nua_handle_t    *nh,
                  CallsSipHandles *op,
                  sip_t const     *sip,
                  tagi_t           tags[])
{
  g_debug ("response to unregistering: %03d %s", status, phrase);

  if (status == 200) {
    g_debug ("Unregistering successful");
    origin->state = CALLS_ACCOUNT_OFFLINE;

  } else {
    g_warning ("Unregisterung unsuccessful: %03d %s", status, phrase);
    origin->state = CALLS_ACCOUNT_UNKNOWN_ERROR;
  }

  g_object_notify_by_pspec (G_OBJECT (origin), props[PROP_ACC_STATE]);
}


static void
sip_i_state (int              status,
             char const      *phrase,
             nua_t           *nua,
             CallsSipOrigin  *origin,
             nua_handle_t    *nh,
             CallsSipHandles *op,
             sip_t const     *sip,
             tagi_t           tags[])
{
  const sdp_session_t *r_sdp = NULL;
  const sdp_session_t *l_sdp = NULL;
  gint call_state = nua_callstate_init;
  CallsCallState state;
  CallsSipCall *call;
  int offer_sent = 0;
  int offer_recv = 0;
  int answer_sent = 0;
  int answer_recv = 0;

  g_assert (CALLS_IS_SIP_ORIGIN (origin));

  g_debug ("The call state has changed: %03d %s", status, phrase);

  call = g_hash_table_lookup (origin->call_handles, nh);

  if (call == NULL) {
    g_warning ("No call found for the current handle");
    return;
  }

  tl_gets (tags,
           SOATAG_LOCAL_SDP_REF (l_sdp),
           SOATAG_REMOTE_SDP_REF (r_sdp),
           NUTAG_CALLSTATE_REF (call_state),
           NUTAG_OFFER_SENT_REF (offer_sent),
           NUTAG_OFFER_RECV_REF (offer_recv),
           NUTAG_ANSWER_SENT_REF (answer_sent),
           NUTAG_ANSWER_RECV_REF (answer_recv),
           TAG_END ());

  if (status == 503) {
    CALLS_EMIT_MESSAGE (origin, "DNS error", GTK_MESSAGE_ERROR);
  }
  /* XXX making some assumptions about the received SDP message here...
   * namely: that there is only the session wide connection c= line
   * and no individual connections per media stream.
   * also: rtcp port = rtp port + 1
   */
  if (r_sdp) {
    GList *codecs =
      calls_sip_media_manager_get_codecs_from_sdp (origin->media_manager,
                                                   r_sdp->sdp_media);

    calls_sip_call_set_codecs (call, codecs);
    calls_sip_call_setup_remote_media_connection (call,
                                                  r_sdp->sdp_connection->c_address,
                                                  r_sdp->sdp_media->m_port,
                                                  r_sdp->sdp_media->m_port + 1);
  }

  switch (call_state) {
  case nua_callstate_init:
    return;

  case nua_callstate_calling:
    state = CALLS_CALL_STATE_DIALING;
    break;

  case nua_callstate_received:
    state = CALLS_CALL_STATE_INCOMING;
    g_debug ("Call incoming");
    break;

  case nua_callstate_ready:
    g_debug ("Call ready. Activating media pipeline");

    calls_sip_call_activate_media (call, TRUE);
    state = CALLS_CALL_STATE_ACTIVE;
    break;

  case nua_callstate_terminated:
    g_debug ("Call terminated. Deactivating media pipeline");

    calls_sip_call_activate_media (call, FALSE);
    state = CALLS_CALL_STATE_DISCONNECTED;

    g_hash_table_remove (origin->call_handles, nh);
    break;

  default:
    return;
  }

  calls_sip_call_set_state (call, state);
}


static void
sip_r_get_params (int              status,
                  char const      *phrase,
                  nua_t           *nua,
                  CallsSipOrigin  *origin,
                  nua_handle_t    *nh,
                  CallsSipHandles *op,
                  sip_t const     *sip,
                  tagi_t           tags[])
{
  tagi_t *from = NULL;
  const char *from_str = NULL;

  g_debug ("response to get_params: %03d %s", status, phrase);

  if (!set_contact_header)
    return;

  if ((status != 200) || ((from = tl_find (tags, siptag_from_str)) == NULL)) {
    g_warning ("Could not find 'siptag_from_tag' among the tags");
    return;
  }

  from_str = (const char *) from->t_value;
  g_free (origin->contact_header);
  origin->contact_header = g_strdup (from_str);
}


static void
sip_callback (nua_event_t   event,
              int           status,
              char const   *phrase,
              nua_t        *nua,
              nua_magic_t  *magic,
              nua_handle_t *nh,
              nua_hmagic_t *hmagic,
              sip_t const  *sip,
              tagi_t        tags[])
{
  CallsSipOrigin *origin = CALLS_SIP_ORIGIN (magic);
  CallsSipHandles *op = origin->oper;
  g_autofree gchar * from = NULL;

  switch (event) {
  case nua_i_invite:
    if (sip->sip_from && sip->sip_from->a_url &&
        sip->sip_from->a_url->url_scheme &&
        sip->sip_from->a_url->url_user &&
        sip->sip_from->a_url->url_host)
      from = g_strconcat (sip->sip_from->a_url->url_scheme, ":",
                          sip->sip_from->a_url->url_user, "@",
                          sip->sip_from->a_url->url_host, NULL);
    else {
      nua_respond (nh, 400, NULL, TAG_END ());
      g_warning ("invalid incoming INVITE request");
      break;
    }

    g_debug ("incoming call INVITE: %03d %s from %s", status, phrase, from);

    /* reject if there already is a ongoing call */
    if (op->call_handle) {
      nua_respond (nh, 486, NULL, TAG_END ());
      g_debug ("Cannot handle more than one call. Rejecting");
    }
    else
      create_inbound (origin, from, nh);

    break;

  case nua_r_invite:
    sip_r_invite (status,
                  phrase,
                  nua,
                  origin,
                  nh,
                  op,
                  sip,
                  tags);
    break;

  case nua_i_ack:
    g_debug ("incoming ACK: %03d %s", status, phrase);
    break;

  case nua_i_bye:
    g_debug ("incoming BYE: %03d %s", status, phrase);
    break;

  case nua_r_bye:
    g_debug ("response to BYE: %03d %s", status, phrase);
    if (status == 200) {
      CallsSipCall *call =
        CALLS_SIP_CALL (g_hash_table_lookup (origin->call_handles, nh));

      if (call == NULL) {
        g_warning ("BYE response from an unknown call");
        return;
      }
    }
    break;

  case nua_r_register:
    sip_r_register (status,
                    phrase,
                    nua,
                    origin,
                    nh,
                    op,
                    sip,
                    tags);
    break;

  case nua_r_unregister:
    sip_r_unregister (status,
                      phrase,
                      nua,
                      origin,
                      nh,
                      op,
                      sip,
                      tags);
    break;

  case nua_r_get_params:
    sip_r_get_params (status,
                      phrase,
                      nua,
                      origin,
                      nh,
                      op,
                      sip,
                      tags);
    break;

  case nua_r_set_params:
    g_debug ("response to set_params: %03d %s", status, phrase);
    break;

  case nua_i_outbound:
    g_debug ("status of outbound engine has changed: %03d %s", status, phrase);

    if (status == 404) {
      CALLS_EMIT_MESSAGE (origin, "contact not found", GTK_MESSAGE_ERROR);

      g_warning ("outbound engine changed state to %03d %s", status, phrase);
    }

    break;

  case nua_i_state:
    sip_i_state (status,
                 phrase,
                 nua,
                 origin,
                 nh,
                 op,
                 sip,
                 tags);
    break;

  case nua_r_cancel:
    g_debug ("response to CANCEL: %03d %s", status, phrase);
    break;

  case nua_r_terminate:
    break;

  case nua_r_shutdown:
    /* see also deinit_sip () */
    g_debug ("response to nua_shutdown: %03d %s", status, phrase);
    if (status == 200) {
      origin->is_nua_shutdown = TRUE;
      origin->is_shutdown_success = TRUE;
    }
    else if (status == 500) {
      origin->is_nua_shutdown = TRUE;
      origin->is_shutdown_success = FALSE;
    }
    break;

    /* Deprecated events */
  case nua_i_active:
    break;
  case nua_i_terminated:
    break;

  default:
    /* unknown event -> print out error message */
    g_warning ("unknown event %d: %03d %s",
               event,
               status,
               phrase);
    g_warning ("printing tags");
    tl_print(stdout, "", tags);
    break;
  }
}


static nua_t *
setup_nua (CallsSipOrigin *self)
{
  const char *sip_test_env = g_getenv ("CALLS_SIP_TEST");
  nua_t *nua;
  gboolean use_sips = FALSE;
  gboolean use_ipv6 = FALSE; /* TODO make configurable or use DNS to figure out if ipv6 is supported*/
  const char *ipv6_bind = "*";
  const char *ipv4_bind = "0.0.0.0";
  const char *uuid = NULL;
  g_autofree char *urn_uuid = NULL;
  g_autofree char *sip_url = NULL;
  g_autofree char *sips_url = NULL;
  g_autofree char *from_str = NULL;

  if (!sip_test_env || sip_test_env[0] == '\0') {
    CallsNetworkWatch *nw = calls_network_watch_get_default ();
    ipv4_bind = calls_network_watch_get_ipv4 (nw);
    ipv6_bind = calls_network_watch_get_ipv6 (nw);
  }

  uuid = nua_generate_instance_identifier (self->ctx->home);
  urn_uuid = g_strdup_printf ("urn:uuid:%s", uuid);

  self->protocol_prefix = get_protocol_prefix (self->transport_protocol);

  self->address = g_strconcat (self->user, "@", self->host, NULL);
  from_str = g_strconcat (self->protocol_prefix, ":", self->address, NULL);

  use_sips = check_sips (from_str);
  use_ipv6 = check_ipv6 (self->host);

  if (self->local_port > 0) {
    sip_url = g_strdup_printf ("sip:%s:%d",
                               use_ipv6 ? ipv6_bind : ipv4_bind,
                               self->local_port);
    sips_url = g_strdup_printf ("sips:%s:%d",
                                use_ipv6 ? ipv6_bind : ipv4_bind,
                                self->local_port);
  } else {
    sip_url = g_strdup_printf ("sip:%s:*",
                               use_ipv6 ? ipv6_bind : ipv4_bind);
    sips_url = g_strdup_printf ("sips:%s:*",
                                use_ipv6 ? ipv6_bind : ipv4_bind);
  }

  /** For TLS nua_create() will error if NUTAG_URL includes ";transport=tls"
   *  In that case NUTAG_SIPS_URL should be used and NUTAG_URL should be as usual
   *  Since UDP is the default we only need to append the suffix in the TCP case
   */
  if (g_ascii_strcasecmp (self->transport_protocol, "TCP") == 0) {
    char *temp = sip_url;

    sip_url = g_strdup_printf ("%s;transport=%s", temp, self->transport_protocol);
    g_free (temp);
  }



  nua = nua_create (self->ctx->root,
                    sip_callback,
                    self,
                    NUTAG_USER_AGENT (APP_DATA_NAME),
                    NUTAG_URL (sip_url),
                    TAG_IF (use_sips, NUTAG_SIPS_URL (sips_url)),
                    SIPTAG_FROM_STR (from_str),
                    NUTAG_ALLOW ("INVITE, ACK, BYE, CANCEL, OPTIONS, UPDATE"),
                    NUTAG_SUPPORTED ("replaces, gruu, outbound"),
                    NTATAG_MAX_FORWARDS (70),
                    NUTAG_ENABLEINVITE (1),
                    NUTAG_AUTOANSWER (0),
                    NUTAG_AUTOACK (1),
                    NUTAG_PATH_ENABLE (0),
                    NUTAG_MEDIA_ENABLE (1),
                    NUTAG_INSTANCE (urn_uuid),
                    TAG_NULL ());

  return nua;
}

static char *
get_registrar_url (CallsSipOrigin *self)
{
  g_assert (CALLS_IS_SIP_ORIGIN (self));

  if (self->port > 0 && self->port <= 65535)
    return g_strdup_printf ("%s:%s:%d", self->protocol_prefix, self->host, self->port);
  else
    return g_strconcat (self->protocol_prefix, ":", self->host, NULL);
}

static CallsSipHandles *
setup_sip_handles (CallsSipOrigin *self)
{
  CallsSipHandles *oper;
  g_autofree char *registrar_url = NULL;

  g_assert (CALLS_IS_SIP_ORIGIN (self));

  if (!(oper = su_zalloc (self->ctx->home, sizeof(CallsSipHandles)))) {
    g_warning ("cannot create handle");
    return NULL;
  }

  oper->context = self->ctx;
  oper->register_handle = nua_handle (self->nua, self->oper,
                                      SIPTAG_EXPIRES_STR ("180"),
                                      NUTAG_SUPPORTED ("replaces, outbound, gruu"),
                                      NUTAG_OUTBOUND ("outbound natify gruuize validate"), // <- janus uses "no-validate"
                                      NUTAG_M_PARAMS ("user=phone"),
                                      NUTAG_CALLEE_CAPS (1), /* header parameters based on SDP capabilities and Allow header */
                                      TAG_END ());
  oper->call_handle = NULL;
  return oper;
}


static void
go_online (CallsAccount *account,
           gboolean      online)
{
  CallsSipOrigin *self;

  g_assert (CALLS_IS_ACCOUNT (account));
  g_assert (CALLS_IS_SIP_ORIGIN (account));

  self = CALLS_SIP_ORIGIN (account);

  if (!self->nua) {
    g_warning ("Cannot go online: nua handle not initialized");
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACC_STATE]);
    return;
  }

  if (online) {
    g_autofree char *registrar_url = NULL;

    if (self->state == CALLS_ACCOUNT_ONLINE)
      return;

    registrar_url = get_registrar_url (self);

    nua_register (self->oper->register_handle,
                  NUTAG_M_USERNAME (self->user),
                  TAG_IF (self->display_name, NUTAG_M_DISPLAY (self->display_name)),
                  NUTAG_REGISTRAR (registrar_url),
                  TAG_END ());

  } else {
    if (self->state == CALLS_ACCOUNT_OFFLINE)
      return;

    nua_unregister (self->oper->register_handle,
                    TAG_END ());
  }
}

static const char *
get_address (CallsAccount *account)
{
  CallsSipOrigin *self;

  g_assert (CALLS_IS_ACCOUNT (account));
  g_assert (CALLS_IS_SIP_ORIGIN (account));

  self = CALLS_SIP_ORIGIN (account);

  return self->address;
}


static void
setup_account_for_direct_connection (CallsSipOrigin *self)
{
  g_assert (CALLS_IS_SIP_ORIGIN (self));

  /* honour username, if previously set */
  if (self->user == NULL)
    self->user = g_strdup (g_get_user_name ());

  g_free (self->host);
  self->host = g_strdup (g_get_host_name ());

  g_clear_pointer (&self->password, g_free);

  g_free (self->transport_protocol);
  self->transport_protocol = g_strdup ("UDP");

  self->protocol_prefix = get_protocol_prefix ("UDP");

  g_debug ("Account changed:\nuser: %s\nhost: %s",
           self->user, self->host);
}


static gboolean
is_account_complete (CallsSipOrigin *self)
{
  /* we need only need to check for password if needing to authenticate over a proxy/UAS */
  if (self->user == NULL ||
      (!self->use_direct_connection && self->password == NULL) ||
      self->host == NULL ||
      self->transport_protocol == NULL)
    return FALSE;

  return TRUE;
}


static gboolean
init_sip_account (CallsSipOrigin *self,
                  GError        **error)
{
  if (self->use_direct_connection) {
    g_debug ("Direct connection case. Using user and hostname");
    setup_account_for_direct_connection (self);
  }

  if (!is_account_complete (self)) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Must have completed account setup before calling "
                 "init_sip_account (). "
                 "Try again when account is setup");

    self->state = CALLS_ACCOUNT_NO_CREDENTIALS;
    goto err;
  }

  // setup_nua() and setup_sip_handles() only after account data has been set
  self->nua = setup_nua (self);
  if (self->nua == NULL) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed setting up nua context");

    self->state = CALLS_ACCOUNT_NULL;
    goto err;
  }

  self->oper = setup_sip_handles (self);
  if (self->oper == NULL) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed setting operation handles");

    self->state = CALLS_ACCOUNT_NULL;
    goto err;
  }

  /* In the case of a direct connection we're immediately good to go */
  if (self->use_direct_connection)
    self->state = CALLS_ACCOUNT_ONLINE;
  else {
    self->state = CALLS_ACCOUNT_OFFLINE;

    if (self->auto_connect)
      go_online (CALLS_ACCOUNT (self), TRUE);
  }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACC_STATE]);
  return TRUE;

 err:
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACC_STATE]);
  return FALSE;
}


static gboolean
deinit_sip_account (CallsSipOrigin *self)
{
  g_assert (CALLS_IS_SIP_ORIGIN (self));

  if (self->state == CALLS_ACCOUNT_NULL)
    return TRUE;

  remove_calls (self, NULL);

  if (self->nua) {
    g_debug ("Clearing any handles");
    g_clear_pointer (&self->oper->register_handle, nua_handle_destroy);
    g_debug ("Requesting nua_shutdown ()");
    self->is_nua_shutdown = FALSE;
    self->is_shutdown_success = FALSE;
    nua_shutdown (self->nua);
    // need to wait for nua_r_shutdown event before calling nua_destroy ()
    while (!self->is_nua_shutdown)
      su_root_step (self->ctx->root, 100);

    if (!self->is_shutdown_success) {
      g_warning ("nua_shutdown() timed out. Cannot proceed");
      return FALSE;
    }
    g_debug ("nua_shutdown() complete. Destroying nua handle");
    nua_destroy (self->nua);
    self->nua = NULL;
  }

  self->state = CALLS_ACCOUNT_NULL;
  return TRUE;
}


static void
on_network_changed (CallsSipOrigin *self)
{
  if (deinit_sip_account (self))
    init_sip_account (self, NULL);
}


static gboolean
supports_protocol (CallsOrigin *origin,
                   const char  *protocol)
{
  CallsSipOrigin *self;
  g_assert (protocol);
  g_assert (CALLS_IS_SIP_ORIGIN (origin));

  self = CALLS_SIP_ORIGIN (origin);

  if (g_strcmp0 (protocol, "sip") == 0)
    return TRUE;
  if (g_strcmp0 (protocol, "sips") == 0)
    return g_strcmp0 (self->protocol_prefix, "sips") == 0;

  /* TODO need to set a property (from the UI) to allow using origin for telephony */
  return FALSE;
}


static void
update_name (CallsSipOrigin *self)
{
  g_assert (CALLS_IS_SIP_ORIGIN (self));

  if (self->display_name && self->display_name[0] != '\0')
    self->name = self->display_name;
  else
    self->name = self->user;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}


static void
calls_sip_origin_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CallsSipOrigin *self = CALLS_SIP_ORIGIN (object);

  switch (property_id) {
  case PROP_ACC_HOST:
    g_free (self->host);
    self->host = g_value_dup_string (value);
    break;

  case PROP_ACC_DISPLAY_NAME:
    g_free (self->display_name);
    self->display_name = g_value_dup_string (value);
    break;

  case PROP_ACC_USER:
    g_free (self->user);
    self->user = g_value_dup_string (value);
    break;

  case PROP_ACC_PASSWORD:
    g_free (self->password);
    self->password = g_value_dup_string (value);
    break;

  case PROP_ACC_PORT:
    self->port = g_value_get_int (value);
    break;

  case PROP_ACC_PROTOCOL:
    g_free (self->transport_protocol);
    self->transport_protocol = g_value_dup_string (value);
    break;

  case PROP_ACC_AUTO_CONNECT:
    self->auto_connect = g_value_get_boolean (value);
    break;

  case PROP_ACC_DIRECT:
    self->use_direct_connection = g_value_get_boolean (value);
    break;

  case PROP_SIP_CONTEXT:
    self->ctx = g_value_get_pointer (value);
    break;

  case PROP_ACC_LOCAL_PORT:
    if (g_value_get_int (value) > 0 && g_value_get_int (value) < 1025) {
      g_warning ("Tried setting a privileged port as the local port to bind to: %d\n"
                 "Continue using old 'local-port' value: %d (using 0 let's the OS decide)",
                 g_value_get_int (value), self->local_port);
      return;
    }
    self->local_port = g_value_get_int (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_sip_origin_get_property (GObject      *object,
                               guint         property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  CallsSipOrigin *self = CALLS_SIP_ORIGIN (object);
  g_autofree char *name = NULL;

  switch (property_id) {
  case PROP_NAME:
    g_value_set_string (value, self->name);
    break;
  case PROP_ACC_HOST:
    g_value_set_string (value, self->host);
    break;

  case PROP_ACC_DISPLAY_NAME:
    g_value_set_string (value, self->display_name);
    break;

  case PROP_ACC_USER:
    g_value_set_string (value, self->user);
    break;

  case PROP_ACC_PASSWORD:
    g_value_set_string (value, self->password);
    break;

  case PROP_ACC_PORT:
    g_value_set_int (value, self->port);
    break;

  case PROP_ACC_PROTOCOL:
    g_value_set_string (value, self->transport_protocol);
    break;

  case PROP_ACC_AUTO_CONNECT:
    g_value_set_boolean (value, self->auto_connect);
    break;

  case PROP_ACC_DIRECT:
    g_value_set_boolean (value, self->direct_mode);
    break;

  case PROP_ACC_LOCAL_PORT:
    g_value_set_int (value, self->local_port);
    break;

  case PROP_CALLS:
    g_value_set_pointer (value, g_list_copy (self->calls));
    break;

  case PROP_ACC_STATE:
    g_value_set_enum (value, self->state);
    break;

  case PROP_ACC_ADDRESS:
    g_value_set_string (value, get_address (CALLS_ACCOUNT (self)));
    break;

  case PROP_COUNTRY_CODE:
    g_value_set_string (value, NULL);
    break;

  case PROP_NUMERIC:
    g_value_set_boolean (value, FALSE);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_sip_origin_constructed (GObject *object)
{
  CallsSipOrigin *self = CALLS_SIP_ORIGIN (object);
  g_autoptr (GError) error = NULL;
  int major = 0;
  int minor = 0;
  int patch = 0;

  if (sscanf (SOFIA_SIP_VERSION, "%d.%d.%d", &major, &minor, &patch) == 3) {
    if (major > 2 || (major == 1 && minor >= 13)) {
      /* Sofia 1.13 does no longer include the contact header by default, see #304 */
      set_contact_header = TRUE;
    }
  }

  if (!init_sip_account (self, &error)) {
    g_warning ("Error initializing the SIP account: %s", error->message);
  }

  update_name (self);

  self->media_manager = calls_sip_media_manager_default ();

  G_OBJECT_CLASS (calls_sip_origin_parent_class)->constructed (object);
}


static void
calls_sip_origin_dispose (GObject *object)
{
  CallsSipOrigin *self = CALLS_SIP_ORIGIN (object);

  if (!self->use_direct_connection && self->state == CALLS_ACCOUNT_ONLINE)
    go_online (CALLS_ACCOUNT (self), FALSE);

  deinit_sip_account (self);

  G_OBJECT_CLASS (calls_sip_origin_parent_class)->dispose (object);
}


static void
calls_sip_origin_finalize (GObject *object)
{
  CallsSipOrigin *self = CALLS_SIP_ORIGIN (object);

  g_hash_table_destroy (self->call_handles);

  G_OBJECT_CLASS (calls_sip_origin_parent_class)->finalize (object);
}


static void
calls_sip_origin_class_init (CallsSipOriginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = calls_sip_origin_constructed;
  object_class->dispose = calls_sip_origin_dispose;
  object_class->finalize = calls_sip_origin_finalize;
  object_class->get_property = calls_sip_origin_get_property;
  object_class->set_property = calls_sip_origin_set_property;

  props[PROP_ACC_HOST] =
    g_param_spec_string ("host",
                         "Host",
                         "The host to connect to",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACC_HOST, props[PROP_ACC_HOST]);

  props[PROP_ACC_USER] =
    g_param_spec_string ("user",
                         "User",
                         "The username",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACC_USER, props[PROP_ACC_USER]);

  props[PROP_ACC_PASSWORD] =
    g_param_spec_string ("password",
                         "Password",
                         "The password",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACC_PASSWORD, props[PROP_ACC_PASSWORD]);

  props[PROP_ACC_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display name",
                         "The display name",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACC_DISPLAY_NAME, props[PROP_ACC_DISPLAY_NAME]);

  props[PROP_ACC_PORT] =
    g_param_spec_int ("port",
                      "Port",
                      "The port to connect to",
                      0, 65535, 0,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACC_PORT, props[PROP_ACC_PORT]);

  props[PROP_ACC_PROTOCOL] =
    g_param_spec_string ("transport-protocol",
                         "Transport protocol",
                         "The transport protocol to use for the connection",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACC_PROTOCOL, props[PROP_ACC_PROTOCOL]);

  props[PROP_ACC_AUTO_CONNECT] =
    g_param_spec_boolean ("auto-connect",
                          "Auto connect",
                          "Whether to connect automatically",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACC_AUTO_CONNECT, props[PROP_ACC_AUTO_CONNECT]);


  props[PROP_ACC_DIRECT] =
    g_param_spec_boolean ("direct-mode",
                          "Direct mode",
                          "Whether to use a direct connection (no SIP server)",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACC_DIRECT, props[PROP_ACC_DIRECT]);

  props[PROP_ACC_LOCAL_PORT] =
    g_param_spec_int ("local-port",
                      "Local port",
                      "The local port to which the SIP stack binds to",
                      0, 65535, 0,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_ACC_LOCAL_PORT, props[PROP_ACC_LOCAL_PORT]);

 props[PROP_SIP_CONTEXT] =
    g_param_spec_pointer ("sip-context",
                          "SIP context",
                          "The SIP context (sofia) used for our sip handles",
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_SIP_CONTEXT, props[PROP_SIP_CONTEXT]);

  g_object_class_override_property (object_class, PROP_ACC_STATE, "account-state");
  props[PROP_ACC_STATE] = g_object_class_find_property (object_class, "account-state");

  g_object_class_override_property (object_class, PROP_ACC_ADDRESS, "address");
  props[PROP_ACC_ADDRESS] = g_object_class_find_property (object_class, "address");

#define IMPLEMENTS(ID, NAME) \
  g_object_class_override_property (object_class, ID, NAME);    \
  props[ID] = g_object_class_find_property(object_class, NAME);

  IMPLEMENTS (PROP_NAME, "name");
  IMPLEMENTS (PROP_CALLS, "calls");
  IMPLEMENTS (PROP_COUNTRY_CODE, "country-code");
  IMPLEMENTS (PROP_NUMERIC, "numeric-addresses");

#undef IMPLEMENTS
}


static void
calls_sip_origin_message_source_interface_init (CallsOriginInterface *iface)
{
}


static void
calls_sip_origin_origin_interface_init (CallsOriginInterface *iface)
{
  iface->dial = dial;
  iface->supports_protocol = supports_protocol;
}

static void
calls_sip_origin_accounts_interface_init (CallsAccountInterface *iface)
{
  iface->go_online = go_online;
  iface->get_address = get_address;
}


static void
calls_sip_origin_init (CallsSipOrigin *self)
{
  const char *sip_test_env = g_getenv ("CALLS_SIP_TEST");

  if (!sip_test_env || sip_test_env[0] == '\0')
    g_signal_connect_swapped (calls_network_watch_get_default (), "network-changed",
                              G_CALLBACK (on_network_changed), self);

  self->call_handles = g_hash_table_new (NULL, NULL);

}

void
calls_sip_origin_set_credentials (CallsSipOrigin *self,
                                  const char     *host,
                                  const char     *user,
                                  const char     *password,
                                  const char     *display_name,
                                  const char     *transport_protocol,
                                  gint            port,
                                  gboolean        auto_connect)
{
  g_return_if_fail (CALLS_IS_SIP_ORIGIN (self));

  if (self->direct_mode) {
    g_warning ("Not allowed to update credentials when using direct mode");
    return;
  }

  g_return_if_fail (host);
  g_return_if_fail (user);
  g_return_if_fail (password);

  if (transport_protocol)
    g_return_if_fail (protocol_is_valid (transport_protocol));

  g_free (self->host);
  self->host = g_strdup (host);

  g_free (self->user);
  self->user = g_strdup (user);

  g_free (self->password);
  self->password = g_strdup (password);

  g_clear_pointer (&self->display_name, g_free);
  if (display_name)
    self->display_name = g_strdup (display_name);

  g_free (self->transport_protocol);
  if (transport_protocol)
    self->transport_protocol = g_strdup (transport_protocol);
  else
    self->transport_protocol = g_strdup ("UDP");

  self->port = port;

  update_name (self);

  /* Propagate changes to nua stack */
  update_nua (self);
  /* TODO:
   * We need to recreate the nua stack when the transport protocol changes
   * because nua_set_params cannot be used to update NUTAG_URL and friends.
   * This will get easier with https://gitlab.gnome.org/GNOME/calls/-/merge_requests/402
   */
}
