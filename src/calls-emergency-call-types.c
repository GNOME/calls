/*
 * Copyright (C) 2022 The Phosh Developers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#define G_LOG_DOMAIN "CallsEmergencyCallType"

#include "calls-emergency-call-types.h"
#include "calls-service-providers.h"

#include <glib/gi18n.h>

/**
 * SECTION:calls-emergency-call-type
 * @short_description: Emergency call types per country
 *
 * Exerpt from https://source.android.com/docs/core/connect/emergency-number-db
 * TODO: parse the actual database for dynamic updates and broader coverage
 */

typedef struct {
  char                 *country_code;
  CallsEmergencyNumber  numbers[3];
} CallsEmergencyNumberTypes;

static GHashTable *by_mcc;

CallsEmergencyNumberTypes emergency_number_types[] = {
  { "at",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "be",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "bg",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "ch",
    {
      { "117", CALLS_EMERGENCY_CALL_TYPE_POLICE },
      { "144", CALLS_EMERGENCY_CALL_TYPE_AMBULANCE },
      { "118", CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE }
    }
  },
  { "cy",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "cz",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "de",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "dk",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "ee",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "es",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "fi",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "fr",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "gr",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "hr",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "hu",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "ie",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "il",
    {
      { "100", CALLS_EMERGENCY_CALL_TYPE_POLICE },
      { "101", CALLS_EMERGENCY_CALL_TYPE_AMBULANCE },
      { "102", CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE }
    }
  },
  { "it",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "lv",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "lt",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "lu",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "mt",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "nl",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "pl",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "pt",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "ro",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "si",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "sk",
    {
      { "112", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  },
  { "us",
    {
      { "911", (CALLS_EMERGENCY_CALL_TYPE_POLICE |
		CALLS_EMERGENCY_CALL_TYPE_AMBULANCE |
		CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE) }
    }
  }
};


void
calls_emergency_number_free (CallsEmergencyNumber *emergency_number)
{
  g_free (emergency_number->number);
  g_free (emergency_number);
}


CallsEmergencyNumber *
calls_emergency_number_new (const char *number, CallsEmergencyCallTypeFlags flags)
{
  CallsEmergencyNumber *emergency_number;

  emergency_number = g_new0 (CallsEmergencyNumber, 1);
  emergency_number->number = g_strdup (number);
  emergency_number->flags = flags;

  return emergency_number;
}


void
calls_emergency_call_country_data_free (CallsEmergencyCallCountryData *country_data)
{
  g_ptr_array_unref (country_data->numbers);
  g_free (country_data);
}


CallsEmergencyCallCountryData *
calls_emergency_call_country_data_new (const char *country)
{
  CallsEmergencyCallCountryData *country_data;

  g_assert (country);
  g_assert (strlen (country) == 2);

  country_data = g_new0 (CallsEmergencyCallCountryData, 1);
  country_data->numbers = g_ptr_array_new_with_free_func ((GDestroyNotify) calls_emergency_number_free);
  strcpy (country_data->country_code, country);

  return country_data;
}


void
calls_emergency_call_types_init (const char *dbfilename)
{
  if (g_once_init_enter (&by_mcc)) {
    g_autoptr (GError) err = NULL;
    GHashTable *table = NULL;

    table = calls_service_providers_get_emergency_info_sync (dbfilename, &err);
    if (!table) {
      g_warning ("Failed to load emergency number database: '%s'", err->message);
      table = g_hash_table_new_full (g_str_hash,
                                     g_str_equal,
                                     NULL,
                                     (GDestroyNotify) calls_emergency_call_country_data_free);
    }

    for (int i = 0; i < G_N_ELEMENTS (emergency_number_types); i++) {
      CallsEmergencyCallCountryData *country;
      const char *country_code = emergency_number_types[i].country_code;

      if (g_hash_table_lookup (table, country_code))
        continue;

      /* Add a built in fallback */
      country = calls_emergency_call_country_data_new (emergency_number_types[i].country_code);
      for (int k = 0; k < G_N_ELEMENTS (emergency_number_types[i].numbers); k++) {
        CallsEmergencyNumber *number;

        number = calls_emergency_number_new (emergency_number_types[i].numbers[k].number,
                                             emergency_number_types[i].numbers[k].flags);
        g_ptr_array_add (country->numbers, number);
      }

      g_hash_table_insert (table, country->country_code, country);
    }

    g_once_init_leave (&by_mcc, table);
  }
}


void
calls_emergency_call_types_destroy (void)
{
  g_clear_pointer (&by_mcc, g_hash_table_unref);
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
  CallsEmergencyCallCountryData *match;
  g_autofree char *lcode = NULL;

  g_return_val_if_fail (lookup, NULL);
  if (country_code == NULL)
    return NULL;

  g_assert (by_mcc);

  lcode = g_ascii_strdown (country_code, -1);
  match = g_hash_table_lookup (by_mcc, lcode);
  if (!match)
    return NULL;

  for (int i = 0; i < match->numbers->len; i++) {
    CallsEmergencyNumber *number = g_ptr_array_index (match->numbers, i);

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
  CallsEmergencyCallCountryData *match;
  g_autoptr (GStrvBuilder) builder = g_strv_builder_new ();
  g_autofree char *lcode = NULL;

 if (country_code == NULL)
    return NULL;

 g_assert (by_mcc);
 lcode = g_ascii_strdown (country_code, -1);

  match = g_hash_table_lookup (by_mcc, lcode);
  if (!match)
    return NULL;

  for (int i = 0; i < match->numbers->len; i++) {
    CallsEmergencyNumber *number = g_ptr_array_index (match->numbers, i);

    g_strv_builder_add (builder, number->number);
  }

  return g_strv_builder_end (builder);
}
