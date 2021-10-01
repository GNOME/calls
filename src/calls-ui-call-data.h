#pragma once

#include "calls-call.h"

#include <cui-call.h>
#include <glib.h>

G_BEGIN_DECLS

#define CALLS_TYPE_UI_CALL_DATA (calls_ui_call_data_get_type ())

G_DECLARE_FINAL_TYPE (CallsUiCallData, calls_ui_call_data, CALLS, UI_CALL_DATA, GObject)

CallsUiCallData         *calls_ui_call_data_new       (CallsCall       *call);
CallsCall               *calls_ui_call_data_get_call  (CallsUiCallData *self);

G_END_DECLS
