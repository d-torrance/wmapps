/* appdb.h - installed-application enumeration, search and launching.
 *
 * Wraps GIO's desktop-file index.  Two query modes, mirroring what GNOME
 * Shell's AppSearchProvider does: an empty query browses every installed
 * application alphabetically, a non-empty one runs the relevance-ranked
 * g_desktop_app_info_search().
 */

#ifndef WMAPPS_APPDB_H
#define WMAPPS_APPDB_H

#include <gio/gio.h>

typedef struct {
	GAppInfo *info;		/* owned reference */
	const char *name;	/* borrowed from info */
	const char *comment;	/* borrowed from info; may be NULL */
	char *sortkey;		/* collation key; only set when browsing */
} AppEntry;

typedef struct {
	AppEntry *items;
	int count;
	int cap;
} AppList;

/* Fill l with the applications matching query (which may be NULL or empty to
 * browse everything).  Reuses l's allocation, so it is cheap to call on every
 * keystroke.  Entries are ordered by relevance when searching, alphabetically
 * when browsing.
 */
void appdb_query(AppList *l, const char *query);

/* Release every reference held by l.  Safe on a zero-initialized AppList. */
void appdb_free(AppList *l);

/* Spawn the application.  Returns TRUE on success; on failure returns FALSE
 * and, if err is non-NULL, stores a message the caller must g_free().
 */
gboolean appdb_launch(const AppEntry *e, char **err);

/* Flush the session bus so an in-flight DBus activation survives exit(). */
void appdb_flush_launch(void);

#endif /* WMAPPS_APPDB_H */
