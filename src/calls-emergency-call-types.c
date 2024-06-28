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
  char                 *country_code;
  CallsEmergencyNumber  numbers[3];
} CallsEmergencyNumberTypes;

GHashTable *by_mcc;

CallsEmergencyNumberTypes emergency_number_types[] = {
  { "AT",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "BE",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "BG",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "CH",
    {
      { "117", CALLS_EMERGENCY_CALL_TYPE_POLICE },
      { "144", CALLS_EMERGENCY_CALL_TYPE_AMBULANCE },
      { "118", CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE }
    }
  },
  { "CY",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "CZ",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "DE",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "DK",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "EE",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "ES",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "FI",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "FR",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "GR",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "HR",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "HU",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "IE",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "IT",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "LV",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "LT",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "LU",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "MT",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "NL",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "PL",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "PT",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "RO",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "SI",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "SK",
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


static void
init_hash (void)
{
  if (g_once_init_enter (&by_mcc)) {
    GHashTable *table = g_hash_table_new (g_str_hash, g_str_equal);

    for (int i = 0; i < G_N_ELEMENTS (emergency_number_types); i++) {
      CallsEmergencyNumberTypes *numbers = &emergency_number_types[i];

      g_hash_table_insert (table, numbers->country_code, numbers);
    }
    g_once_init_leave (&by_mcc, table);
  }
}


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
  CallsEmergencyNumberTypes *match;

  g_return_val_if_fail (lookup, NULL);
  if (country_code == NULL)
    return NULL;

  init_hash ();

  match = g_hash_table_lookup (by_mcc, country_code);
  if (!match)
    return NULL;

  for (int i = 0; i < G_N_ELEMENTS (match->numbers); i++) {
    CallsEmergencyNumber *number = &match->numbers[i];

    if (g_strcmp0 (lookup, number->number) == 0)
      return flags_to_string (number->flags);
  }

  return NULL;
}

/**
 * calls_emergency_call_types_get_numbers_by_country_code:
 * @mcc: The country code
 *
 * Get the valid emergency numbers for this country code
 *
 * Returns:(transfer full): A string array of emergency numbers
 */
GStrv
calls_emergency_call_types_get_numbers_by_country_code (const char *country_code)
{
  g_autoptr (GPtrArray) ret = g_ptr_array_new_with_free_func (g_free);
  CallsEmergencyNumberTypes *match;

 if (country_code == NULL)
    return NULL;

  init_hash ();

  match = g_hash_table_lookup (by_mcc, country_code);
  if (!match)
    return NULL;

  /* Can use g_strv_builder with glib > 2.68 */
  for (int i = 0; i < G_N_ELEMENTS (match->numbers); i++) {
    char *number = g_strdup (match->numbers[i].number);
    g_ptr_array_add (ret, number);
  }
  g_ptr_array_add (ret, NULL);

  return (GStrv) g_ptr_array_steal (ret, NULL);
}
