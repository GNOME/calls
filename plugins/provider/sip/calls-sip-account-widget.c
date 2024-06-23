/*
 * Copyright (C) 2021 Purism SPC
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
 * Author: Evangelos Ribeiro Tzaras <evangelos.tzaras@puri.sm>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#define G_LOG_DOMAIN "CallsSipAccountWidget"

#include "calls-settings.h"
#include "calls-sip-account-widget.h"
#include "calls-sip-provider.h"
#include "calls-sip-origin.h"
#include "calls-sip-util.h"

#include <glib/gi18n.h>

/**
 * Section:calls-sip-account-widget
 * short_description: A #GtkWidget to edit or add SIP accounts
 * @Title: CallsSipAccountWidget
 *
 * This #GtkWidget allows the user to add a new or edit an existing SIP account.
 */


enum {
  PROP_0,
  PROP_PROVIDER,
  PROP_ORIGIN,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];


struct _CallsSipAccountWidget {
  AdwBin            parent;

  GtkWidget        *child;

  /* Header bar */
  GtkWidget        *header_add;
  GtkSpinner       *spinner_add;
  GtkWidget        *header_edit;
  GtkSpinner       *spinner_edit;
  GtkWidget        *login_btn;
  GtkWidget        *apply_btn;
  GtkWidget        *delete_btn;

  /* widgets for editing account credentials */
  AdwEntryRow      *host;
  AdwEntryRow      *display_name;
  AdwEntryRow      *user;
  AdwEntryRow      *password;
  AdwEntryRow      *port;
  char             *last_port;
  AdwComboRow      *protocol;
  GtkStringList    *protocols_store; /* bound model for protocol AdwComboRow */
  AdwComboRow      *media_encryption;
  GListStore       *media_encryption_store;
  AdwSwitchRow     *tel_switch;
  AdwSwitchRow     *auto_connect_switch;


  /* properties */
  CallsSipProvider *provider;
  CallsSipOrigin   *origin; /* nullable to add a new account */

  /* misc */
  CallsSettings    *settings;
  gboolean          connecting;
  gboolean          port_self_change;
};

G_DEFINE_TYPE (CallsSipAccountWidget, calls_sip_account_widget, ADW_TYPE_BIN)


static gboolean
is_form_valid (CallsSipAccountWidget *self)
{
  g_assert (CALLS_IS_SIP_ACCOUNT_WIDGET (self));

  /* TODO perform some sanity checks */
  return TRUE;
}

static gboolean
is_form_filled (CallsSipAccountWidget *self)
{
  g_assert (CALLS_IS_SIP_ACCOUNT_WIDGET (self));

  return
    g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (self->host)), "") != 0 &&
    g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (self->user)), "") != 0 &&
    g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (self->password)), "") != 0 &&
    g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (self->port)), "") != 0;
}

static const char *
get_selected_protocol (CallsSipAccountWidget *self)
{
  const char *protocol = NULL;
  gint i;

  if ((i = adw_combo_row_get_selected(self->protocol)) != GTK_INVALID_LIST_POSITION) {
    protocol = gtk_string_list_get_string (self->protocols_store, i);
  }
  return protocol;
}


static SipMediaEncryption
get_selected_media_encryption (CallsSipAccountWidget *self)
{
  GObject *obj = NULL;
  SipMediaEncryption media_encryption = SIP_MEDIA_ENCRYPTION_NONE;
  gint i;

  if ((i = adw_combo_row_get_selected(self->media_encryption)) != GTK_INVALID_LIST_POSITION) {
    obj = g_list_model_get_item (G_LIST_MODEL (self->media_encryption_store), i);
    media_encryption = (SipMediaEncryption) GPOINTER_TO_INT (g_object_get_data (G_OBJECT (obj), "value"));
  }


  return media_encryption;
}


static void
update_media_encryption (CallsSipAccountWidget *self)
{
  gboolean transport_is_tls;
  gboolean sdes_always_allowed;

  g_assert (CALLS_IS_SIP_ACCOUNT_WIDGET (self));

  transport_is_tls = g_strcmp0 (get_selected_protocol (self), "TLS") == 0;
  sdes_always_allowed = calls_settings_get_always_allow_sdes (self->settings);

  gtk_widget_set_sensitive (GTK_WIDGET (self->media_encryption),
                            transport_is_tls | sdes_always_allowed);

  if (!transport_is_tls && !sdes_always_allowed)
    adw_combo_row_set_selected (self->media_encryption, 0);
}


static void
on_user_changed (CallsSipAccountWidget *self)
{
  g_assert (CALLS_IS_SIP_ACCOUNT_WIDGET (self));

  gtk_widget_set_sensitive (self->login_btn,
                            is_form_filled (self) &&
                            is_form_valid (self));

  gtk_widget_set_sensitive (self->apply_btn,
                            is_form_filled (self) &&
                            is_form_valid (self));

  update_media_encryption (self);
}


/*
 * Stop "insert-text" signal emission if any undesired port
 * value occurs
 */
static void
on_port_entry_insert_text (CallsSipAccountWidget *self,
                           char                  *new_text,
                           int                    new_text_length,
                           gpointer               position,
                           AdwEntryRow           *entry)
{
  size_t digit_end, len;
  int *pos;

  g_assert (CALLS_IS_SIP_ACCOUNT_WIDGET (self));
  g_assert (ADW_IS_ENTRY_ROW (entry));

  if (!new_text || !*new_text || self->port_self_change)
    return;

  pos = (int *) position;
  g_object_set_data (G_OBJECT (entry), "old-pos", GINT_TO_POINTER (*pos));

  if (new_text_length == -1)
    len = strlen (new_text);
  else
    len = new_text_length;

  digit_end = strspn (new_text, "1234567890");

  /* If user inserted something other than a digit,
   * stop inserting the text and warn the user.
   */
  if (digit_end != len) {
    g_signal_stop_emission_by_name (entry, "insert-text");
    gtk_widget_error_bell (GTK_WIDGET (entry));
  } else {
    g_free (self->last_port);
    self->last_port = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));
  }
}


static gboolean
update_port_cursor_position (AdwEntryRow *entry)
{
  int pos;

  pos = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (entry), "old-pos"));
  gtk_editable_set_position (GTK_EDITABLE (entry), pos);

  return G_SOURCE_REMOVE;
}


static int
get_port (CallsSipAccountWidget *self)
{
  const char *text;
  int port = 0;

  text = gtk_editable_get_text (GTK_EDITABLE (self->port));
  port = (int) g_ascii_strtod (text, NULL);

  return port;
}


static void
on_port_entry_after_insert_text (CallsSipAccountWidget *self,
                                 char                  *new_text,
                                 int                    new_text_length,
                                 gpointer               position,
                                 AdwEntryRow           *entry)
{
  int port = get_port (self);

  /* Reset to the old value if new port number is invalid */
  if ((port < 0 || port > 65535) && self->last_port) {
    self->port_self_change = TRUE;
    gtk_editable_set_text (GTK_EDITABLE (entry), self->last_port);
    g_idle_add (G_SOURCE_FUNC (update_port_cursor_position), entry);
    gtk_widget_error_bell (GTK_WIDGET (entry));
    self->port_self_change = FALSE;
  }
}

static void
update_header (CallsSipAccountWidget *self)
{
  g_assert (CALLS_IS_SIP_ACCOUNT_WIDGET (self));

  if (self->origin) {
    gtk_widget_set_visible (self->header_edit, TRUE);
    gtk_widget_set_visible (self->header_add, FALSE);

  } else {
    gtk_widget_set_visible (self->header_add, TRUE);
    gtk_widget_set_visible (self->header_edit, FALSE);
  }

  if (self->connecting) {
    gtk_spinner_start (self->spinner_add);
    gtk_spinner_start (self->spinner_edit);
  } else {
    gtk_spinner_stop (self->spinner_add);
    gtk_spinner_stop (self->spinner_edit);
  }
}


static gboolean
find_protocol (CallsSipAccountWidget *self,
               const char            *protocol,
               guint                 *index)
{
  guint len;

  g_assert (CALLS_IS_SIP_ACCOUNT_WIDGET (self));

  len = g_list_model_get_n_items (G_LIST_MODEL (self->protocols_store));
  for (guint i = 0; i < len; i++) {
    const char *prot = gtk_string_list_get_string (self->protocols_store, i);

    if (g_strcmp0 (protocol, prot) == 0) {
      if (index)
        *index = i;
      return TRUE;
    }
  }

  g_warning ("Could not find protocol '%s'", protocol);
  return FALSE;
}


static gboolean
find_media_encryption (CallsSipAccountWidget   *self,
                       const SipMediaEncryption encryption,
                       guint                   *index)
{
  guint len;

  g_assert (CALLS_IS_SIP_ACCOUNT_WIDGET (self));

  len = g_list_model_get_n_items (G_LIST_MODEL (self->media_encryption_store));

  for (guint i = 0; i < len; i++) {
    GString* obj =
      g_list_model_get_item (G_LIST_MODEL (self->media_encryption_store), i);
    SipMediaEncryption obj_enc =
      (SipMediaEncryption) GPOINTER_TO_INT (g_object_get_data (G_OBJECT (obj), "value"));

    if (obj_enc == encryption) {
      if (index)
        *index = i;
      return TRUE;
    }
  }

  g_warning ("Could not find encryption mode %d", encryption);
  return FALSE;
}


static void
clear_form (CallsSipAccountWidget *self)
{
  g_assert (CALLS_IS_SIP_ACCOUNT_WIDGET (self));

  gtk_editable_set_text (GTK_EDITABLE (self->host), "");
  gtk_editable_set_text (GTK_EDITABLE (self->display_name), "");
  gtk_editable_set_text (GTK_EDITABLE (self->user), "");
  gtk_editable_set_text (GTK_EDITABLE (self->password), "");
  gtk_editable_set_text (GTK_EDITABLE (self->port), "0");
  adw_combo_row_set_selected (self->protocol, 0);
  gtk_widget_set_sensitive (GTK_WIDGET (self->media_encryption), FALSE);
  adw_combo_row_set_selected (self->media_encryption, 0);
  adw_switch_row_set_active (self->tel_switch, FALSE);
  adw_switch_row_set_active (self->auto_connect_switch, TRUE);

  self->origin = NULL;

  update_header (self);

  if (gtk_widget_get_can_focus (GTK_WIDGET (self->host)))
    gtk_widget_grab_focus (GTK_WIDGET (self->host));
}


static void
edit_form (CallsSipAccountWidget *self,
           CallsSipOrigin        *origin)
{
  g_autofree char *host = NULL;
  g_autofree char *display_name = NULL;
  g_autofree char *user = NULL;
  g_autofree char *password = NULL;
  g_autofree char *port_str = NULL;
  g_autofree char *protocol = NULL;
  gint port;
  SipMediaEncryption encryption;
  guint encryption_index;
  guint protocol_index;
  gboolean can_tel;
  gboolean auto_connect;

  g_assert (CALLS_IS_SIP_ACCOUNT_WIDGET (self));

  if (!origin) {
    clear_form (self);
    return;
  }

  g_assert (CALLS_IS_SIP_ORIGIN (origin));

  self->origin = origin;

  g_object_get (origin,
                "host", &host,
                "display-name", &display_name,
                "user", &user,
                "password", &password,
                "port", &port,
                "transport-protocol", &protocol,
                "media-encryption", &encryption,
                "can-tel", &can_tel,
                "auto-connect", &auto_connect,
                NULL);

  port_str = g_strdup_printf ("%d", port);

  /* The following should always succeed,
     TODO inform user in the error case
     related issue #275 https://source.puri.sm/Librem5/calls/-/issues/275
   */
  if (!find_protocol (self, protocol, &protocol_index))
    protocol_index = 0;

  if (!find_media_encryption (self, encryption, &encryption_index))
    encryption_index = 0;

  /* set UI elements */
  gtk_editable_set_text (GTK_EDITABLE (self->host), host);
  gtk_editable_set_text (GTK_EDITABLE (self->display_name), display_name ?: "");
  gtk_editable_set_text (GTK_EDITABLE (self->user), user);
  gtk_editable_set_text (GTK_EDITABLE (self->password), password);
  gtk_editable_set_text (GTK_EDITABLE (self->port), port_str);
  adw_combo_row_set_selected (self->protocol, protocol_index);
  adw_combo_row_set_selected (self->media_encryption, encryption_index);
  adw_switch_row_set_active (self->tel_switch, can_tel);
  adw_switch_row_set_active (self->auto_connect_switch, auto_connect);

  gtk_widget_set_sensitive (self->apply_btn, FALSE);

  update_header (self);

  if (gtk_widget_get_can_focus (GTK_WIDGET (self->host)))
    gtk_widget_grab_focus (GTK_WIDGET (self->host));
}


static void
on_login_clicked (CallsSipAccountWidget *self)
{
  CallsSipOrigin *origin;
  g_autofree char *id = g_uuid_string_random ();

  g_debug ("Logging into newly created account");

  origin = calls_sip_provider_add_origin (self->provider,
                                          id,
                                          gtk_editable_get_text (GTK_EDITABLE (self->host)),
                                          gtk_editable_get_text (GTK_EDITABLE (self->user)),
                                          gtk_editable_get_text (GTK_EDITABLE (self->password)),
                                          gtk_editable_get_text (GTK_EDITABLE (self->display_name)),
                                          get_selected_protocol (self),
                                          get_port (self),
                                          get_selected_media_encryption (self),
                                          TRUE);

  self->origin = origin;
  update_header (self);
  g_signal_emit_by_name (self->provider, "widget-edit-done");
}


static void
on_delete_clicked (CallsSipAccountWidget *self)
{
  g_debug ("Deleting account");

  calls_sip_provider_remove_origin (self->provider, self->origin);
  self->origin = NULL;

  update_header (self);
  g_signal_emit_by_name (self->provider, "widget-edit-done");
}


static void
on_apply_clicked (CallsSipAccountWidget *self)
{
  g_debug ("Applying changes to the account");

  calls_sip_origin_set_credentials (self->origin,
                                    gtk_editable_get_text (GTK_EDITABLE (self->host)),
                                    gtk_editable_get_text (GTK_EDITABLE (self->user)),
                                    gtk_editable_get_text (GTK_EDITABLE (self->password)),
                                    gtk_editable_get_text (GTK_EDITABLE (self->display_name)),
                                    get_selected_protocol (self),
                                    get_port (self),
                                    get_selected_media_encryption (self),
                                    adw_switch_row_get_active (self->tel_switch),
                                    adw_switch_row_get_active (self->auto_connect_switch));

  update_header (self);
  calls_sip_provider_save_accounts_to_disk (self->provider);
  g_signal_emit_by_name (self->provider, "widget-edit-done");
}


static void
calls_sip_account_widget_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  CallsSipAccountWidget *self = CALLS_SIP_ACCOUNT_WIDGET (object);

  switch (property_id) {
  case PROP_PROVIDER:
    self->provider = g_value_get_object (value);
    break;

  case PROP_ORIGIN:
    calls_sip_account_widget_set_origin (self, g_value_get_object (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
calls_sip_account_widget_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  CallsSipAccountWidget *self = CALLS_SIP_ACCOUNT_WIDGET (object);

  switch (property_id) {
  case PROP_ORIGIN:
    g_value_set_object (value, calls_sip_account_widget_get_origin (self));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
calls_sip_account_widget_dispose (GObject *object)
{
  CallsSipAccountWidget *self = CALLS_SIP_ACCOUNT_WIDGET (object);

  GtkWidget *child = GTK_WIDGET (self->child);

  g_clear_pointer (&child, gtk_widget_unparent);

  g_clear_pointer (&self->last_port, g_free);
  g_clear_object (&self->protocols_store);
  g_clear_object (&self->media_encryption_store);

  G_OBJECT_CLASS (calls_sip_account_widget_parent_class)->dispose (object);
}


static void
calls_sip_account_widget_class_init (CallsSipAccountWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = calls_sip_account_widget_set_property;
  object_class->get_property = calls_sip_account_widget_get_property;
  object_class->dispose = calls_sip_account_widget_dispose;

  props[PROP_PROVIDER] =
    g_param_spec_object ("provider",
                         "Provider",
                         "The SIP provider",
                         CALLS_TYPE_SIP_PROVIDER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  props[PROP_ORIGIN] =
    g_param_spec_object ("origin",
                         "Origin",
                         "The origin to edit",
                         CALLS_TYPE_SIP_ORIGIN,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Calls/ui/sip-account-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, child);

  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, header_add);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, spinner_add);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, header_edit);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, spinner_edit);

  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, login_btn);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, apply_btn);

  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, host);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, display_name);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, user);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, password);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, port);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, protocol);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, media_encryption);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, tel_switch);
  gtk_widget_class_bind_template_child (widget_class, CallsSipAccountWidget, auto_connect_switch);

  gtk_widget_class_bind_template_callback (widget_class, on_login_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_delete_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_apply_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_user_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_port_entry_insert_text);
  gtk_widget_class_bind_template_callback (widget_class, on_port_entry_after_insert_text);
}


static void
calls_sip_account_widget_init (CallsSipAccountWidget *self)
{
  GtkStringObject *obj;

  self->settings = calls_settings_get_default ();

  g_signal_connect_swapped (self->settings,
                            "notify::always-allow-sdes",
                            G_CALLBACK (update_media_encryption),
                            self);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->media_encryption_store = g_list_store_new (GTK_TYPE_STRING_OBJECT);

  obj = gtk_string_object_new (_("No encryption"));
  g_object_set_data (G_OBJECT (obj),
                     "value", GINT_TO_POINTER (SIP_MEDIA_ENCRYPTION_NONE));
  g_list_store_insert (self->media_encryption_store, 0, obj);
  g_clear_object (&obj);

  /* TODO Optional encryption */
  obj = gtk_string_object_new (_("Force encryption"));
  g_object_set_data (G_OBJECT (obj),
                     "value", GINT_TO_POINTER (SIP_MEDIA_ENCRYPTION_FORCED));
  g_list_store_insert (self->media_encryption_store, 1, obj);
  g_clear_object (&obj);

  adw_combo_row_set_model(self->media_encryption, G_LIST_MODEL (self->media_encryption_store));

  self->protocols_store = gtk_string_list_new (NULL);

  gtk_string_list_append (self->protocols_store, "UDP");
  gtk_string_list_append (self->protocols_store, "TCP");
  gtk_string_list_append (self->protocols_store, "TLS");

  adw_combo_row_set_model(self->protocol, G_LIST_MODEL (self->protocols_store));
}


CallsSipAccountWidget *
calls_sip_account_widget_new (CallsSipProvider *provider)
{
  g_return_val_if_fail (CALLS_IS_SIP_PROVIDER (provider), NULL);

  return g_object_new (CALLS_TYPE_SIP_ACCOUNT_WIDGET,
                       "provider", provider,
                       NULL);
}


CallsSipOrigin *
calls_sip_account_widget_get_origin (CallsSipAccountWidget *self)
{
  g_return_val_if_fail (CALLS_IS_SIP_ACCOUNT_WIDGET (self), NULL);

  return self->origin;
}


void
calls_sip_account_widget_set_origin (CallsSipAccountWidget *self,
                                     CallsSipOrigin        *origin)
{
  g_return_if_fail (CALLS_IS_SIP_ACCOUNT_WIDGET (self));
  g_return_if_fail (!origin || CALLS_IS_SIP_ORIGIN (origin));

  edit_form (self, origin);
}
