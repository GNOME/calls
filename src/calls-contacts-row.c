/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* calls-contacts-row.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *   Julian Sparber <julian@sparber.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <adwaita.h>
#include <folks/folks.h>

#include "calls-contacts-row.h"
#include "calls-contacts-provider.h"

struct _CallsContactsRow {
  GtkListBoxRow    parent_instance;

  GtkWidget       *avatar;
  GtkWidget       *title;
  GtkWidget       *grid;

  gint             n_phonenumbers;

  FolksIndividual *item;
};

G_DEFINE_TYPE (CallsContactsRow, calls_contacts_row, GTK_TYPE_LIST_BOX_ROW)


static void
insert_phonenumber (CallsContactsRow *self,
                    const gchar      *number)
{
  GtkWidget *label = gtk_label_new (number);
  GtkWidget *button = gtk_button_new_from_icon_name ("call-start-symbolic");

  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_widget_add_css_class (label, "dim-label");
  gtk_widget_set_visible (label, TRUE);
  gtk_grid_attach (GTK_GRID (self->grid), label, 1, self->n_phonenumbers, 1, 1);

  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "app.dial");
  gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "s", number, NULL);
  gtk_widget_set_visible (button, TRUE);
  gtk_widget_add_css_class (button, "flat");
  gtk_grid_attach_next_to (GTK_GRID (self->grid),
                           button,
                           label,
                           GTK_POS_RIGHT,
                           1,
                           1);

  self->n_phonenumbers++;
}

static void
phone_numbers_changed_cb (CallsContactsRow *self)
{
  GeeIterator *phone_iter;

  g_autoptr (GeeSet) phone_numbers;

  while (gtk_grid_get_child_at (GTK_GRID (self->grid), 1, 1) != NULL) {
    gtk_grid_remove_row (GTK_GRID (self->grid), 1);
  }

  self->n_phonenumbers = 1;
  g_object_get (self->item, "phone-numbers", &phone_numbers, NULL);
  phone_iter = gee_iterable_iterator (GEE_ITERABLE (phone_numbers));

  while (gee_iterator_next (phone_iter)) {
    g_autoptr (FolksAbstractFieldDetails) detail = gee_iterator_get (phone_iter);

    if (FOLKS_IS_PHONE_FIELD_DETAILS (detail)) {
      FolksPhoneFieldDetails *phone = FOLKS_PHONE_FIELD_DETAILS (detail);
      g_autofree gchar *number = NULL;
      number = folks_phone_field_details_get_normalised (phone);
      if (number)
        insert_phonenumber (self, number);
    }
  }
}

static void
avatar_changed_cb (CallsContactsRow *self)
{
  FolksAvatarDetails *avatar_details;
  GLoadableIcon *loadable_icon;
  g_autoptr (GdkTexture) icon = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (FOLKS_IS_INDIVIDUAL (self->item));

  avatar_details = FOLKS_AVATAR_DETAILS (self->item);
  if (avatar_details == NULL)
    return;

  loadable_icon = folks_avatar_details_get_avatar (avatar_details);
  if (!G_IS_FILE_ICON (loadable_icon)) {
    return;
  }

  icon = gdk_texture_new_from_file (g_file_icon_get_file (G_FILE_ICON (loadable_icon)), &error);
  if (icon == NULL) {
    g_print ("Failed to load avatar icon: %s", error->message);
    return;
  }

  adw_avatar_set_custom_image (ADW_AVATAR (self->avatar), GDK_PAINTABLE (icon));
}

static void
calls_contacts_row_dispose (GObject *object)
{
  CallsContactsRow *self = CALLS_CONTACTS_ROW (object);

  g_clear_object (&self->item);

  G_OBJECT_CLASS (calls_contacts_row_parent_class)->dispose (object);
}

static void
calls_contacts_row_class_init (CallsContactsRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = calls_contacts_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/Calls/"
                                               "ui/contacts-row.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsContactsRow, avatar);
  gtk_widget_class_bind_template_child (widget_class, CallsContactsRow, title);
  gtk_widget_class_bind_template_child (widget_class, CallsContactsRow, grid);
}

static void
calls_contacts_row_init (CallsContactsRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
calls_contacts_row_new (FolksIndividual *item)
{
  CallsContactsRow *self;

  g_return_val_if_fail (FOLKS_IS_INDIVIDUAL (item), NULL);

  self = g_object_new (CALLS_TYPE_CONTACTS_ROW, NULL);
  self->item = g_object_ref (item);

  g_object_bind_property (item, "display-name",
                          self->title, "label",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (item, "display-name",
                          self->avatar, "text",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (item,
                           "notify::phone-numbers",
                           G_CALLBACK (phone_numbers_changed_cb),
                           self, G_CONNECT_SWAPPED);

  g_signal_connect_object (item,
                           "notify::avatar",
                           G_CALLBACK (avatar_changed_cb),
                           self, G_CONNECT_SWAPPED);

  avatar_changed_cb (self);
  phone_numbers_changed_cb (self);

  return GTK_WIDGET (self);
}


FolksIndividual *
calls_contacts_row_get_item (CallsContactsRow *self)
{
  g_return_val_if_fail (CALLS_IS_CONTACTS_ROW (self), NULL);

  return self->item;
}
