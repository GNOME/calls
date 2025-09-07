/*
 * Copyright (C) 2025 The Phosh.mobi e.V.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

void        calls_service_providers_get_emergency_info (const char          *serviceproviders,
                                                        GCancellable        *cancellable,
                                                        GAsyncReadyCallback  callback,
                                                        gpointer             user_data);
GHashTable *calls_service_providers_get_emergency_info_finish (GAsyncResult *res,
                                                               GError      **error);
GHashTable *calls_service_providers_get_emergency_info_sync (const char *serviceproviders,
                                                             GError    **error) G_GNUC_WARN_UNUSED_RESULT;


G_END_DECLS
