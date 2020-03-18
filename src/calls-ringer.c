/*
 * Copyright (C) 2018 Purism SPC
 *
 * This file is part of Calls.
 *
 * Calls is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Calls is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Calls.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Bob Ham <bob.ham@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "calls-ringer.h"
#include "calls-manager.h"
#include "config.h"

#include <gsound.h>
#include <glib/gi18n.h>
#include <glib-object.h>


struct _CallsRinger
{
  GObject parent_instance;

  gchar         *theme_name;
  GSoundContext *ctx;
  unsigned       ring_count;
  GCancellable  *playing;
};

G_DEFINE_TYPE (CallsRinger, calls_ringer, G_TYPE_OBJECT);


static void
ringer_error (CallsRinger *self,
              const gchar *prefix,
              GError      *error)
{
  g_warning ("%s: %s", prefix, error->message);
  g_error_free (error);

  g_clear_object (&self->ctx);
}


void
get_theme_name (CallsRinger *self,
                GtkSettings *settings)
{
  gchar *theme_name = NULL;

  g_object_get (settings,
                "gtk-sound-theme-name", &theme_name,
                NULL);

  g_free (self->theme_name);
  self->theme_name = theme_name;

  g_debug ("Got GTK sound theme name `%s'", theme_name);
}


void
notify_sound_theme_name_cb (GtkSettings   *settings,
                            GParamSpec  *pspec,
                            CallsRinger *self)
{
  get_theme_name (self, settings);

  if (self->ctx)
    {
      GError *error = NULL;
      gboolean ok;

      ok = gsound_context_set_attributes
        (self->ctx,
         &error,
         GSOUND_ATTR_CANBERRA_XDG_THEME_NAME, self->theme_name,
         NULL);

      if (!ok)
        {
          g_warning ("Could not set GSound theme name: %s",
                     error->message);
          g_error_free (error);
        }
    }
}


static void
monitor_theme_name (CallsRinger *self)
{
  GtkSettings *settings = gtk_settings_get_default ();
  g_assert (settings != NULL);

  g_signal_connect (settings,
                    "notify::gtk-sound-theme-name",
                    G_CALLBACK (notify_sound_theme_name_cb),
                    self);

  get_theme_name (self, settings);
}


static gboolean
create_ctx (CallsRinger *self)
{
  GError *error = NULL;
  gboolean ok;
  GHashTable *attrs;

  self->ctx = gsound_context_new (NULL, &error);
  if (!self->ctx)
    {
      ringer_error (self, "Error creating GSound context", error);
      return FALSE;
    }

  attrs = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert
    (attrs, GSOUND_ATTR_APPLICATION_ICON_NAME, APP_ID);
  if (self->theme_name)
    {
      g_hash_table_insert
        (attrs, GSOUND_ATTR_CANBERRA_XDG_THEME_NAME,
         self->theme_name);
    }

  ok = gsound_context_set_attributesv (self->ctx, attrs, &error);
  g_hash_table_unref (attrs);
  if (!ok)
    {
      ringer_error (self, "Error setting GSound attributes", error);
      return FALSE;
    }

  g_debug ("Created ringtone context");

  return TRUE;
}


static void play (CallsRinger *self);


static void
play_cb (GSoundContext *ctx,
         GAsyncResult  *res,
         CallsRinger   *self)
{
  gboolean ok;
  GError *error = NULL;

  ok = gsound_context_play_full_finish (ctx, res, &error);
  if (!ok)
    {
      g_clear_object (&self->playing);

      if (error->domain == G_IO_ERROR
          && error->code == G_IO_ERROR_CANCELLED)
        {
          g_debug ("Ringtone cancelled");
        }
      else
        {
          ringer_error (self, "Error playing ringtone", error);
        }

      return;
    }

  g_assert (self->ring_count > 0);
  play (self);
}


static void
play (CallsRinger *self)
{
  g_assert (self->ctx     != NULL);
  g_assert (self->playing != NULL);

  g_debug ("Playing ringtone");
  gsound_context_play_full (self->ctx,
                            self->playing,
                            (GAsyncReadyCallback)play_cb,
                            self,
                            GSOUND_ATTR_MEDIA_ROLE, "event",
                            GSOUND_ATTR_EVENT_ID, "phone-incoming-call",
                            GSOUND_ATTR_EVENT_DESCRIPTION, _("Incoming call"),
                            NULL);
}


static void
start (CallsRinger *self)
{
  g_assert (self->playing == NULL);

  if (!self->ctx)
    {
      gboolean ok;

      ok = create_ctx (self);
      if (!ok)
        {
          return;
        }
    }

  g_debug ("Starting ringtone");
  self->playing = g_cancellable_new ();
  play (self);
}


static void
stop (CallsRinger *self)
{
  g_debug ("Stopping ringtone");

  g_assert (self->ctx != NULL);

  g_cancellable_cancel (self->playing);
}


static void
update_ring (CallsRinger *self)
{
  if (!self->playing)
    {
      if (self->ring_count > 0)
        {
          g_debug ("Starting ringer");
          start (self);
        }
    }
  else
    {
      if (self->ring_count == 0)
        {
          g_debug ("Stopping ringer");
          stop (self);
        }
    }
}


static inline gboolean
is_ring_state (CallsCallState state)
{
  switch (state)
    {
    case CALLS_CALL_STATE_INCOMING:
    case CALLS_CALL_STATE_WAITING:
      return TRUE;
    default:
      return FALSE;
    }
}


static void
state_changed_cb (CallsRinger   *self,
                  CallsCallState new_state,
                  CallsCallState old_state)
{
  gboolean old_is_ring;

  g_return_if_fail (old_state != new_state);

  old_is_ring = is_ring_state (old_state);
  if (old_is_ring == is_ring_state (new_state))
    {
      // No change in ring state
      return;
    }

  if (old_is_ring)
    {
      --self->ring_count;
    }
  else
    {
      ++self->ring_count;
    }

  update_ring (self);
}


static void
update_count (CallsRinger    *self,
              CallsCall      *call,
              short           delta)
{
  if (is_ring_state (calls_call_get_state (call)))
    {
      self->ring_count += delta;
    }

  update_ring (self);
}


static void
call_added_cb (CallsRinger *self, CallsCall *call)
{
  update_count (self, call, +1);

  g_signal_connect_swapped (call,
                            "state-changed",
                            G_CALLBACK (state_changed_cb),
                            self);
}


static void
call_removed_cb (CallsRinger *self, CallsCall *call)
{
  update_count (self, call, -1);

  g_signal_handlers_disconnect_by_data (call, self);
}


static void
calls_ringer_init (CallsRinger *self)
{
}


static void
constructed (GObject *object)
{
  g_autoptr (GList) calls = NULL;
  GList *c;
  CallsRinger *self = CALLS_RINGER (object);

  monitor_theme_name (self);
  create_ctx (self);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "call-add",
                            G_CALLBACK (call_added_cb),
                            self);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "call-remove",
                            G_CALLBACK (call_removed_cb),
                            self);

  calls = calls_manager_get_calls (calls_manager_get_default ());
  for (c = calls; c != NULL; c = c->next) {
    call_added_cb (self, c->data);
  }

  G_OBJECT_CLASS (calls_ringer_parent_class)->constructed (object);
}


static void
finalize (GObject *object)
{
  CallsRinger *self = CALLS_RINGER (object);

  g_free (self->theme_name);

  G_OBJECT_CLASS (calls_ringer_parent_class)->finalize (object);
}


static void
calls_ringer_class_init (CallsRingerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = constructed;
  object_class->finalize = finalize;
}

CallsRinger *
calls_ringer_new (void)
{
  return g_object_new (CALLS_TYPE_RINGER, NULL);
}
