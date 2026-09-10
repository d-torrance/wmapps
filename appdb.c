#include "appdb.h"

#include <gio/gdesktopappinfo.h>
#include <stdlib.h>
#include <string.h>

static void append(AppList *l, GAppInfo *info, gboolean want_sortkey)
{
	AppEntry *e;

	if (l->count == l->cap) {
		l->cap = l->cap ? l->cap * 2 : 64;
		l->items = g_realloc_n(l->items, l->cap, sizeof(*l->items));
	}

	e = &l->items[l->count++];
	e->info = info;
	e->name = g_app_info_get_display_name(info);
	e->comment = g_app_info_get_description(info);
	e->sortkey = NULL;

	if (want_sortkey && e->name) {
		char *folded = g_utf8_casefold(e->name, -1);

		e->sortkey = g_utf8_collate_key(folded, -1);
		g_free(folded);
	}
}

static void reset(AppList *l)
{
	int i;

	for (i = 0; i < l->count; i++) {
		g_object_unref(l->items[i].info);
		g_free(l->items[i].sortkey);
	}
	l->count = 0;	/* keep the allocation: we re-query on every keystroke */
}

static int by_sortkey(const void *a, const void *b)
{
	const AppEntry *x = a, *y = b;

	if (!x->sortkey || !y->sortkey)
		return (x->sortkey ? 1 : 0) - (y->sortkey ? 1 : 0);

	return strcmp(x->sortkey, y->sortkey);
}

static gboolean blank(const char *s)
{
	if (!s)
		return TRUE;

	while (*s) {
		if (!g_ascii_isspace(*s))
			return FALSE;
		s++;
	}
	return TRUE;
}

/* Every installed application, alphabetically.  g_desktop_app_info_search()
 * returns nothing for an empty query, so this is what fills the list when the
 * entry is empty -- the same thing the GNOME Shell overview shows.
 */
static void browse_all(AppList *l)
{
	GList *all, *it;

	all = g_app_info_get_all();
	for (it = all; it; it = it->next) {
		GAppInfo *info = it->data;

		if (g_app_info_should_show(info))
			append(l, g_object_ref(info), TRUE);
	}
	g_list_free_full(all, g_object_unref);

	qsort(l->items, l->count, sizeof(*l->items), by_sortkey);
}

/* Relevance-ranked search.  The result is a NULL-terminated array of
 * NULL-terminated arrays: one inner array per relevance tier, best first.  We
 * flatten it in order and keep that order.
 *
 * The search deliberately does not filter NoDisplay/Hidden entries, so we have
 * to -- otherwise stubs like firefox.desktop (NoDisplay=true on Ubuntu, a snap
 * redirect) show up as duplicates of the real application.
 */
static void search(AppList *l, const char *query)
{
	gchar ***tiers;
	int t, i;

	tiers = g_desktop_app_info_search(query);
	if (!tiers)
		return;

	for (t = 0; tiers[t]; t++) {
		for (i = 0; tiers[t][i]; i++) {
			GDesktopAppInfo *d = g_desktop_app_info_new(tiers[t][i]);

			if (!d)
				continue;

			if (g_app_info_should_show(G_APP_INFO(d)))
				append(l, G_APP_INFO(d), FALSE);
			else
				g_object_unref(d);
		}
	}

	for (t = 0; tiers[t]; t++)
		g_strfreev(tiers[t]);
	g_free(tiers);
}

void appdb_query(AppList *l, const char *query)
{
	reset(l);

	if (blank(query))
		browse_all(l);
	else
		search(l, query);
}

void appdb_free(AppList *l)
{
	reset(l);
	g_free(l->items);
	l->items = NULL;
	l->cap = 0;
}

gboolean appdb_launch(const AppEntry *e, char **err)
{
	GAppLaunchContext *ctx;
	GError *gerr = NULL;
	gboolean ok;

	/* A plain launch context rather than GDK's: all we lose is the
	 * startup-notification id, and Window Maker does not implement startup
	 * notification anyway.
	 */
	ctx = g_app_launch_context_new();
	ok = g_app_info_launch(e->info, NULL, ctx, &gerr);
	g_object_unref(ctx);

	if (!ok) {
		if (err)
			*err = g_strdup(gerr ? gerr->message : "launch failed");
		g_clear_error(&gerr);
	}

	return ok;
}

/* DBusActivatable applications are started with an asynchronous D-Bus call, so
 * exiting immediately can drop the message before it reaches the bus.
 */
void appdb_flush_launch(void)
{
	GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

	if (!bus)
		return;

	g_dbus_connection_flush_sync(bus, NULL, NULL);
	g_object_unref(bus);
}
