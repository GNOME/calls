/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 */

#include "mock-call.h"

#include <glib/gi18n.h>

enum {
  PROP_0,
  PROP_NAME,
  PROP_ID,
  PROP_STATE,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

struct _CallsMockCall
{
  CallsCall       parent_instance;

  char           *id;
  char           *display_name;
  CallsCallState  state;
};

G_DEFINE_TYPE (CallsMockCall, calls_mock_call, CALLS_TYPE_CALL)


static void
calls_mock_call_answer (CallsCall *call)
{
  g_assert (CALLS_IS_MOCK_CALL (call));
  g_assert_cmpint (calls_call_get_state (call), ==, CALLS_CALL_STATE_INCOMING);

  calls_mock_call_set_state (CALLS_MOCK_CALL (call), CALLS_CALL_STATE_ACTIVE);
}


static void
calls_mock_call_hang_up (CallsCall *call)
{
  g_assert (CALLS_IS_MOCK_CALL (call));
  g_assert_cmpint (calls_call_get_state (call), !=, CALLS_CALL_STATE_DISCONNECTED);

  calls_mock_call_set_state (CALLS_MOCK_CALL (call), CALLS_CALL_STATE_DISCONNECTED);
}


static CallsCallState
calls_mock_call_get_state (CallsCall *call)
{
  g_assert (CALLS_IS_MOCK_CALL (call));

  return CALLS_MOCK_CALL (call)->state;
}

static void
calls_mock_call_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CallsMockCall *self = CALLS_MOCK_CALL (object);

  switch (prop_id) {
  case PROP_ID:
    g_value_set_string (value, self->id);
    break;
  case PROP_NAME:
    g_value_set_string (value, self->display_name);
    break;
  case PROP_STATE:
    g_value_set_enum (value, self->state);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}


static void
calls_mock_call_finalize (GObject *object)
{
  CallsMockCall *self = CALLS_MOCK_CALL (object);

  g_free (self->id);
  g_free (self->display_name);

  G_OBJECT_CLASS (calls_mock_call_parent_class)->finalize (object);
}


static void
calls_mock_call_class_init (CallsMockCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CallsCallClass *call_class = CALLS_CALL_CLASS (klass);

  call_class->answer = calls_mock_call_answer;
  call_class->hang_up = calls_mock_call_hang_up;
  call_class->get_state = calls_mock_call_get_state;

  object_class->get_property = calls_mock_call_get_property;
  object_class->finalize = calls_mock_call_finalize;

  g_object_class_override_property (object_class,
                                    PROP_ID,
                                    "id");
  props[PROP_ID] = g_object_class_find_property (object_class, "id");

  g_object_class_override_property (object_class,
                                    PROP_NAME,
                                    "name");
  props[PROP_NAME] = g_object_class_find_property (object_class, "name");

  g_object_class_override_property (object_class,
                                    PROP_STATE,
                                    "state");
  props[PROP_STATE] = g_object_class_find_property (object_class, "state");

}


static void
calls_mock_call_init (CallsMockCall *self)
{
  self->display_name = g_strdup ("John Doe");
  self->id = g_strdup ("0800 1234");
  self->state = CALLS_CALL_STATE_INCOMING;
}


CallsMockCall *
calls_mock_call_new (void)
{
   return g_object_new (CALLS_TYPE_MOCK_CALL, NULL);
}


void
calls_mock_call_set_id (CallsMockCall *self,
                        const char   *id)
{
  g_return_if_fail (CALLS_IS_MOCK_CALL (self));

  g_free (self->id);
  self->id = g_strdup (id);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ID]);
}


void
calls_mock_call_set_name (CallsMockCall *self,
                          const char    *name)
{
  g_return_if_fail (CALLS_IS_MOCK_CALL (self));

  g_free (self->display_name);
  self->display_name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_NAME]);
}


void
calls_mock_call_set_state (CallsMockCall *self,
                           CallsCallState  state)
{
  CallsCallState old_state;

  g_return_if_fail (CALLS_IS_MOCK_CALL (self));

  old_state = self->state;
  if (old_state == state)
    return;

  self->state = state;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STATE]);
  g_signal_emit_by_name (CALLS_CALL (self),
                         "state-changed",
                         state,
                         old_state);
}

