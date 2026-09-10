/* icons.h - XDG icon theme lookup and pixmap caching.
 *
 * Resolves a GAppInfo's GIcon to a file through the icon theme search path,
 * loads it via gdk-pixbuf (so SVG works), converts it to a WINGs pixmap and
 * caches the result.  Lookups are lazy: nothing touches the disk until a row
 * is actually drawn.
 */

#ifndef WMAPPS_ICONS_H
#define WMAPPS_ICONS_H

#include <WINGs/WINGs.h>
#include <gio/gio.h>

typedef struct IconCache IconCache;

/* Create a cache rendering icons at size pixels square, blended against the
 * background colour bg so antialiased edges survive.  theme may be NULL to
 * autodetect.
 */
IconCache *icons_new(WMScreen *scr, const char *theme, int size,
		     const RColor *bg);

void icons_free(IconCache *ic);

/* The pixmap for info, or a generic fallback if it cannot be resolved.  Never
 * returns NULL.  The cache retains ownership.
 */
WMPixmap *icons_get(IconCache *ic, GAppInfo *info);

/* The file icons_get() would load for info, or NULL if it would fall back to
 * the built-in placeholder.  Caller must g_free().  Used by --print-icons.
 */
char *icons_resolve(IconCache *ic, GAppInfo *info);

#endif /* WMAPPS_ICONS_H */
