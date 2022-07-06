#include <glib-object.h>
#include <gdk/gdk.h>

#include "gtksorter.h"
#include "gtkfilter.h"

/* enumerations from "gtksorter.h" */
GDK_AVAILABLE_IN_ALL GType gtk_sorter_order_get_type (void) G_GNUC_CONST;
#define GTK_TYPE_SORTER_ORDER (gtk_sorter_order_get_type ())
GDK_AVAILABLE_IN_ALL GType gtk_sorter_change_get_type (void) G_GNUC_CONST;
#define GTK_TYPE_SORTER_CHANGE (gtk_sorter_change_get_type ())

/* enumerations from "gtkfilter.h" */
GDK_AVAILABLE_IN_ALL GType gtk_filter_match_get_type (void) G_GNUC_CONST;
#define GTK_TYPE_FILTER_MATCH (gtk_filter_match_get_type ())
GDK_AVAILABLE_IN_ALL GType gtk_filter_change_get_type (void) G_GNUC_CONST;
#define GTK_TYPE_FILTER_CHANGE (gtk_filter_change_get_type ())

