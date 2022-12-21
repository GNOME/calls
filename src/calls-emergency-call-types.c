/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "CallsEmergencyCallType"

#include "calls-emergency-call-types.h"

#include <glib/gi18n.h>

/**
 * SECTION:calls-emergency-call-type
 * @short_description: Emergency call types per country
 *
 * Exerpt from https://source.android.com/docs/core/connect/emergency-number-db
 * TODO: parse the actual database for dynamic updates and broader coverage
 */

typedef struct {
  char                         *number;
  CallsEmergencyCallTypeFlags   flags;
} CallsEmergencyNumber;

typedef struct {
  char *country_code;
  CallsEmergencyNumber numbers[3];
} CallsEmergencyNumberTypes;

CallsEmergencyNumberTypes emergency_number_types[] = {
  { "CH",
    {
      { "117", CALLS_EMERGENCY_CALL_TYPE_POLICE },
      { "144", CALLS_EMERGENCY_CALL_TYPE_AMBULANCE },
      { "118", CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE }
    }
  },
  { "DE",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "US",
    {
      { "911", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  }
};


static char *
flags_to_string (CallsEmergencyCallTypeFlags flags)
{
  g_autoptr (GPtrArray) types = g_ptr_array_new ();

  if (flags & CALLS_EMERGENCY_CALL_TYPE_POLICE) {
    g_ptr_array_add (types, _("Police"));
  }
  if (flags & CALLS_EMERGENCY_CALL_TYPE_AMBULANCE) {
    g_ptr_array_add (types, _("Ambulance"));
  }
  if (flags & CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) {
    g_ptr_array_add (types, _("Fire Brigade"));
  }
  if (flags & CALLS_EMERGENCY_CALL_TYPE_MOUNTAIN_RESCUE) {
    g_ptr_array_add (types, _("Mountain Rescue"));
  }

  if (types->len == 0)
    return NULL;

  g_ptr_array_add (types, NULL);
  /* TODO: join in RTL and locale aware way */
  return g_strjoinv (", ", (GStrv)types->pdata);
}


char *
calls_emergency_call_type_get_name (const char *lookup, const char *country_code)
{
  g_return_val_if_fail (lookup, NULL);
  if (country_code == NULL)
    return NULL;

  for (int i = 0; i < G_N_ELEMENTS (emergency_number_types); i++){
    CallsEmergencyNumberTypes *numbers = &emergency_number_types[i];

    if (g_str_equal (numbers->country_code, country_code) == FALSE)
      continue;

    for (int n = 0; n < G_N_ELEMENTS (numbers->numbers); n++) {
      CallsEmergencyNumber *number = &numbers->numbers[n];

      if (g_strcmp0 (lookup, number->number) == 0)
	return flags_to_string (number->flags);
    }
  }

  return NULL;
}
