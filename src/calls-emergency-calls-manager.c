/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "CallsEmergencyCallsManger"

#include "calls-emergency-calls-manager.h"

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

  int dbus_name_id;
} CallsEmergencyCallsManger;

static void calls_emergency_calls_iface_init (CallsDBusEmergencyCallsIface *iface);

G_DEFINE_TYPE_WITH_CODE (CallsEmergencyCallsManager,
                         calls_emergency_calls_manager,
                         CALLS_DBUS_TYPE_EMERGENCY_CALLS_SKELETON,
                         G_IMPLEMENT_INTERFACE (
                           CALLS_DBUS_TYPE_EMERGENCY_CALLS,
                           calls_emergency_calls_iface_init));

static gboolean
handle_call_emergency_contact (CallsDBusEmergencyCalls *object,
                               GDBusMethodInvocation *invocation,
                               const gchar *arg_id)
{
  return FALSE;
}


static gboolean
handle_get_emergency_contacts (CallsDBusEmergencyCalls *object,
                               GDBusMethodInvocation *invocation)
{
  return FALSE;
}


static void
calls_emergency_calls_iface_init (CallsDBusEmergencyCallsIface *iface)
{
  iface->handle_call_emergency_contact = handle_call_emergency_contact;
  iface->handle_get_emergency_contacts = handle_get_emergency_contacts;
}

static void
calls_emergency_calls_manager_class_init (CallsEmergencyCallsManagerClass *klass)
{
}


static void
calls_emergency_calls_manager_init (CallsEmergencyCallsManager *self)
{
}

CallsEmergencyCallsManager *
calls_emergency_calls_manager_new (void)
{
  return g_object_new (CALLS_TYPE_EMERGENCY_CALLS_MANAGER, NULL);
}
