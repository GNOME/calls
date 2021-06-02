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
 * Authors:
 *     Bob Ham <bob.ham@puri.sm>
 *     Adrien Plazas <adrien.plazas@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "calls-call-window.h"
#include "calls-origin.h"
#include "calls-call-selector-item.h"
#include "calls-new-call-box.h"
#include "calls-in-app-notification.h"
#include "calls-wayland-config.h"
#include "calls-manager.h"
#include "util.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <handy.h>

#ifdef CALLS_WAYLAND
#include <gdk/gdkwayland.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wayland/layersurface.h"
#endif // CALLS_WAYLAND

struct _CallsCallWindow
{
  GtkApplicationWindow parent_instance;

  GListStore *calls;

  CallsInAppNotification *in_app_notification;

  GtkStack *main_stack;
  GtkStack *header_bar_stack;
  GtkButton *show_calls;
  GtkStack *call_stack;
  GtkFlowBox *call_selector;

  guint inhibit_cookie;

#ifdef CALLS_WAYLAND
  gboolean screensaver_active;
  struct zwlr_layer_shell_v1 *layer_shell_iface;
  struct wl_output *output;
  GtkWindow *layer_surface;
#endif // CALLS_WAYLAND
};

G_DEFINE_TYPE (CallsCallWindow, calls_call_window, GTK_TYPE_APPLICATION_WINDOW);


#ifdef CALLS_WAYLAND
static void
tear_down_layer_surface (CallsCallWindow *self)
{
  GtkWidget *child;

  if (!self->layer_surface)
    {
      return;
    }

  g_debug ("Tearing down layer surface");

  // Remove window content from layer surface
  child = gtk_bin_get_child (GTK_BIN (self->layer_surface));
  g_object_ref (child);

  gtk_container_remove (GTK_CONTAINER (self->layer_surface),
                        child);

  // Add to the call window
  gtk_container_add (GTK_CONTAINER (self), child);
  g_object_unref (child);

  // Destroy layer surface
  gtk_widget_destroy (GTK_WIDGET (self->layer_surface));
  self->layer_surface = NULL;
}


static void
set_up_layer_surface (CallsCallWindow *self)
{
  GtkWidget *child;

  if (self->layer_surface
      || !self->layer_shell_iface
      || !self->output)
    {
      return;
    }

  g_debug ("Setting up layer surface");

  // Create layer surface
  self->layer_surface = g_object_new
    (PHOSH_TYPE_LAYER_SURFACE,
     "layer-shell", self->layer_shell_iface,
     "wl-output", self->output,
     "layer", ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
     "namespace", "calls",
     "anchor", ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
     | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
     | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
     | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
     NULL);
  g_assert (self->layer_surface);

  // Remove window content from call window
  child = gtk_bin_get_child (GTK_BIN (self));
  g_object_ref (child);

  gtk_container_remove (GTK_CONTAINER (self), child);

  // Add to the layer surface
  gtk_container_add (GTK_CONTAINER (self->layer_surface),
                     child);
  g_object_unref (child);

  // Show the layer surface
  gtk_window_set_keep_above (self->layer_surface, TRUE);
  gtk_widget_show (GTK_WIDGET (self->layer_surface));
}


static void
update_layer_surface (CallsCallWindow *self,
                      guint calls)
{
  g_debug ("Updating layer surface");

  if (calls == 0)
    {
      tear_down_layer_surface (self);
    }
  else if (self->screensaver_active)
    {
      set_up_layer_surface (self);
    }
}
#endif // CALLS_WAYLAND


static void
session_inhibit (CallsCallWindow *self, gboolean inhibit)
{
  if (inhibit)
    {
      if (self->inhibit_cookie == 0)
        self->inhibit_cookie =
          gtk_application_inhibit (gtk_window_get_application (GTK_WINDOW (self)),
                                   GTK_WINDOW (self),
                                   GTK_APPLICATION_INHIBIT_SUSPEND |
                                   GTK_APPLICATION_INHIBIT_IDLE |
                                   GTK_APPLICATION_INHIBIT_LOGOUT |
                                   GTK_APPLICATION_INHIBIT_SWITCH,
                                   "call active");
    }
  else
    {
      if (self->inhibit_cookie != 0)
        gtk_application_uninhibit (gtk_window_get_application (GTK_WINDOW (self)),
                                   self->inhibit_cookie);
      self->inhibit_cookie = 0;
    }

}


static void
update_visibility (CallsCallWindow *self)
{
  guint calls = g_list_model_get_n_items (G_LIST_MODEL (self->calls));

#ifdef CALLS_WAYLAND
  update_layer_surface (self, calls);
#endif // CALLS_WAYLAND

  gtk_widget_set_visible (GTK_WIDGET (self), calls > 0);
  gtk_widget_set_sensitive (GTK_WIDGET (self->show_calls), calls > 1);

  if (calls == 0)
    {
      gtk_stack_set_visible_child_name (self->main_stack, "calls");
    }
  else if (calls == 1)
    {
      gtk_stack_set_visible_child_name (self->main_stack, "active-call");
    }

  session_inhibit (self, !!calls);
}


static GtkWidget *
calls_create_widget_cb (CallsCallSelectorItem *item,
                        gpointer               user_data)
{
  return GTK_WIDGET (item);
}


static void
new_call_submitted_cb (CallsCallWindow *self,
                       CallsOrigin     *origin,
                       const gchar     *number,
                       CallsNewCallBox *new_call_box)
{
  g_return_if_fail (CALLS_IS_CALL_WINDOW (self));

  calls_origin_dial (origin, number);
}


static void
set_focus (CallsCallWindow  *self,
           CallsCallDisplay *display)
{
  gtk_stack_set_visible_child_name (self->main_stack, "active-call");
  gtk_stack_set_visible_child_name (self->header_bar_stack, "active-call");
  gtk_stack_set_visible_child (self->call_stack, GTK_WIDGET (display));
}


static void
show_calls_clicked_cb (GtkButton       *button,
                       CallsCallWindow *self)
{
  /* FIXME Setting only one of them should be enough as the properties are binded. */
  gtk_stack_set_visible_child_name (self->main_stack, "calls");
  gtk_stack_set_visible_child_name (self->header_bar_stack, "calls");
}


static void
call_selector_child_activated_cb (GtkFlowBox      *box,
                                  GtkFlowBoxChild *child,
                                  CallsCallWindow *self)
{
  GtkWidget *widget = gtk_bin_get_child (GTK_BIN (child));
  CallsCallSelectorItem *item = CALLS_CALL_SELECTOR_ITEM (widget);
  CallsCallDisplay *display = calls_call_selector_item_get_display (item);

  set_focus (self, display);
}


static void
add_call (CallsCallWindow *self,
          CallsCall       *call)
{
  CallsCallDisplay *display;
  CallsCallSelectorItem *item;

  g_return_if_fail (CALLS_IS_CALL_WINDOW (self));
  g_return_if_fail (CALLS_IS_CALL (call));

  display = calls_call_display_new (call);
  item = calls_call_selector_item_new (display);
  gtk_stack_add_named (self->call_stack, GTK_WIDGET (display),
                       calls_call_get_number (call));

  g_list_store_append (self->calls, item);

  update_visibility (self);
  set_focus (self, display);
}

static void
remove_call (CallsCallWindow *self,
             CallsCall       *call,
             const gchar     *reason)
{
  g_autoptr (CallsCallSelectorItem) item = NULL;
  gint position;

  g_return_if_fail (CALLS_IS_CALL_WINDOW (self));
  g_return_if_fail (CALLS_IS_CALL (call));

  position = 0;
  item = g_list_model_get_item (G_LIST_MODEL (self->calls), position);
  while (item != NULL) {
    CallsCallDisplay *display = calls_call_selector_item_get_display (item);

    if (calls_call_display_get_call (display) == call) {
      g_list_store_remove (self->calls, position);
      gtk_container_remove (GTK_CONTAINER (self->call_stack),
                            GTK_WIDGET (display));


      break;
    }

    g_object_unref (item);
    position++;
    item = g_list_model_get_item (G_LIST_MODEL (self->calls), position);
  }

  update_visibility (self);
}


static void
remove_calls (CallsCallWindow *self)
{
  GList *children, *child;

  /* Safely remove the call stack's children. */
  children = gtk_container_get_children (GTK_CONTAINER (self->call_stack));
  for (child = children; child != NULL; child = child->next)
    gtk_container_remove (GTK_CONTAINER (self->call_stack),
                          GTK_WIDGET (child->data));
  g_list_free (children);

  g_list_store_remove_all (self->calls);

  update_visibility (self);
}

#ifdef CALLS_WAYLAND
static void
get_screensaver_active (CallsCallWindow *self,
                        GtkApplication  *app)
{
  g_object_get (app,
                "screensaver-active", &self->screensaver_active,
                NULL);
}


static void
notify_screensaver_active_cb (CallsCallWindow *self,
                              GParamSpec      *pspec,
                              GtkApplication  *app)
{
  get_screensaver_active (self, app);
  g_debug ("Screensaver became %sactive",
           self->screensaver_active ? "" : "in");

  update_visibility (self);
}


static gboolean
set_up_screensaver (CallsCallWindow *self)
{
  GtkApplication *app;
  gboolean reg;

  app = gtk_window_get_application (GTK_WINDOW (self));
  g_assert (app != NULL);

  g_object_get (app,
                "register-session", &reg,
                NULL);
  if (!reg)
    {
      g_warning ("Cannot monitor screensaver state because"
                 " Application is not set to register with session");
      return FALSE;
    }

  g_signal_connect_swapped (app,
                            "notify::screensaver-active",
                            G_CALLBACK (notify_screensaver_active_cb),
                            self);

  get_screensaver_active (self, app);
  g_debug ("Screensaver is %sactive at startup",
           self->screensaver_active ? "" : "in");

  return TRUE;
}


static void
registry_global_cb (void               *data,
                    struct wl_registry *registry,
                    uint32_t            name,
                    const char         *interface,
                    uint32_t            version)
{
  CallsCallWindow *self = CALLS_CALL_WINDOW (data);

  if (strcmp (interface, zwlr_layer_shell_v1_interface.name) == 0)
    {
      self->layer_shell_iface = wl_registry_bind
        (registry, name, &zwlr_layer_shell_v1_interface, version);
    }
  else if (strcmp (interface, "wl_output") == 0)
    {
      // FIXME: Deal with more than one output
      if (!self->output)
        {
          self->output = wl_registry_bind
            (registry, name, &wl_output_interface, version);
        }
    }
}

static const struct wl_registry_listener registry_listener =
{
  registry_global_cb,
  // FIXME: Add a remove callback
};


static void
set_up_wayland (CallsCallWindow *self)
{
  GdkDisplay *gdk_display;
  struct wl_display *display;
  struct wl_registry *registry;

  gdk_display = gdk_display_get_default ();
  g_assert (gdk_display != NULL);

  if (!GDK_IS_WAYLAND_DISPLAY (gdk_display))
    {
      g_debug ("GDK display is not a Wayland display");
      return;
    }

  display = gdk_wayland_display_get_wl_display (gdk_display);
  g_assert (display != NULL);

  registry = wl_display_get_registry (display);
  g_assert (registry != NULL);

  wl_registry_add_listener (registry, &registry_listener, self);
  wl_display_roundtrip (display);


  if (!self->layer_shell_iface)
    {
      g_warning ("Wayland display has no layer shell interface");
    }
  if (!self->output)
    {
      g_warning ("Wayland display has no output");
    }
}


static void
application_cb (CallsCallWindow *self)
{
  gboolean ok;

  ok = set_up_screensaver (self);

  if (ok)
    {
      set_up_wayland (self);
    }
}


static void
notify (GObject    *object,
        GParamSpec *pspec)
{
  CallsCallWindow *self = CALLS_CALL_WINDOW (object);
  const gchar *name;

  name = g_param_spec_get_name (pspec);
  if (strcmp (name, "application") == 0)
    {
      application_cb (self);
    }

  return;
}
#endif // CALLS_WAYLAND


static void
constructed (GObject *object)
{
  CallsCallWindow *self = CALLS_CALL_WINDOW (object);

  gtk_flow_box_bind_model (self->call_selector,
                           G_LIST_MODEL (self->calls),
                           (GtkFlowBoxCreateWidgetFunc) calls_create_widget_cb,
                           NULL, NULL);

  update_visibility (self);

  G_OBJECT_CLASS (calls_call_window_parent_class)->constructed (object);
}


static void
calls_call_window_init (CallsCallWindow *self)
{
  g_autoptr (GList) calls = NULL;
  GList *c;
  gtk_widget_init_template (GTK_WIDGET (self));

  self->calls = g_list_store_new (CALLS_TYPE_CALL_SELECTOR_ITEM);

  // Show errors in in-app-notification
  g_signal_connect_swapped (calls_manager_get_default (),
                            "error",
                            G_CALLBACK (calls_in_app_notification_show),
                            self->in_app_notification);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "call-add",
                            G_CALLBACK (add_call),
                            self);

  g_signal_connect_swapped (calls_manager_get_default (),
                            "call-remove",
                            G_CALLBACK (remove_call),
                            self);

  calls = calls_manager_get_calls (calls_manager_get_default ());
  for (c = calls; c != NULL; c = c->next) {
    add_call (self, c->data);
  }
}


static void
dispose (GObject *object)
{
  CallsCallWindow *self = CALLS_CALL_WINDOW (object);

  if (self->calls)
    {
      remove_calls (self);
    }

  g_clear_object (&self->calls);

  G_OBJECT_CLASS (calls_call_window_parent_class)->dispose (object);
}


static void
calls_call_window_class_init (CallsCallWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = constructed;
  object_class->dispose = dispose;

#ifdef CALLS_WAYLAND
  // The "application" property is not a construction property so we
  // have to wait for it to be set before setting up wayland & co.
  object_class->notify = notify;
#endif // CALLS_WAYLAND

  gtk_widget_class_set_template_from_resource (widget_class, "/sm/puri/calls/ui/call-window.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, in_app_notification);
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, header_bar_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, show_calls);
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, call_stack);
  gtk_widget_class_bind_template_child (widget_class, CallsCallWindow, call_selector);
  gtk_widget_class_bind_template_callback (widget_class, call_selector_child_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_calls_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, new_call_submitted_cb);
}


CallsCallWindow *
calls_call_window_new (GtkApplication *application)
{
  return g_object_new (CALLS_TYPE_CALL_WINDOW,
                       "application", application,
                       NULL);
}
