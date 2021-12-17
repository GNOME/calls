/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#pragma once

#include "dbus/calls-emergency-call-dbus.h"
#include <glib-object.h>

#define CALLS_TYPE_EMERGENCY_CALLS_MANAGER             (calls_emergency_calls_manager_get_type ())

G_DECLARE_FINAL_TYPE (CallsEmergencyCallsManager, calls_emergency_calls_manager, CALLS, EMERGENCY_CALLS_MANAGER,
                      CallsDBusEmergencyCallsSkeleton)

CallsEmergencyCallsManager * calls_emergency_calls_manager_new (void);
