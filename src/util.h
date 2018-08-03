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

#ifndef CALLS__UTIL_H__
#define CALLS__UTIL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

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



#define CALLS_SET_OBJECT_PROPERTY(obj_ptr,new_value)    \
  if (obj_ptr)                                          \
    {                                                   \
      g_object_unref (G_OBJECT (obj_ptr));              \
    }                                                   \
  obj_ptr = new_value;                                  \
  g_object_ref (G_OBJECT (obj_ptr));
  

#define CALLS_SET_PTR_PROPERTY(ptr,new_value)   \
  g_free (ptr);                                 \
  ptr = new_value;


/** Find a particular pointer value in a GtkListStore */
gboolean
calls_list_store_find (GtkListStore *store,
                       gpointer needle,
                       gint needle_column,
                       GtkTreeIter *iter);

G_END_DECLS

#endif /* CALLS__UTIL_H__ */
