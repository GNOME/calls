/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 */

#pragma once

#include "calls-call.h"

G_BEGIN_DECLS

#define CALLS_TYPE_MOCK_CALL (calls_mock_call_get_type())

G_DECLARE_FINAL_TYPE (CallsMockCall, calls_mock_call, CALLS, MOCK_CALL, CallsCall)

CallsMockCall    *calls_mock_call_new               (void);
void              calls_mock_call_set_id            (CallsMockCall *self,
                                                     const char    *id);

G_END_DECLS
