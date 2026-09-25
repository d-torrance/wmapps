/* wmapps - an application launcher for Window Maker.
 *
 * WINGs widgets for the look of the desktop it runs on, GIO's desktop-file
 * index for the search behaviour of a modern launcher.
 */

#include "config.h"

#include <WINGs/WINGs.h>
#include <gio/gio.h>
#include <wraster.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "appdb.h"
#include "icons.h"
#include "ui.h"

#include "icon/wmapps_icon.h"

static void usage(FILE *out, int status)
{
	fprintf(out,
		"Usage: wmapps [options]\n"
		"\n"
		"An application launcher for Window Maker.  Type to filter, Up\n"
		"and Down to choose, Return to launch, Escape to quit.\n"
		"\n"
		"Options:\n"
		"  -t, --icon-theme NAME  use this icon theme instead of the\n"
		"                         desktop's configured one\n"
		"      --print [QUERY]    print matching applications and exit\n"
		"      --print-icons      print each application's resolved icon\n"
		"                         file and exit\n"
		"  -h, --help             show this message\n"
		"  -V, --version          show version information\n");
	exit(status);
}

/* Non-graphical modes, for checking the search and icon lookup against what
 * GIO itself reports without having to eyeball the window.
 */
static int print_matches(const char *query)
{
	AppList apps = { 0 };
	int i;

	appdb_query(&apps, query);
	for (i = 0; i < apps.count; i++)
		printf("%s\t%s\n", g_app_info_get_id(apps.items[i].info),
		       apps.items[i].name ? apps.items[i].name : "");

	appdb_free(&apps);

	return 0;
}

static int print_icons(WMScreen *scr, const char *theme)
{
	AppList apps = { 0 };
	RColor bg = { 0xae, 0xaa, 0xae, 0xff };
	IconCache *icons;
	int i;

	icons = icons_new(scr, theme, 32, &bg);

	appdb_query(&apps, NULL);
	for (i = 0; i < apps.count; i++) {
		char *path = icons_resolve(icons, apps.items[i].info);

		printf("%-40s %s\n", apps.items[i].name ? apps.items[i].name : "",
		       path ? path : "(fallback)");
		g_free(path);
	}

	appdb_free(&apps);
	icons_free(icons);

	return 0;
}

/* Give Window Maker something to draw for the appicon, the miniaturized
 * window and a Dock tile.  Without this it falls back to its generic default
 * icon.  The image is compiled in so it is right whether or not the icon file
 * has been installed; WMSetApplicationIconImage takes its own reference.
 */
static void set_application_icon(WMScreen *scr)
{
	RImage *icon;

	icon = RCreateImage(WMAPPS_ICON_WIDTH, WMAPPS_ICON_HEIGHT, True);
	if (!icon)
		return;
	memcpy(icon->data, wmapps_icon_rgba, sizeof(wmapps_icon_rgba));

	WMSetApplicationIconImage(scr, icon);
	RReleaseImage(icon);
}

int main(int argc, char **argv)
{
	const char *theme = NULL;
	const char *query = NULL;
	gboolean do_print = FALSE, do_print_icons = FALSE;
	Display *dpy;
	WMScreen *scr;
	UI *ui;
	int i;

	WMInitializeApplication("wmapps", &argc, argv);

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
			usage(stdout, 0);
		} else if (!strcmp(a, "-V") || !strcmp(a, "--version")) {
			printf("%s\n", PACKAGE_STRING);
			return 0;
		} else if (!strcmp(a, "-t") || !strcmp(a, "--icon-theme")) {
			if (++i >= argc) {
				fprintf(stderr, "wmapps: %s needs an argument\n", a);
				return 2;
			}
			theme = argv[i];
		} else if (!strcmp(a, "--print")) {
			do_print = TRUE;
			if (i + 1 < argc && argv[i + 1][0] != '-')
				query = argv[++i];
		} else if (!strcmp(a, "--print-icons")) {
			do_print_icons = TRUE;
		} else {
			fprintf(stderr, "wmapps: unrecognised option '%s'\n", a);
			usage(stderr, 2);
		}
	}

	if (!theme)
		theme = g_getenv("WMAPPS_ICON_THEME");

	if (do_print)
		return print_matches(query);

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "wmapps: cannot open display\n");
		return 1;
	}

	/* A full application screen rather than a simple one: it sets the
	 * client leader and window group on our window, which is what makes
	 * Window Maker treat us as an application, give us an appicon and let
	 * that appicon be dragged into the Dock.  A simple screen skips all of
	 * that and leaves us with no appicon at all.
	 */
	scr = WMCreateScreen(dpy, DefaultScreen(dpy));
	if (!scr) {
		fprintf(stderr, "wmapps: cannot initialize WINGs screen\n");
		return 1;
	}

	if (do_print_icons)
		return print_icons(scr, theme);

	set_application_icon(scr);

	ui = ui_new(scr, theme);
	ui_show(ui);

	WMScreenMainLoop(scr);	/* does not return; ui_quit() exits */
}
