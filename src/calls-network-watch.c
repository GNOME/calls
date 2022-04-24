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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Calls. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define G_LOG_DOMAIN "CallsNetworkWatch"

#include "calls-network-watch.h"

#include <gio/gio.h>

#include <arpa/inet.h>
#include <linux/rtnetlink.h>

/**
 * SECTION:calls-network-watch
 * @short_description: Watches network interfaces
 * @Title: CallsNetworkWatch
 *
 * The #CallsNetworkWatch uses rtnetlink messages to keep
 * track of network interfaces.
 *
 * This allows the sofia SIP stack to bind to the
 * correct interface and can later help in deciding which codec to
 * use for media (f.e. lower bandwidth on a metered connection).
 */


typedef struct {
  struct nlmsghdr n;
  struct rtmsg    r;
  char            buf[1024];
} RequestData;

enum {
  PROP_0,
  PROP_IPV4,
  PROP_IPV6,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

enum {
  NETWORK_CHANGED,
  N_SIGNALS
};
static guint signals[N_SIGNALS];


typedef struct _CallsNetworkWatch {
  GObject      parent;

  RequestData *req;
  int          fd;
  unsigned int seq;
  char         buf[1024]; /* buffer for responses to rtnetlink requests */

  guint        timeout_id;

  gboolean     repeated_warning;

  char        *ipv4;
  char        *ipv6;
  char         tmp_addr[INET6_ADDRSTRLEN];
} CallsNetworkWatch;


static void initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsNetworkWatch, calls_network_watch, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

#define DUMMY_TARGET_V4 "1.2.3.4"
#define DUMMY_TARGET_V6 "::1.2.3.4"

static gboolean
req_route_v4 (CallsNetworkWatch *self)
{
  int addr_len = 4;
  int len = RTA_LENGTH (addr_len);
  struct rtattr *rta;

  g_assert (CALLS_IS_NETWORK_WATCH (self));

  self->req->n.nlmsg_len = NLMSG_LENGTH (sizeof (struct rtmsg));
  self->req->n.nlmsg_flags = NLM_F_REQUEST;
  self->req->n.nlmsg_type = RTM_GETROUTE;
  self->req->r.rtm_family = AF_INET;


  rta = ((struct rtattr *) (((void *) (&self->req->n)) +
                            NLMSG_ALIGN (self->req->n.nlmsg_len)));
  rta->rta_type = RTA_DST;
  rta->rta_len = len;

  /* use a dummy target destination */
  if (inet_pton (AF_INET, DUMMY_TARGET_V4, RTA_DATA (rta)) != 1)
    return FALSE;

  self->req->n.nlmsg_len = NLMSG_ALIGN (self->req->n.nlmsg_len) + RTA_ALIGN (len);

  return TRUE;
}


static gboolean
req_route_v6 (CallsNetworkWatch *self)
{
  int addr_len = 16;
  int len = RTA_LENGTH (addr_len);
  struct rtattr *rta;

  g_assert (CALLS_IS_NETWORK_WATCH (self));

  self->req->n.nlmsg_len = NLMSG_LENGTH (sizeof (struct rtmsg));
  self->req->n.nlmsg_flags = NLM_F_REQUEST;
  self->req->n.nlmsg_type = RTM_GETROUTE;
  self->req->r.rtm_family = AF_INET6;

  rta = ((struct rtattr *) (((void *) (&self->req->n)) +
                            NLMSG_ALIGN (self->req->n.nlmsg_len)));
  rta->rta_type = RTA_DST;
  rta->rta_len = len;

  /* use a dummy target destination */
  if (inet_pton (AF_INET6, DUMMY_TARGET_V6, RTA_DATA (rta)) != 1)
    return FALSE;

  self->req->n.nlmsg_len = NLMSG_ALIGN (self->req->n.nlmsg_len) + RTA_ALIGN (len);

  return TRUE;
}

#undef DUMMY_TARGET_V4
#undef DUMMY_TARGET_V6

static gboolean
talk_rtnl (CallsNetworkWatch *self)
{
  struct iovec iov;
  struct iovec riov;
  struct sockaddr_nl nladdr = { .nl_family = AF_NETLINK };
  struct msghdr msg = {
    .msg_name = &nladdr,
    .msg_namelen = sizeof (nladdr),
    .msg_iov = &iov,
    .msg_iovlen = 1,
  };

  struct nlmsghdr *h;
  int status;

  g_assert (CALLS_IS_NETWORK_WATCH (self));

  iov.iov_base = (void *) &self->req->n;
  iov.iov_len = self->req->n.nlmsg_len;
  self->req->n.nlmsg_seq = self->seq++;

  status = sendmsg (self->fd, &msg, 0);
  if (status < 0) {
    g_warning ("Could not send rtnetlink: %d", errno);
    return FALSE;
  }

  /* change msg to use response iov */
  riov.iov_base = self->buf;
  riov.iov_len = sizeof (self->buf);

  msg.msg_iov = &riov;
  msg.msg_iovlen = 1;

  status = recvmsg (self->fd, &msg, 0);

  if (status == -1) {
    g_warning ("Could not receive rtnetlink: %d", errno);
    return FALSE;
  }

  h = (struct nlmsghdr *) self->buf;
  if (h->nlmsg_type == NLMSG_ERROR) {
    /* TODO figure out why this fails and provide more information in the warning */
    if (!self->repeated_warning)
      g_warning ("Unexpected error response to netlink request while trying "
                 "to fetch local IP address");

    self->repeated_warning = TRUE;
    return FALSE;
  }

  self->repeated_warning = FALSE;

  return TRUE;
}

static gboolean
get_prefsrc (CallsNetworkWatch *self,
             int                family)
{
  struct nlmsghdr *h;
  struct rtmsg *r;
  struct rtattr *rta;
  int len;
  gboolean found = FALSE;

  g_assert (CALLS_IS_NETWORK_WATCH (self));

  h = (struct nlmsghdr *) self->buf;
  r = NLMSG_DATA (self->buf);
  rta = RTM_RTA (r);
  len = h->nlmsg_len - NLMSG_LENGTH (sizeof (*r));

  while (RTA_OK (rta, len)) {
    if (rta->rta_type == RTA_PREFSRC) {
      found = TRUE;
      break;
    }
    rta = RTA_NEXT (rta, len);
  }

  if (!found)
    return FALSE;

  if (family == AF_INET) {
    inet_ntop (AF_INET, RTA_DATA (rta), self->tmp_addr, INET_ADDRSTRLEN);
  } else if (family == AF_INET6) {
    inet_ntop (AF_INET6, RTA_DATA (rta), self->tmp_addr, INET6_ADDRSTRLEN);
  } else {
    return FALSE;
  }

  return TRUE;
}

static gboolean
fetch_ipv4 (CallsNetworkWatch *self)
{
  g_assert (CALLS_IS_NETWORK_WATCH (self));

  if (!req_route_v4 (self))
    return FALSE;

  if (!talk_rtnl (self))
    return FALSE;

  return get_prefsrc (self, AF_INET);
}

static gboolean
fetch_ipv6 (CallsNetworkWatch *self)
{
  g_assert (CALLS_IS_NETWORK_WATCH (self));

  if (!req_route_v6 (self))
    return FALSE;

  if (!talk_rtnl (self))
    return FALSE;

  return get_prefsrc (self, AF_INET6);
}


static gboolean
on_watch_network (CallsNetworkWatch *self)
{
  gboolean changed_v4 = FALSE;
  gboolean changed_v6 = FALSE;

  changed_v4 = fetch_ipv4 (self) && g_strcmp0 (self->tmp_addr, self->ipv4) != 0;
  if (changed_v4) {
    g_free (self->ipv4);
    self->ipv4 = g_strdup (self->tmp_addr);
    g_debug ("New IPv4: %s", self->ipv4);
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IPV4]);
  }

  changed_v6 = fetch_ipv6 (self) && g_strcmp0 (self->tmp_addr, self->ipv6) != 0;
  if (changed_v6) {
    g_free (self->ipv6);
    self->ipv6 = g_strdup (self->tmp_addr);
    g_debug ("New IPv6: %s", self->ipv6);
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_IPV6]);
  }

  if (changed_v4 || changed_v6)
    g_signal_emit (self, signals[NETWORK_CHANGED], 0);

  return G_SOURCE_CONTINUE;
}

static void
calls_network_watch_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  CallsNetworkWatch *self = CALLS_NETWORK_WATCH (object);

  switch (property_id) {
  case PROP_IPV4:
    g_value_set_string (value, calls_network_watch_get_ipv4 (self));
    break;

  case PROP_IPV6:
    g_value_set_string (value, calls_network_watch_get_ipv6 (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_network_watch_finalize (GObject *object)
{
  CallsNetworkWatch *self = CALLS_NETWORK_WATCH (object);

  g_source_remove (self->timeout_id);
  g_free (self->req);
  g_free (self->ipv4);
  g_free (self->ipv6);
  close (self->fd);

  G_OBJECT_CLASS (calls_network_watch_parent_class)->finalize (object);
}

static void
calls_network_watch_class_init (CallsNetworkWatchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = calls_network_watch_get_property;
  object_class->finalize = calls_network_watch_finalize;

  signals[NETWORK_CHANGED] = g_signal_new ("network-changed",
                                           CALLS_TYPE_NETWORK_WATCH,
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE,
                                           0);

  props[PROP_IPV4] =
    g_param_spec_string ("ipv4",
                         "IPv4",
                         "The preferred source address for IPv4",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_IPV6] =
    g_param_spec_string ("ipv6",
                         "IPv6",
                         "The preferred source address for IPv6",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
}


static void
calls_network_watch_init (CallsNetworkWatch *self)
{
  self->seq = time (NULL);
  self->req = g_new0 (RequestData, 1);
  self->timeout_id = g_timeout_add_seconds (15,
                                            G_SOURCE_FUNC (on_watch_network),
                                            self);
}


static gboolean
calls_network_watch_initable_init (GInitable    *initable,
                                   GCancellable *cancelable,
                                   GError      **error)
{
  CallsNetworkWatch *self = CALLS_NETWORK_WATCH (initable);

  self->fd = socket (AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (self->fd == -1 && error) {
    int errsv = errno;
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                 "Failed to create netlink socket: %d", errsv);
    return FALSE;
  }

  if (fetch_ipv4 (self))
    self->ipv4 = g_strdup (self->tmp_addr);
  else
    self->ipv4 = g_strdup ("127.0.0.1");

  if (fetch_ipv6 (self))
    self->ipv6 = g_strdup (self->tmp_addr);
  else
    self->ipv6 = g_strdup ("::1");

  return TRUE;
}


static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = calls_network_watch_initable_init;
}


CallsNetworkWatch *
calls_network_watch_get_default (void)
{
  static CallsNetworkWatch *instance;

  if (instance == NULL) {
    g_autoptr (GError) error = NULL;
    instance = g_initable_new (CALLS_TYPE_NETWORK_WATCH, NULL, &error, NULL);

    if (!instance)
      g_warning ("Network watch could not be initialized: %s", error->message);
  }
  return instance;
}


const char *
calls_network_watch_get_ipv4 (CallsNetworkWatch *self)
{
  g_return_val_if_fail (CALLS_IS_NETWORK_WATCH (self), NULL);

  return self->ipv4;
}


const char *
calls_network_watch_get_ipv6 (CallsNetworkWatch *self)
{
  g_return_val_if_fail (CALLS_IS_NETWORK_WATCH (self), NULL);

  return self->ipv6;
}
