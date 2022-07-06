#ifndef __GTKINTL_H__
#define __GTKINTL_H__

#ifndef GETTEXT_PACKAGE
# define GETTEXT_PACKAGE
#endif
#include <glib/gi18n-lib.h>

#ifndef G_GNUC_FALLTHROUGH
#define G_GNUC_FALLTHROUGH __attribute__((fallthrough))
#endif

#ifdef ENABLE_NLS
#define P_(String) g_dgettext(GETTEXT_PACKAGE "-properties",String)
#else 
#define P_(String) (String)
#endif

/* not really I18N-related, but also a string marker macro */
#define I_(string) g_intern_static_string (string)

#endif
