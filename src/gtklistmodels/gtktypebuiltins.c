#include <gtk/gtk.h>
#include "gtkprivate.h"

#include "gtktypebuiltins.h"

/* enumerations from "gtksorter.h" */
GType
gtk_sorter_order_get_type (void)
{
  static gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { GTK_SORTER_ORDER_PARTIAL, "GTK_SORTER_ORDER_PARTIAL", "partial" },
        { GTK_SORTER_ORDER_NONE, "GTK_SORTER_ORDER_NONE", "none" },
        { GTK_SORTER_ORDER_TOTAL, "GTK_SORTER_ORDER_TOTAL", "total" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("GtkSorterOrder"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}
GType
gtk_sorter_change_get_type (void)
{
  static gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { GTK_SORTER_CHANGE_DIFFERENT, "GTK_SORTER_CHANGE_DIFFERENT", "different" },
        { GTK_SORTER_CHANGE_INVERTED, "GTK_SORTER_CHANGE_INVERTED", "inverted" },
        { GTK_SORTER_CHANGE_LESS_STRICT, "GTK_SORTER_CHANGE_LESS_STRICT", "less-strict" },
        { GTK_SORTER_CHANGE_MORE_STRICT, "GTK_SORTER_CHANGE_MORE_STRICT", "more-strict" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("GtkSorterChange"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

/* enumerations from "gtkfilter.h" */
GType
gtk_filter_match_get_type (void)
{
  static gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { GTK_FILTER_MATCH_SOME, "GTK_FILTER_MATCH_SOME", "some" },
        { GTK_FILTER_MATCH_NONE, "GTK_FILTER_MATCH_NONE", "none" },
        { GTK_FILTER_MATCH_ALL, "GTK_FILTER_MATCH_ALL", "all" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("GtkFilterMatch"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}
GType
gtk_filter_change_get_type (void)
{
  static gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { GTK_FILTER_CHANGE_DIFFERENT, "GTK_FILTER_CHANGE_DIFFERENT", "different" },
        { GTK_FILTER_CHANGE_LESS_STRICT, "GTK_FILTER_CHANGE_LESS_STRICT", "less-strict" },
        { GTK_FILTER_CHANGE_MORE_STRICT, "GTK_FILTER_CHANGE_MORE_STRICT", "more-strict" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("GtkFilterChange"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}
