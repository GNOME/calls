/*
 * Copyright (C) 2020 Purism SPC
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
 * Author: Guido Günther <agx@sigxcpu.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define G_LOG_DOMAIN "CallsNotifier"

#include "calls-notifier.h"
#include "calls-manager.h"
#include "config.h"

#include <glib/gi18n.h>
#include <glib-object.h>

struct _CallsNotifier
{
  GObject parent_instance;

  GListStore    *unanswered;
  GList         *notifications;
};

G_DEFINE_TYPE (CallsNotifier, calls_notifier, G_TYPE_OBJECT);

static void
notify (CallsNotifier *self, CallsCall *call)
{
  GApplication *app = g_application_get_default ();
  g_autoptr (GNotification) notification = g_notification_new (_("Missed call"));
  g_autoptr (CallsBestMatch) contact = NULL;
  g_autofree gchar *msg = NULL;
  g_autofree gchar *ref = NULL;
  g_autofree gchar *label_callback = NULL;
  const char *name;
  const char *number;
  gboolean got_number;

#if GLIB_CHECK_VERSION(2,70,0)
  g_notification_set_category (notification, "x-gnome.call.unanswered");
#endif
  contact = calls_call_get_contact (call);
  // TODO: We need to update the notification when the contact name changes
  name = calls_best_match_get_name (contact);
  number = calls_call_get_number (call);

  got_number = !!number && (g_strcmp0 (number, "") != 0);

  if (calls_best_match_has_individual (contact))
    msg = g_strdup_printf (_("Missed call from <b>%s</b>"), name);
  else if (got_number)
    msg = g_strdup_printf (_("Missed call from %s"), number);
  else
    msg = g_strdup (_("Missed call from unknown caller"));

  g_notification_set_body (notification, msg);

  if (got_number) {
    label_callback = g_strdup_printf ("app.dial::%s", number);
    g_notification_add_button (notification, _("Call back"), label_callback);
  }

  ref = g_strdup_printf ("missed-call-%s", number ?: "unknown");
  g_application_send_notification (app, ref, notification);
}


static void
state_changed_cb (CallsNotifier  *self,
                  CallsCallState new_state,
                  CallsCallState old_state,
                  CallsCall      *call)
{
  guint n;

  g_return_if_fail (CALLS_IS_NOTIFIER (self));
  g_return_if_fail (CALLS_IS_CALL (call));
  g_return_if_fail (old_state != new_state);

  if (old_state == CALLS_CALL_STATE_INCOMING &&
      new_state == CALLS_CALL_STATE_DISCONNECTED)
    {
      notify (self, call);
    }

  /* Can use g_list_store_find with newer glib */
  n = g_list_model_get_n_items (G_LIST_MODEL (self->unanswered));
  for (int i = 0; i < n; i++)
    {
      g_autoptr (CallsCall) item = g_list_model_get_item (G_LIST_MODEL (self->unanswered), i);
      if (item == call)
        {
          g_list_store_remove (self->unanswered, i);
          g_signal_handlers_disconnect_by_data (item, self);
        }
    }
}

static void
call_added_cb (CallsNotifier *self, CallsCall *call)
{
  g_list_store_append(self->unanswered, call);

  g_signal_connect_swapped (call,
                            "state-changed",
                            G_CALLBACK (state_changed_cb),
                            self);
}


static void
calls_notifier_init (CallsNotifier *self)
{
  self->unanswered = g_list_store_new (CALLS_TYPE_CALL);
}


static void
calls_notifier_constructed (GObject *object)
{
  g_autoptr (GList) calls = NULL;
  GList *c;
  CallsNotifier *self = CALLS_NOTIFIER (object);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "call-add",
                            G_CALLBACK (call_added_cb),
                            self);

  calls = calls_manager_get_calls (calls_manager_get_default ());
  for (c = calls; c != NULL; c = c->next)
    {
      call_added_cb (self, c->data);
    }

  G_OBJECT_CLASS (calls_notifier_parent_class)->constructed (object);
}


static void
calls_notifier_dispose (GObject *object)
{
  CallsNotifier *self = CALLS_NOTIFIER (object);

  g_list_store_remove_all (self->unanswered);
  g_clear_object (&self->unanswered);

  G_OBJECT_CLASS (calls_notifier_parent_class)->dispose (object);
}


static void
calls_notifier_class_init (CallsNotifierClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = calls_notifier_constructed;
  object_class->dispose = calls_notifier_dispose;
}

CallsNotifier *
calls_notifier_new (void)
{
  return g_object_new (CALLS_TYPE_NOTIFIER, NULL);
}
