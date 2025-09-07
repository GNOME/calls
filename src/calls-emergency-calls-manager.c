/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "CallsEmergencyCallsManager"

#include "calls-config.h"

#include "calls-emergency-calls-manager.h"
#include "calls-emergency-call-types.h"
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

typedef struct _CallsEmergencyCallsManager {
  CallsDBusEmergencyCallsSkeleton parent;

  GListModel                     *origins;
} CallsEmergencyCallsManager;

static void calls_emergency_calls_iface_init (CallsDBusEmergencyCallsIface *iface);
G_DEFINE_TYPE_WITH_CODE (CallsEmergencyCallsManager,
                         calls_emergency_calls_manager,
                         CALLS_DBUS_TYPE_EMERGENCY_CALLS_SKELETON,
                         G_IMPLEMENT_INTERFACE (
                           CALLS_DBUS_TYPE_EMERGENCY_CALLS,
                           calls_emergency_calls_iface_init));

static void
on_emergency_numbers_changed (CallsEmergencyCallsManager *self)
{
  g_signal_emit_by_name (self, "emergency-numbers-changed", 0);
}


static void
on_origins_changed (CallsEmergencyCallsManager *self,
                    guint                       position,
                    guint                       removed,
                    guint                       added)
{
  g_assert (CALLS_IS_EMERGENCY_CALLS_MANAGER (self));

  for (int i = 0; i < added; i++) {
    g_autoptr (CallsOrigin) origin = g_list_model_get_item (self->origins, position - i);

    g_signal_connect_object (origin, "notify::emergency-numbers",
                             G_CALLBACK (on_emergency_numbers_changed),
                             self,
                             G_CONNECT_SWAPPED);
  }

  g_signal_emit_by_name (self, "emergency-numbers-changed", 0);
}


static gboolean
handle_call_emergency_contact (CallsDBusEmergencyCalls *object,
                               GDBusMethodInvocation   *invocation,
                               const gchar             *arg_id)
{
  CallsEmergencyCallsManager *self = CALLS_EMERGENCY_CALLS_MANAGER (object);
  g_debug ("Looking for emergency number %s", arg_id);

  g_return_val_if_fail (CALLS_IS_EMERGENCY_CALLS_MANAGER (self), FALSE);

  /* Pick the first origin that supports the given emergency number */
  for (int i = 0; i < g_list_model_get_n_items (self->origins); i++) {
    g_autoptr (CallsOrigin) origin = g_list_model_get_item (self->origins, i);
    g_auto (GStrv) emergency_numbers = NULL;

    emergency_numbers = calls_origin_get_emergency_numbers (origin);
    if (!emergency_numbers)
      continue;

    for (int j = 0; j < g_strv_length (emergency_numbers); j++) {
      if (g_strcmp0 (arg_id, emergency_numbers[j]) == 0) {
        g_debug ("Dialing %s via %s", arg_id, calls_origin_get_name (origin));
        calls_origin_dial (origin, arg_id);
        calls_dbus_emergency_calls_complete_call_emergency_contact (object, invocation);
        goto done;
      }
    }
  }

  g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                         G_DBUS_ERROR_NOT_SUPPORTED,
                                         "%s not a known emergency number", arg_id);
 done:
  return TRUE;
}


#define CONTACT_FORMAT "(ssia{sv})"
#define CONTACTS_FORMAT "a" CONTACT_FORMAT


static void
add_emergency_contact (GVariantBuilder             *contacts_builder,
                       const char                  *number,
                       const char                  *contact,
                       CallsEmergencyContactSource  type)

{
  g_variant_builder_open (contacts_builder, G_VARIANT_TYPE (CONTACT_FORMAT));
  g_variant_builder_add (contacts_builder, "s", number);
  g_variant_builder_add (contacts_builder, "s", contact);
  g_variant_builder_add (contacts_builder, "i", type);
  /* Currently no hints */
  g_variant_builder_add (contacts_builder, "a{sv}", NULL);
  g_variant_builder_close (contacts_builder);
}


static gboolean
handle_get_emergency_contacts (CallsDBusEmergencyCalls *object,
                               GDBusMethodInvocation   *invocation)
{
  GVariant *contacts;
  GVariantBuilder contacts_builder;
  CallsEmergencyCallsManager *self = CALLS_EMERGENCY_CALLS_MANAGER (object);
  g_autoptr (GHashTable) numbers = g_hash_table_new_full (g_str_hash,
                                                          g_str_equal,
                                                          g_free,
                                                          NULL);

  g_variant_builder_init (&contacts_builder, G_VARIANT_TYPE (CONTACTS_FORMAT));

  for (int i = 0; i < g_list_model_get_n_items (self->origins); i++) {
    g_autoptr (CallsOrigin) origin = g_list_model_get_item (self->origins, i);
    g_auto (GStrv) emergency_numbers = NULL;
    const char *country_code;

    /* Emergency numbers by location */
    country_code = calls_origin_get_network_country_code (origin);
    emergency_numbers = calls_emergency_call_types_get_numbers_by_country_code (country_code);
    for (int j = 0; emergency_numbers && j < g_strv_length (emergency_numbers); j++) {
      g_autofree char *contact = NULL;

      if (g_hash_table_contains (numbers, emergency_numbers[j]))
          continue;

      contact = calls_emergency_call_type_get_name (emergency_numbers[j], country_code);
      if (contact == NULL)
        contact = g_strdup (emergency_numbers[j]);

      add_emergency_contact (&contacts_builder,
                             emergency_numbers[j],
                             contact,
                             CALLS_EMERGENCY_CONTACT_SOURCE_LOCATION);
      g_hash_table_insert (numbers, g_strdup (emergency_numbers[j]), NULL);
    }
    g_clear_pointer (&emergency_numbers, g_strfreev);

    /* Emergency numbers by origin */
    country_code = calls_origin_get_country_code (origin);
    emergency_numbers = calls_origin_get_emergency_numbers (origin);
    for (int j = 0; emergency_numbers && j < g_strv_length (emergency_numbers); j++) {
      g_autofree char *contact = NULL;

      if (g_hash_table_contains (numbers, emergency_numbers[j]))
          continue;

      contact = calls_emergency_call_type_get_name (emergency_numbers[j], country_code);
      if (contact == NULL)
        contact = g_strdup (emergency_numbers[j]);

      add_emergency_contact (&contacts_builder,
                             emergency_numbers[j],
                             contact,
                             /* TODO: allow to query type */
                             CALLS_EMERGENCY_CONTACT_SOURCE_UNKNOWN);
      g_hash_table_insert (numbers, g_strdup (emergency_numbers[j]), NULL);
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
calls_emergency_calls_manager_finalize (GObject *object)
{
  calls_emergency_call_types_destroy ();

  G_OBJECT_CLASS (calls_emergency_calls_manager_parent_class)->finalize (object);
}


static void
calls_emergency_calls_manager_class_init (CallsEmergencyCallsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = calls_emergency_calls_manager_dispose;
  object_class->finalize = calls_emergency_calls_manager_finalize;
}

static void
calls_emergency_calls_manager_init (CallsEmergencyCallsManager *self)
{
  CallsManager *manager = calls_manager_get_default ();

  calls_emergency_call_types_init (CALLS_EMERGENCY_INFO_DATABASE);

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
