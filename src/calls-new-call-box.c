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
 * Author: Adrien Plazas <adrien.plazas@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "calls-new-call-box.h"
#include "calls-enumerate.h"

#include <glib/gi18n.h>
#define HANDY_USE_UNSTABLE_API
#include <handy.h>


struct _CallsNewCallBox
{
  GtkBox parent_instance;

  GtkListStore *origin_store;
  GtkComboBox *origin_box;
  GtkButton *backspace;
  HdyKeypad *keypad;
  GtkButton *dial;
  GtkLabel *status;

  GList *dial_queue;
};

G_DEFINE_TYPE (CallsNewCallBox, calls_new_call_box, GTK_TYPE_BOX);


enum {
  PROP_0,
  PROP_PROVIDER,
  PROP_LAST_PROP,
};
static GParamSpec *props[PROP_LAST_PROP];


enum {
  ORIGIN_STORE_COLUMN_NAME,
  ORIGIN_STORE_COLUMN_ORIGIN
};


static CallsOrigin *
get_origin (CallsNewCallBox *self)
{
  GtkTreeIter iter;
  gboolean ok;
  CallsOrigin *origin;

  ok = gtk_combo_box_get_active_iter (self->origin_box, &iter);
  if (!ok)
    {
      return NULL;
    }

  gtk_tree_model_get (GTK_TREE_MODEL (self->origin_store),
                      &iter,
                      ORIGIN_STORE_COLUMN_ORIGIN, &origin,
                      -1);
  g_assert (CALLS_IS_ORIGIN (origin));

  return origin;
}


static void
backspace_clicked_cb (CallsNewCallBox *self)
{
  GtkWidget *entry = hdy_keypad_get_entry (self->keypad);
  g_signal_emit_by_name (entry, "backspace", NULL);
}


static void
dial_clicked_cb (CallsNewCallBox *self)
{
  GtkWidget *entry = hdy_keypad_get_entry (self->keypad);
  calls_new_call_box_dial
    (self,
     gtk_entry_get_text (GTK_ENTRY (entry)));
}


void
notify_status_cb (CallsNewCallBox *self,
                  GParamSpec      *pspec,
                  CallsProvider   *provider)
{
  gchar *status;

  g_assert (CALLS_IS_PROVIDER (provider));

  status = calls_provider_get_status (provider);
  gtk_label_set_text (self->status, status);
  g_free (status);
}


static void
dial_queued_cb (gchar       *target,
                CallsOrigin *origin)
{
  g_debug ("Dialing queued target `%s'", target);
  calls_origin_dial (origin, target);
}


static void
clear_dial_queue (CallsNewCallBox *self)
{
  g_list_free_full (self->dial_queue, g_free);
  self->dial_queue = NULL;
}


static void
dial_queued (CallsNewCallBox *self)
{
  CallsOrigin *origin;

  if (!self->dial_queue)
    {
      return;
    }

  g_debug ("Dialing %u queued targets",
           g_list_length (self->dial_queue));

  origin = get_origin (self);
  g_assert (origin != NULL);

  g_list_foreach (self->dial_queue,
                  (GFunc)dial_queued_cb,
                  origin);
  g_object_unref (origin);

  clear_dial_queue (self);
}


void
update_origin_box (CallsNewCallBox *self)
{
  GtkTreeModel *origin_store = GTK_TREE_MODEL (self->origin_store);
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_first (origin_store, &iter))
    {
      gtk_widget_hide (GTK_WIDGET (self->origin_box));
      gtk_widget_set_sensitive (GTK_WIDGET (self->dial), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->status), TRUE);
      return;
    }

  /* We know there is at least one origin. */

  gtk_widget_set_sensitive (GTK_WIDGET (self->dial), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->status), FALSE);

  if (!gtk_tree_model_iter_next (origin_store, &iter))
    {
      gtk_combo_box_set_active (self->origin_box, 0);
      gtk_widget_hide (GTK_WIDGET (self->origin_box));
    }
  else
    {
      /* We know there are multiple origins. */

      if (gtk_combo_box_get_active (self->origin_box) < 0)
        {
          gtk_combo_box_set_active (self->origin_box, 0);
        }

      /* We know there are multiple origins and one is selected. */

      gtk_widget_show (GTK_WIDGET (self->origin_box));
    }

  dial_queued (self);
}


static void
add_origin (CallsNewCallBox *self, CallsOrigin *origin)
{
  GtkTreeIter iter;

  gtk_list_store_append (self->origin_store, &iter);
  gtk_list_store_set (self->origin_store, &iter,
                      ORIGIN_STORE_COLUMN_NAME, calls_origin_get_name (origin),
                      ORIGIN_STORE_COLUMN_ORIGIN, G_OBJECT (origin),
                      -1);

  update_origin_box (self);
}


static void
remove_origin (CallsNewCallBox *self, CallsOrigin *origin)
{
  GtkTreeIter iter;
  gboolean ok;

  ok = calls_list_store_find (self->origin_store, origin,
                              ORIGIN_STORE_COLUMN_ORIGIN, &iter);
  g_return_if_fail (ok);

  gtk_list_store_remove (self->origin_store, &iter);

  update_origin_box (self);
}


static void
remove_origins (CallsNewCallBox *self)
{
  GtkTreeModel *model = GTK_TREE_MODEL (self->origin_store);
  GtkTreeIter iter;

  while (gtk_tree_model_get_iter_first (model, &iter))
    {
      gtk_list_store_remove (self->origin_store, &iter);
    }
}


static void
set_provider (CallsNewCallBox *self, CallsProvider *provider)
{
  CallsEnumerateParams *params;

  params = calls_enumerate_params_new (self);

#define add(detail,cb)                          \
  calls_enumerate_params_add                    \
    (params, CALLS_TYPE_PROVIDER, detail,       \
     G_CALLBACK (cb));

  add ("notify::status", notify_status_cb);
  add ("origin-added",   add_origin);
  add ("origin-removed", remove_origin);

#undef add

  calls_enumerate (provider, params);

  g_object_unref (params);
}

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  CallsNewCallBox *self = CALLS_NEW_CALL_BOX (object);

  switch (property_id)
    {
      case PROP_PROVIDER:
        set_provider (self, CALLS_PROVIDER (g_value_get_object (value)));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}


static void
calls_new_call_box_init (CallsNewCallBox *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  update_origin_box (self);
}


static void
constructed (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsNewCallBox *self = CALLS_NEW_CALL_BOX (object);
  GtkWidget *entry = hdy_keypad_get_entry (self->keypad);
  PangoAttrList *attrs;

  // Increase the size of the number entry text
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (1.2));
  gtk_entry_set_attributes (GTK_ENTRY (entry), attrs);
  pango_attr_list_unref (attrs);

  parent_class->constructed (object);
}


static void
dispose (GObject *object)
{
  GObjectClass *parent_class = g_type_class_peek (G_TYPE_OBJECT);
  CallsNewCallBox *self = CALLS_NEW_CALL_BOX (object);

  clear_dial_queue (self);

  if (self->origin_store)
    {
      remove_origins (self);
    }

  parent_class->dispose (object);
}


static void
calls_new_call_box_class_init (CallsNewCallBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->dispose = dispose;


  props[PROP_PROVIDER] =
    g_param_spec_object ("provider",
                         _("Provider"),
                         _("An object implementing low-level call-making functionality"),
                         CALLS_TYPE_PROVIDER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);


  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/new-call-box.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, origin_store);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, origin_box);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, backspace);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, keypad);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, dial);
  gtk_widget_class_bind_template_child (widget_class, CallsNewCallBox, status);
  gtk_widget_class_bind_template_callback (widget_class, dial_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, backspace_clicked_cb);
}


CallsNewCallBox *
calls_new_call_box_new (CallsProvider *provider)
{
  return g_object_new (CALLS_TYPE_NEW_CALL_BOX,
                       "provider", provider,
                       NULL);
}


void
calls_new_call_box_dial (CallsNewCallBox *self,
                         const gchar     *target)
{
  CallsOrigin *origin;

  g_return_if_fail (CALLS_IS_NEW_CALL_BOX (self));
  g_return_if_fail (target != NULL);

  origin = get_origin (self);
  if (!origin)
    {
      // Queue for dialing when an origin appears
      g_debug ("Can't submit call with no origin, queuing for later");
      self->dial_queue = g_list_append (self->dial_queue,
                                        g_strdup (target));
      return;
    }

  calls_origin_dial (origin, target);
  g_object_unref (origin);
}
