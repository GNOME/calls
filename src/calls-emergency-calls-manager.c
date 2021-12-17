/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "CallsEmergencyCallsManger"

#include "calls-emergency-calls-manager.h"
#include "calls-origin.h"
#include "calls-manager.h"

/**
 * SECTION:calls-emergency-calls-manager
 * @short_description: Provide a DBus interface for emergency calls
 * @Title: CallsEmergencyCallsManager
 *
 * This tracks emergency call information from the various origins
 * and makes them available for initiating emergency calls.
 */

typedef struct _CallsEmergencyCallsManager
{
  CallsDBusEmergencyCallsSkeleton parent;

  GListModel *origins;
} CallsEmergencyCallsManger;

static void calls_emergency_calls_iface_init (CallsDBusEmergencyCallsIface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsEmergencyCallsManager,
                         calls_emergency_calls_manager,
                         CALLS_DBUS_TYPE_EMERGENCY_CALLS_SKELETON,
                         G_IMPLEMENT_INTERFACE (
                           CALLS_DBUS_TYPE_EMERGENCY_CALLS,
                           calls_emergency_calls_iface_init));

static void
on_origins_changed (CallsEmergencyCallsManger *self)
{
  g_assert (CALLS_IS_EMERGENCY_CALLS_MANAGER (self));

  g_signal_emit_by_name (self, "emergency-numbers-changed", 0);
}


static gboolean
handle_call_emergency_contact (CallsDBusEmergencyCalls *object,
                               GDBusMethodInvocation *invocation,
                               const gchar *arg_id)
{
  g_debug ("Calling %s", arg_id);

  calls_dbus_emergency_calls_complete_call_emergency_contact (object, invocation);
  return TRUE;
}


#define CONTACT_FORMAT "(ssia{sv})"
#define CONTACTS_FORMAT "a" CONTACT_FORMAT

static gboolean
handle_get_emergency_contacts (CallsDBusEmergencyCalls *object,
                               GDBusMethodInvocation   *invocation)
{
  GVariant *contacts;
  GVariantBuilder contacts_builder;
  CallsEmergencyCallsManger *self = CALLS_EMERGENCY_CALLS_MANAGER (object);

  g_variant_builder_init (&contacts_builder, G_VARIANT_TYPE (CONTACTS_FORMAT));

  for (int i = 0; i < g_list_model_get_n_items (self->origins); i++) {
    CallsOrigin *origin = g_list_model_get_item (self->origins, i);
    g_auto (GStrv) emergency_numbers = NULL;

    emergency_numbers = calls_origin_get_emergency_numbers (origin);
    if (!emergency_numbers)
      continue;

    for (int j = 0; j < g_strv_length (emergency_numbers); j++) {
      g_variant_builder_open (&contacts_builder, G_VARIANT_TYPE (CONTACT_FORMAT));
      g_variant_builder_add (&contacts_builder, "s", emergency_numbers[j]);
      /* For non-addressbook numbers we just use the number itself as contact */
      g_variant_builder_add (&contacts_builder, "s", emergency_numbers[j]);
      /* Currently unused */
      g_variant_builder_add (&contacts_builder, "i", 0);
      /* Currently no hints */
      g_variant_builder_add (&contacts_builder, "a{sv}", NULL);
      g_variant_builder_close (&contacts_builder);
    }
  }
  contacts = g_variant_builder_end (&contacts_builder);

  calls_dbus_emergency_calls_complete_get_emergency_contacts (object, invocation, contacts);

  return TRUE;
}

static void
calls_emergency_calls_iface_init (CallsDBusEmergencyCallsIface *iface)
{
  iface->handle_call_emergency_contact = handle_call_emergency_contact;
  iface->handle_get_emergency_contacts = handle_get_emergency_contacts;
}

static void
calls_emergency_calls_manager_dispose (GObject *object)
{
  CallsEmergencyCallsManager *self = CALLS_EMERGENCY_CALLS_MANAGER (object);

  g_clear_object (&self->origins);

  G_OBJECT_CLASS (calls_emergency_calls_manager_parent_class)->dispose (object);
}

static void
calls_emergency_calls_manager_class_init (CallsEmergencyCallsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = calls_emergency_calls_manager_dispose;
}

static void
calls_emergency_calls_manager_init (CallsEmergencyCallsManager *self)
{
  CallsManager *manager = calls_manager_get_default ();

  self->origins = g_object_ref (calls_manager_get_origins (manager));
  g_signal_connect_object (self->origins,
                           "items-changed",
                           G_CALLBACK (on_origins_changed),
                           self, G_CONNECT_SWAPPED);
}

CallsEmergencyCallsManager *
calls_emergency_calls_manager_new (void)
{
  return g_object_new (CALLS_TYPE_EMERGENCY_CALLS_MANAGER, NULL);
}
