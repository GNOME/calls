/*
 * Copyright (C) 2018, 2019 Purism SPC
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define STR_IS_NULL_OR_EMPTY(x) ((x) == NULL || (x)[0] == '\0')

/*
 * For defining simple interface functions
 */
#define CALLS_DEFINE_IFACE_FUNC_BASE(prefix,iface,Prefix,Iface,PREFIX,IFACE,function,rettype,errval) \
  rettype                                                               \
  prefix##_##iface##_##function (Prefix##Iface *self)                   \
  {                                                                     \
    Prefix##Iface##Interface *i;                                        \
                                                                        \
    g_return_val_if_fail (PREFIX##_IS_##IFACE (self), errval);          \
                                                                        \
    i = PREFIX##_##IFACE##_GET_IFACE (self);                            \
    g_return_val_if_fail (i-> function != NULL, errval);                \
                                                                        \
    return i-> function (self);                                         \
  }


#define CALLS_DEFINE_IFACE_FUNC_VOID_BASE(prefix,iface,Prefix,Iface,PREFIX,IFACE,function) \
  void                                                                  \
  prefix##_##iface##_##function (Prefix##Iface *self)                   \
  {                                                                     \
    Prefix##Iface##Interface *i;                                        \
                                                                        \
    g_return_if_fail (PREFIX##_IS_##IFACE (self));                      \
                                                                        \
    i = PREFIX##_##IFACE##_GET_IFACE (self);                            \
    g_return_if_fail (i-> function != NULL);                            \
                                                                        \
    i-> function (self);                                                \
  }


#define CALLS_DEFINE_IFACE_FUNC(iface,Iface,IFACE,function,rettype,errval) \
  CALLS_DEFINE_IFACE_FUNC_BASE(calls,iface,Calls,Iface,CALLS,IFACE,function,rettype,errval)


#define CALLS_DEFINE_IFACE_FUNC_VOID(iface,Iface,IFACE,function) \
  CALLS_DEFINE_IFACE_FUNC_VOID_BASE(calls,iface,Calls,Iface,CALLS,IFACE,function)


/*
 * For defining simple getters for properties
 */
#define CALLS_DEFINE_IFACE_GETTER_BASE(prefix,iface,Prefix,Iface,PREFIX,IFACE,prop,rettype,errval) \
   rettype                                                              \
   prefix##_##iface##_get_ ## prop (Prefix##Iface *self)                \
   {                                                                    \
     rettype result;                                                    \
     g_return_val_if_fail (PREFIX##_IS_##IFACE (self), errval);         \
     g_object_get (self, #prop, &result, NULL);                         \
     return result;                                                     \
   }


#define CALLS_DEFINE_IFACE_GETTER(iface,Iface,IFACE,prop,rettype,errval) \
  CALLS_DEFINE_IFACE_GETTER_BASE(calls,iface,Calls,Iface,CALLS,IFACE,prop,rettype,errval)


gboolean    calls_date_time_is_same_day  (GDateTime  *a,
                                           GDateTime *b);
gboolean    calls_date_time_is_yesterday (GDateTime  *now,
                                           GDateTime *t);
gboolean    calls_date_time_is_same_year (GDateTime  *a,
                                           GDateTime *b);
gboolean    calls_number_is_ussd         (const char *number);

gboolean    calls_find_in_model (GListModel *list,
                                  gpointer   item,
                                  guint     *position);

const char *get_protocol_from_address (const char *target);
const char *get_protocol_from_address_with_fallback (const char *target);

gboolean    dtmf_tone_key_is_valid (char key);
const char *get_call_icon_symbolic_name (gboolean  inbound,
                                          gboolean missed);
int         get_address_family_for_ip (const char *ip,
                                        gboolean only_configured_interfaces);

G_END_DECLS
