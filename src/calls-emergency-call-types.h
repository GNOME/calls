/*
 * Copyright (C) 2022 The Phosh Developers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib.h>

# pragma once

/**
 * CallsEmergencyContactSource:
 *
 * Source of the emergency numbers:
 *
 * CALLS_EMERGENCY_CONTACT_SOURCE_UNKNOWN: No idea where it came from
 * CALLS_EMERGENCY_CONTACT_SOURCE_LOCATION: Based on the devices current location
 * CALLS_EMERGENCY_CONTACT_SOURCE_SIM: Based on information on the SIM card
 */
typedef enum {
  CALLS_EMERGENCY_CONTACT_SOURCE_UNKNOWN     = 0,
  CALLS_EMERGENCY_CONTACT_SOURCE_LOCATION    = (1 << 0),
  CALLS_EMERGENCY_CONTACT_SOURCE_SIM         = (1 << 1),
} CallsEmergencyContactSource;

/* See 3GPP TS 22.101 version 14.8.0 Release 14, Chapter 10.1 */
typedef enum {
  CALLS_EMERGENCY_CALL_TYPE_UNKNOWN          = 0,
  CALLS_EMERGENCY_CALL_TYPE_POLICE           = (1 << 0),
  CALLS_EMERGENCY_CALL_TYPE_AMBULANCE        = (1 << 1),
  CALLS_EMERGENCY_CALL_TYPE_FIRE_BRIGADE     = (1 << 2),
  CALLS_EMERGENCY_CALL_TYPE_MOUNTAIN_RESCUE  = (1 << 3),
} CallsEmergencyCallTypeFlags;

char *calls_emergency_call_type_get_name (const char *number, const char *country_code);
GStrv calls_emergency_call_types_get_numbers_by_country_code (const char *country_code);
