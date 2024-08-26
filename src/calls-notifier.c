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

#include "calls-config.h"

#include "calls-manager.h"
#include "calls-notifier.h"
#include "calls-ui-call-data.h"
#include "calls-util.h"

#include <cui-call.h>

#include <glib/gi18n.h>
#include <glib-object.h>

struct _CallsNotifier {
  GObject     parent_instance;

  GListStore *unanswered;
};

G_DEFINE_TYPE (CallsNotifier, calls_notifier, G_TYPE_OBJECT);


static void
notify (CallsNotifier *self, CuiCall *call)
{
  GApplication *app = g_application_get_default ();

  g_autoptr (GNotification) notification = g_notification_new (_("Missed call"));
  g_autofree gchar *msg = NULL;
  g_autofree gchar *ref = NULL;
  g_autofree gchar *label_callback = NULL;
  const char *name;
  const char *id;
  gboolean got_id;
  gboolean got_contact;

  g_notification_set_category (notification, "call.unanswered");
  /* TODO: We need to update the notification when the contact name changes
     We would need to resend the notification in this case, as changing the properties
     after having called g_application_send_notification() will have no effect.
   */
  name = cui_call_get_display_name (call);
  id = cui_call_get_id (call);

  got_id = !STR_IS_NULL_OR_EMPTY (id);
  got_contact = !STR_IS_NULL_OR_EMPTY (name);

  if (got_contact)
    /* %s is a name here */
    msg = g_strdup_printf (_("Missed call from <b>%s</b>"), name);
  else if (got_id)
    /* %s is a id here */
    msg = g_strdup_printf (_("Missed call from %s"), id);
  else
    msg = g_strdup (_("Missed call from unknown caller"));

  g_notification_set_body (notification, msg);

  if (got_id) {
    label_callback = g_strdup_printf ("app.dial::%s", id);
    g_notification_add_button (notification, _("Call back"), label_callback);
  }

  ref = g_strdup_printf ("missed-call-%s", id ?: "unknown");
  g_application_send_notification (app, ref, notification);
}


static void
state_changed_cb (CallsNotifier *self,
                  CuiCallState   new_state,
                  CuiCallState   old_state,
                  CuiCall       *call)
{
  guint n;

  g_return_if_fail (CALLS_IS_NOTIFIER (self));
  g_return_if_fail (CUI_IS_CALL (call));
  g_return_if_fail (old_state != new_state);

  if (old_state == CUI_CALL_STATE_INCOMING &&
      new_state == CUI_CALL_STATE_DISCONNECTED)
    notify (self, call);

  /* Can use g_list_store_find with newer glib */
  n = g_list_model_get_n_items (G_LIST_MODEL (self->unanswered));
  for (int i = 0; i < n; i++) {
    g_autoptr (CuiCall) item = g_list_model_get_item (G_LIST_MODEL (self->unanswered), i);
    if (item == call) {
      g_list_store_remove (self->unanswered, i);
      g_signal_handlers_disconnect_by_data (item, self);
    }
  }
}


static void
call_added_cb (CallsNotifier *self, CuiCall *call)
{
  g_list_store_append (self->unanswered, call);

  g_signal_connect_swapped (call,
                            "state-changed",
                            G_CALLBACK (state_changed_cb),
                            self);
}


static void
calls_notifier_init (CallsNotifier *self)
{
  self->unanswered = g_list_store_new (CUI_TYPE_CALL);
}


static void
calls_notifier_constructed (GObject *object)
{
  g_autoptr (GList) calls = NULL;
  GList *c;
  CallsNotifier *self = CALLS_NOTIFIER (object);

  G_OBJECT_CLASS (calls_notifier_parent_class)->constructed (object);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "ui-call-added",
                            G_CALLBACK (call_added_cb),
                            self);

  calls = calls_manager_get_calls (calls_manager_get_default ());
  for (c = calls; c != NULL; c = c->next)
    call_added_cb (self, c->data);
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
