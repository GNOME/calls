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

#include "config.h"
#include "calls-call-display.h"
#include "calls-call-data.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib.h>

#include <libcallaudio.h>

struct _CallsCallDisplay
{
  GtkOverlay parent_instance;

  CallsCall *call;
  GTimer *timer;
  guint timeout;

  GtkLabel *incoming_phone_call;
  GtkBox *party_box;
  GtkLabel *primary_contact_info;
  GtkLabel *secondary_contact_info;
  GtkLabel *status;

  GtkBox *controls;
  GtkBox *gsm_controls;
  GtkBox *general_controls;
  GtkToggleButton *speaker;
  GtkToggleButton *mute;
  GtkButton *hang_up;
  GtkButton *answer;

  GtkRevealer *dial_pad_revealer;
};

G_DEFINE_TYPE (CallsCallDisplay, calls_call_display, GTK_TYPE_OVERLAY);

enum {
  PROP_0,
  PROP_CALL_DATA,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];

static void
answer_clicked_cb (GtkButton        *button,
                   CallsCallDisplay *self)
{
  g_return_if_fail (CALLS_IS_CALL_DISPLAY (self));

  if (self->call)
    {
      calls_call_answer (self->call);
    }
}

static void
hang_up_clicked_cb (GtkButton        *button,
                    CallsCallDisplay *self)
{
  g_return_if_fail (CALLS_IS_CALL_DISPLAY (self));

  if (self->call)
    {
      calls_call_hang_up (self->call);
    }
}

static void
hold_toggled_cb (GtkToggleButton  *togglebutton,
                 CallsCallDisplay *self)
{
}

static void
mute_toggled_cb (GtkToggleButton  *togglebutton,
                 CallsCallDisplay *self)
{
  gboolean want_mute, ret;
  g_autoptr(GError) error = NULL;

  want_mute = gtk_toggle_button_get_active (togglebutton);
  ret = call_audio_mute_mic (want_mute, &error);
  if (!ret && error)
    {
      g_warning ("Failed to %smute microphone: %s", want_mute ? "" : "un",
                                                    error->message);
    }
}


static void
speaker_toggled_cb (GtkToggleButton  *togglebutton,
                    CallsCallDisplay *self)
{
  gboolean want_speaker, ret;
  g_autoptr(GError) error = NULL;

  want_speaker = gtk_toggle_button_get_active (togglebutton);
  ret = call_audio_enable_speaker (want_speaker, &error);
  if (!ret && error)
    {
      g_warning ("Failed to %sable speaker: %s", want_speaker ? "en" : "dis",
                                                 error->message);
    }
}


static void
add_call_clicked_cb (GtkButton  *button,
                     CallsCallDisplay *self)
{
}


static void
hide_dial_pad_clicked_cb (CallsCallDisplay *self)
{
  gtk_revealer_set_reveal_child (self->dial_pad_revealer, FALSE);
}

static gboolean
timeout_cb (CallsCallDisplay *self)
{
#define MINUTE 60
#define HOUR   (60 * MINUTE)
#define DAY    (24 * HOUR)

  gdouble elapsed;
  GString *str;
  gboolean printing;
  guint minutes;

  g_return_val_if_fail (CALLS_IS_CALL_DISPLAY (self), FALSE);
  if (!self->call)
    {
      return FALSE;
    }

  elapsed = g_timer_elapsed (self->timer, NULL);

  str = g_string_new ("");

  if ( (printing = (elapsed > DAY)) )
    {
      guint days = (guint)(elapsed / DAY);
      g_string_append_printf (str, "%ud ", days);
      elapsed -= (days * DAY);
    }

  if (printing || elapsed > HOUR)
    {
      guint hours = (guint)(elapsed / HOUR);
      g_string_append_printf (str, "%u:", hours);
      elapsed -= (hours * HOUR);
    }

  minutes = (guint)(elapsed / MINUTE);
  g_string_append_printf (str, "%02u:", minutes);
  elapsed -= (minutes * MINUTE);

  g_string_append_printf (str, "%02u", (guint)elapsed);

  gtk_label_set_text (self->status, str->str);

  g_string_free (str, TRUE);
  return TRUE;

#undef DAY
#undef HOUR
#undef MINUTE
}


static void
stop_timeout (CallsCallDisplay *self)
{
  if (self->timeout == 0)
    {
      return;
    }

  g_source_remove (self->timeout);
  self->timeout = 0;
}


static void
select_mode_complete (gboolean success, GError *error)
{
  if (error)
    {
      g_warning ("Failed to select audio mode: %s", error->message);
      g_error_free (error);
    }
}


static void
call_state_changed_cb (CallsCallDisplay *self,
                       CallsCallState    state)
{
  GtkStyleContext *hang_up_style;

  g_return_if_fail (CALLS_IS_CALL_DISPLAY (self));

  hang_up_style = gtk_widget_get_style_context
    (GTK_WIDGET (self->hang_up));

  /* Widgets */
  switch (state)
    {
    case CALLS_CALL_STATE_INCOMING:
      gtk_widget_hide (GTK_WIDGET (self->status));
      gtk_widget_hide (GTK_WIDGET (self->controls));
      gtk_widget_show (GTK_WIDGET (self->incoming_phone_call));
      gtk_widget_show (GTK_WIDGET (self->answer));
      gtk_style_context_remove_class
        (hang_up_style, GTK_STYLE_CLASS_DESTRUCTIVE_ACTION);
      break;

    case CALLS_CALL_STATE_DIALING:
    case CALLS_CALL_STATE_ALERTING:
    case CALLS_CALL_STATE_ACTIVE:
    case CALLS_CALL_STATE_HELD:
    case CALLS_CALL_STATE_WAITING:
      gtk_style_context_add_class
        (hang_up_style, GTK_STYLE_CLASS_DESTRUCTIVE_ACTION);
      gtk_widget_hide (GTK_WIDGET (self->answer));
      gtk_widget_hide (GTK_WIDGET (self->incoming_phone_call));
      gtk_widget_show (GTK_WIDGET (self->controls));
      gtk_widget_show (GTK_WIDGET (self->status));

      gtk_widget_set_visible
        (GTK_WIDGET (self->gsm_controls),
         state != CALLS_CALL_STATE_DIALING
         && state != CALLS_CALL_STATE_ALERTING);

      call_audio_select_mode_async (CALL_AUDIO_MODE_CALL, select_mode_complete);
      break;

    case CALLS_CALL_STATE_DISCONNECTED:
      call_audio_select_mode_async (CALL_AUDIO_MODE_DEFAULT, select_mode_complete);
      break;
    }

  /* Status text */
  switch (state)
    {
    case CALLS_CALL_STATE_INCOMING:
      break;

    case CALLS_CALL_STATE_DIALING:
    case CALLS_CALL_STATE_ALERTING:
      gtk_label_set_text (self->status, _("Calling…"));
      break;

    case CALLS_CALL_STATE_ACTIVE:
    case CALLS_CALL_STATE_HELD:
    case CALLS_CALL_STATE_WAITING:
      if (self->timeout == 0)
        {
          self->timeout = g_timeout_add
            (500, (GSourceFunc)timeout_cb, self);
          timeout_cb (self);
        }
      break;

    case CALLS_CALL_STATE_DISCONNECTED:
      stop_timeout (self);
      break;
    }
}


CallsCallDisplay *
calls_call_display_new (CallsCallData *data)
{
  return g_object_new (CALLS_TYPE_CALL_DISPLAY,
                       "call-data", data,
                       NULL);
}


static void
set_call (CallsCallDisplay *self, CallsCall *call)
{
  g_signal_connect_object (call, "state-changed",
                           G_CALLBACK (call_state_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  self->call = call;
  g_object_ref (G_OBJECT (call));
}


static void
set_party (CallsCallDisplay *self, CallsParty *party)
{
  GtkWidget *image;
  const gchar *name, *number;

  image = calls_party_create_image (party);
  gtk_box_pack_end (self->party_box, image, TRUE, TRUE, 0);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 100);
  gtk_widget_show (image);

  name = calls_party_get_name (party);
  number = calls_party_get_number (party);

  gtk_label_set_text (self->primary_contact_info, name != NULL ? name : number);
  gtk_label_set_text (self->secondary_contact_info, name != NULL ? number : NULL);
}


static void
set_call_data (CallsCallDisplay *self, CallsCallData *data)
{
  set_call (self, calls_call_data_get_call (data));
  set_party (self, calls_call_data_get_party (data));
}


static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsCallDisplay *self = CALLS_CALL_DISPLAY (object);

  switch (property_id) {
  case PROP_CALL_DATA:
    set_call_data (self, CALLS_CALL_DATA (g_value_get_object (value)));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
constructed (GObject *object)
{
  CallsCallDisplay *self = CALLS_CALL_DISPLAY (object);

  self->timer = g_timer_new ();

  call_state_changed_cb (self, calls_call_get_state (self->call));

  G_OBJECT_CLASS (calls_call_display_parent_class)->constructed (object);
}


static void
block_delete_cb (GtkWidget *widget)
{
  g_signal_stop_emission_by_name (widget, "delete-text");
}


static void
insert_text_cb (GtkEditable      *editable,
                gchar            *text,
                gint              length,
                gint             *position,
                CallsCallDisplay *self)
{
  gint end_pos = -1;

  calls_call_tone_start (self->call, *text);

  // Make sure that new chars are inserted at the end of the input
  *position = end_pos;
  g_signal_handlers_block_by_func (editable,
                                   (gpointer) insert_text_cb, self);
  gtk_editable_insert_text (editable, text, length, &end_pos);
  g_signal_handlers_unblock_by_func (editable,
                                     (gpointer) insert_text_cb, self);

  g_signal_stop_emission_by_name (editable, "insert-text");
}


static void
calls_call_display_init (CallsCallDisplay *self)
{
  g_autoptr(GError) err = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  if (!call_audio_is_inited ())
    {
      g_critical ("libcallaudio not initialized: %s", err->message);
      gtk_widget_set_sensitive (GTK_WIDGET (self->speaker), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->mute), FALSE);
    }
}

static void
dispose (GObject *object)
{
  CallsCallDisplay *self = CALLS_CALL_DISPLAY (object);

  stop_timeout (self);
  g_clear_object (&self->call);

  G_OBJECT_CLASS (calls_call_display_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
  CallsCallDisplay *self = CALLS_CALL_DISPLAY (object);

  g_timer_destroy (self->timer);

  G_OBJECT_CLASS (calls_call_display_parent_class)->finalize (object);
}

static void
calls_call_display_class_init (CallsCallDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = constructed;
  object_class->set_property = set_property;
  object_class->dispose = dispose;
  object_class->finalize = finalize;

  props[PROP_CALL_DATA] =
    g_param_spec_object ("call-data",
                         "Call data",
                         "Data for the call this display will be associated with",
                         CALLS_TYPE_CALL_DATA,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/call-display.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, incoming_phone_call);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, party_box);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, primary_contact_info);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, secondary_contact_info);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, status);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, controls);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, gsm_controls);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, general_controls);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, speaker);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, mute);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, hang_up);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, answer);
  gtk_widget_class_bind_template_child (widget_class, CallsCallDisplay, dial_pad_revealer);
  gtk_widget_class_bind_template_callback (widget_class, answer_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, hang_up_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, hold_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, mute_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, speaker_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, add_call_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, hide_dial_pad_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, block_delete_cb);
  gtk_widget_class_bind_template_callback (widget_class, insert_text_cb);
}
