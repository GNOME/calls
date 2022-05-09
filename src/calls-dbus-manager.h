/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CALLS_TYPE_DBUS_MANAGER calls_dbus_manager_get_type ()

G_DECLARE_FINAL_TYPE (CallsDBusManager, calls_dbus_manager,
                      CALLS, DBUS_MANAGER, GObject);

CallsDBusManager *calls_dbus_manager_new      (void);
gboolean          calls_dbus_manager_register (CallsDBusManager *self,
                                               GDBusConnection  *connection,
                                               const char       *object_path,
                                               GError          **error);
G_END_DECLS
