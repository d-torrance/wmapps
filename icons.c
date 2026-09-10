#include "config.h"

#include "icons.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>
#include <wraster.h>

/* Directory types from the icon theme spec. */
enum dir_type { DIR_FIXED, DIR_SCALABLE, DIR_THRESHOLD };

typedef struct {
	char *subdir;
	enum dir_type type;
	int size;
	int min, max;
	int threshold;
} ThemeDir;

typedef struct {
	char *name;
	GArray *dirs;		/* ThemeDir */
} Theme;

struct IconCache {
	WMScreen *scr;
	int size;
	RColor bg;

	GPtrArray *basedirs;	/* char *, in search order */
	GPtrArray *themes;	/* Theme *, inheritance chain */

	GHashTable *paths;	/* icon name -> resolved path, or NEGATIVE */
	GHashTable *pixmaps;	/* resolved path -> WMPixmap * */
	WMPixmap *fallback;
};

/* Sentinel for "we looked and there is no such icon", so a missing icon is not
 * re-stat'd on every keystroke.
 */
static char negative_marker;
#define NEGATIVE (&negative_marker)

static const char *const extensions[] = { "png", "svg", "xpm", NULL };

/* Drawn when an icon cannot be resolved at all -- a NeXT-ish bevelled square,
 * so a row is never blank.
 */
static char *fallback_xpm[] = {
	"16 16 4 1",
	"  c None",
	". c #FFFFFF",
	"X c #7F7F7F",
	"o c #B2B2B2",
	"................",
	".ooooooooooooooX",
	".ooooooooooooooX",
	".ooXXXXXXXXXXooX",
	".ooXooooooooXooX",
	".ooXooooooooXooX",
	".ooXooooooooXooX",
	".ooXooooooooXooX",
	".ooXooooooooXooX",
	".ooXooooooooXooX",
	".ooXooooooooXooX",
	".ooXXXXXXXXXXooX",
	".ooooooooooooooX",
	".ooooooooooooooX",
	".XXXXXXXXXXXXXXX",
	"                "
};

/* ---- theme discovery -------------------------------------------------- */

static void add_basedir(GPtrArray *dirs, const char *path)
{
	if (g_file_test(path, G_FILE_TEST_IS_DIR))
		g_ptr_array_add(dirs, g_strdup(path));
}

/* Icon base directories in the order the spec prescribes.  The user ones are
 * not optional: locally installed icons (Chrome web apps, for instance) live
 * only under ~/.local/share/icons.
 */
static GPtrArray *build_basedirs(void)
{
	GPtrArray *dirs = g_ptr_array_new_with_free_func(g_free);
	const char *const *sys;
	char *p;
	int i;

	p = g_build_filename(g_get_home_dir(), ".icons", NULL);
	add_basedir(dirs, p);
	g_free(p);

	p = g_build_filename(g_get_user_data_dir(), "icons", NULL);
	add_basedir(dirs, p);
	g_free(p);

	sys = g_get_system_data_dirs();
	for (i = 0; sys && sys[i]; i++) {
		p = g_build_filename(sys[i], "icons", NULL);
		add_basedir(dirs, p);
		g_free(p);
	}

	return dirs;
}

static char *detect_theme(void)
{
	GSettingsSchemaSource *src;
	GSettingsSchema *schema;
	GSettings *settings;
	char *name;

	/* g_settings_new() aborts if the schema is not installed, so look it up
	 * first -- there is no guarantee GNOME's schemas are present.
	 */
	src = g_settings_schema_source_get_default();
	if (!src)
		return g_strdup("Adwaita");

	schema = g_settings_schema_source_lookup(src, "org.gnome.desktop.interface",
						 TRUE);
	if (!schema)
		return g_strdup("Adwaita");

	settings = g_settings_new("org.gnome.desktop.interface");
	name = g_settings_get_string(settings, "icon-theme");
	g_object_unref(settings);
	g_settings_schema_unref(schema);

	if (!name || !*name) {
		g_free(name);
		return g_strdup("Adwaita");
	}

	return name;
}

static enum dir_type parse_type(const char *s)
{
	if (!s)
		return DIR_THRESHOLD;	/* the spec's default */
	if (!g_ascii_strcasecmp(s, "Fixed"))
		return DIR_FIXED;
	if (!g_ascii_strcasecmp(s, "Scalable"))
		return DIR_SCALABLE;

	return DIR_THRESHOLD;
}

/* Read one theme's index.theme.  Returns the theme, or NULL if it has no
 * index anywhere on the search path, and appends any inherited theme names to
 * inherits.
 */
static Theme *load_theme(GPtrArray *basedirs, const char *name, GQueue *inherits)
{
	GKeyFile *kf = g_key_file_new();
	Theme *theme = NULL;
	char **subdirs = NULL;
	char **parents = NULL;
	guint i;
	int d;

	/* Icon theme files separate list values with commas; GKeyFile defaults
	 * to semicolons, and would otherwise hand back the entire Directories
	 * value as a single element.
	 */
	g_key_file_set_list_separator(kf, ',');

	for (i = 0; i < basedirs->len; i++) {
		char *idx = g_build_filename(g_ptr_array_index(basedirs, i),
					     name, "index.theme", NULL);
		gboolean ok = g_key_file_load_from_file(kf, idx, G_KEY_FILE_NONE, NULL);

		g_free(idx);
		if (ok)
			break;
	}
	if (i == basedirs->len)
		goto out;

	subdirs = g_key_file_get_string_list(kf, "Icon Theme", "Directories",
					     NULL, NULL);
	if (!subdirs)
		goto out;

	theme = g_new0(Theme, 1);
	theme->name = g_strdup(name);
	theme->dirs = g_array_new(FALSE, FALSE, sizeof(ThemeDir));

	for (d = 0; subdirs[d]; d++) {
		const char *sub = subdirs[d];
		ThemeDir td;
		char *type;
		int scale;

		/* @2x and friends carry Scale=2; they are not icons of the
		 * nominal pixel size and must not be matched as such.
		 */
		scale = g_key_file_get_integer(kf, sub, "Scale", NULL);
		if (scale > 1)
			continue;

		td.size = g_key_file_get_integer(kf, sub, "Size", NULL);
		if (td.size <= 0)
			continue;

		type = g_key_file_get_string(kf, sub, "Type", NULL);
		td.type = parse_type(type);
		g_free(type);

		td.min = g_key_file_get_integer(kf, sub, "MinSize", NULL);
		td.max = g_key_file_get_integer(kf, sub, "MaxSize", NULL);
		td.threshold = g_key_file_get_integer(kf, sub, "Threshold", NULL);
		if (td.min <= 0)
			td.min = td.size;
		if (td.max <= 0)
			td.max = td.size;
		if (td.threshold <= 0)
			td.threshold = 2;

		td.subdir = g_strdup(sub);
		g_array_append_val(theme->dirs, td);
	}

	parents = g_key_file_get_string_list(kf, "Icon Theme", "Inherits", NULL, NULL);
	for (d = 0; parents && parents[d]; d++)
		g_queue_push_tail(inherits, g_strdup(parents[d]));

out:
	g_strfreev(subdirs);
	g_strfreev(parents);
	g_key_file_free(kf);

	return theme;
}

/* Walk the theme and its Inherits chain breadth-first, always ending at
 * hicolor (the spec's mandated fallback theme).
 */
static GPtrArray *build_themes(GPtrArray *basedirs, const char *first)
{
	GPtrArray *themes = g_ptr_array_new();
	GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
	GQueue queue = G_QUEUE_INIT;

	g_queue_push_tail(&queue, g_strdup(first));
	g_queue_push_tail(&queue, g_strdup("hicolor"));

	while (!g_queue_is_empty(&queue)) {
		char *name = g_queue_pop_head(&queue);
		Theme *theme;

		if (g_hash_table_contains(seen, name)) {
			g_free(name);
			continue;
		}
		g_hash_table_add(seen, name);	/* seen owns name */

		theme = load_theme(basedirs, name, &queue);
		if (theme)
			g_ptr_array_add(themes, theme);
	}

	g_hash_table_foreach(seen, (GHFunc)(void *)g_free, NULL);
	g_hash_table_destroy(seen);

	return themes;
}

/* ---- lookup ----------------------------------------------------------- */

/* Distance from a directory's size range to the size we want; 0 is a match.
 * Straight out of the spec's DirectorySizeDistance.
 */
static int size_distance(const ThemeDir *td, int want)
{
	switch (td->type) {
	case DIR_FIXED:
		return ABS(td->size - want);

	case DIR_SCALABLE:
		if (want < td->min)
			return td->min - want;
		if (want > td->max)
			return want - td->max;
		return 0;

	case DIR_THRESHOLD:
	default:
		if (want < td->size - td->threshold)
			return td->size - td->threshold - want;
		if (want > td->size + td->threshold)
			return want - td->size - td->threshold;
		return 0;
	}
}

/* Best file for one icon name across the whole theme chain.
 *
 * Note the loop order: for each theme, for each of its directories, for each
 * base directory.  A single theme is routinely split across base dirs --
 * hicolor exists under both ~/.local/share/icons and /usr/share/icons -- so
 * searching only the base dir that happened to hold index.theme would miss
 * locally installed icons.
 */
static char *lookup_name(IconCache *ic, const char *name)
{
	char *best = NULL;
	int best_dist = G_MAXINT;
	int best_size = -1;
	guint t, d, b;
	int x;

	if (g_path_is_absolute(name))
		return g_file_test(name, G_FILE_TEST_EXISTS) ? g_strdup(name) : NULL;

	for (t = 0; t < ic->themes->len; t++) {
		Theme *theme = g_ptr_array_index(ic->themes, t);

		for (d = 0; d < theme->dirs->len; d++) {
			ThemeDir *td = &g_array_index(theme->dirs, ThemeDir, d);
			int dist = size_distance(td, ic->size);

			/* A worse-fitting directory cannot win, and among
			 * equals prefer the larger icon: scaling down looks
			 * better than scaling up.
			 */
			if (dist > best_dist ||
			    (dist == best_dist && td->size <= best_size))
				continue;

			for (b = 0; b < ic->basedirs->len; b++) {
				for (x = 0; extensions[x]; x++) {
					char *file = g_strdup_printf("%s/%s/%s/%s.%s",
						(char *)g_ptr_array_index(ic->basedirs, b),
						theme->name, td->subdir, name,
						extensions[x]);

					if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
						g_free(file);
						continue;
					}

					g_free(best);
					best = file;
					best_dist = dist;
					best_size = td->size;
					break;
				}
				if (best_dist == 0 && best)
					break;
			}
		}
	}

	if (best)
		return best;

	/* Legacy flat locations, for icons installed outside any theme. */
	for (b = 0; b < ic->basedirs->len; b++) {
		for (x = 0; extensions[x]; x++) {
			char *file = g_strdup_printf("%s/%s.%s",
				(char *)g_ptr_array_index(ic->basedirs, b),
				name, extensions[x]);

			if (g_file_test(file, G_FILE_TEST_EXISTS))
				return file;
			g_free(file);
		}
	}

	for (x = 0; extensions[x]; x++) {
		char *file = g_strdup_printf("/usr/share/pixmaps/%s.%s",
					     name, extensions[x]);

		if (g_file_test(file, G_FILE_TEST_EXISTS))
			return file;
		g_free(file);
	}

	return NULL;
}

static const char *lookup_cached(IconCache *ic, const char *name)
{
	char *path;

	path = g_hash_table_lookup(ic->paths, name);
	if (path)
		return path == NEGATIVE ? NULL : path;

	path = lookup_name(ic, name);
	g_hash_table_insert(ic->paths, g_strdup(name), path ? path : NEGATIVE);

	return path;
}

/* Resolve a GIcon to a file.
 *
 * GIO has already done the hard part: an absolute Icon= value arrives as a
 * GFileIcon with the path resolved, and a bare name arrives as a GThemedIcon
 * carrying an ordered list of names to try.  So there is no need to parse the
 * desktop file's Icon= key ourselves.
 */
static const char *resolve_icon(IconCache *ic, GIcon *icon)
{
	const char *const *names;
	int i;

	if (!icon)
		return lookup_cached(ic, "application-x-executable");

	if (G_IS_FILE_ICON(icon)) {
		GFile *f = g_file_icon_get_file(G_FILE_ICON(icon));
		char *path = f ? g_file_get_path(f) : NULL;
		const char *ret = NULL;

		if (path) {
			ret = lookup_cached(ic, path);
			g_free(path);
		}
		if (ret)
			return ret;
	} else if (G_IS_THEMED_ICON(icon)) {
		names = g_themed_icon_get_names(G_THEMED_ICON(icon));
		for (i = 0; names && names[i]; i++) {
			const char *ret = lookup_cached(ic, names[i]);

			if (ret)
				return ret;
		}
	}

	return lookup_cached(ic, "application-x-executable");
}

/* ---- loading ---------------------------------------------------------- */

/* gdk-pixbuf handles every format we care about, including SVG via the librsvg
 * loader, which libwraster cannot read.  Using it for everything keeps this to
 * one code path.
 */
static WMPixmap *load_pixmap(IconCache *ic, const char *path)
{
	GdkPixbuf *pb;
	RImage *img;
	WMPixmap *pm;
	int w, h, rowstride, bpp, y;
	gboolean alpha;
	const guchar *src;

	pb = gdk_pixbuf_new_from_file_at_scale(path, ic->size, ic->size, TRUE, NULL);
	if (!pb)
		return NULL;

	w = gdk_pixbuf_get_width(pb);
	h = gdk_pixbuf_get_height(pb);
	rowstride = gdk_pixbuf_get_rowstride(pb);
	alpha = gdk_pixbuf_get_has_alpha(pb);
	bpp = alpha ? 4 : 3;
	src = gdk_pixbuf_get_pixels(pb);

	img = RCreateImage(w, h, alpha);
	if (!img) {
		g_object_unref(pb);
		return NULL;
	}

	/* Copy row by row: RImage rows are tightly packed at width*bpp, while
	 * gdk-pixbuf pads each row out to a four-byte boundary.  A single flat
	 * memcpy happens to work when the padding is zero and shears the image
	 * when it is not.
	 *
	 * Both sides use straight, non-premultiplied alpha, so the bytes need
	 * no further conversion.
	 */
	for (y = 0; y < h; y++)
		memcpy(img->data + (size_t)y * w * bpp,
		       src + (size_t)y * rowstride,
		       (size_t)w * bpp);

	/* Blending against the list background keeps antialiased edges; the
	 * alternative thresholds alpha into a bitmap mask and looks ragged.
	 */
#ifdef HAVE_WMCREATEBLENDEDPIXMAPFROMRIMAGE
	pm = WMCreateBlendedPixmapFromRImage(ic->scr, img, &ic->bg);
#else
	pm = WMCreatePixmapFromRImage(ic->scr, img, 127);
#endif
	RReleaseImage(img);
	g_object_unref(pb);

	return pm;
}

/* ---- public interface ------------------------------------------------- */

IconCache *icons_new(WMScreen *scr, const char *theme, int size, const RColor *bg)
{
	IconCache *ic = g_new0(IconCache, 1);
	char *detected = NULL;

	ic->scr = scr;
	ic->size = size;
	ic->bg = *bg;

	if (!theme) {
		detected = detect_theme();
		theme = detected;
	}

	ic->basedirs = build_basedirs();
	ic->themes = build_themes(ic->basedirs, theme);
	g_free(detected);

	/* Values are either an owned path or the NEGATIVE sentinel, so they
	 * cannot be freed automatically.
	 */
	ic->paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	ic->pixmaps = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	ic->fallback = WMCreatePixmapFromXPMData(scr, fallback_xpm);

	return ic;
}

void icons_free(IconCache *ic)
{
	GHashTableIter iter;
	gpointer key, value;
	guint t, d;

	if (!ic)
		return;

	g_hash_table_iter_init(&iter, ic->pixmaps);
	while (g_hash_table_iter_next(&iter, &key, &value))
		WMReleasePixmap(value);
	g_hash_table_destroy(ic->pixmaps);

	g_hash_table_iter_init(&iter, ic->paths);
	while (g_hash_table_iter_next(&iter, &key, &value))
		if (value != NEGATIVE)
			g_free(value);
	g_hash_table_destroy(ic->paths);

	for (t = 0; t < ic->themes->len; t++) {
		Theme *theme = g_ptr_array_index(ic->themes, t);

		for (d = 0; d < theme->dirs->len; d++)
			g_free(g_array_index(theme->dirs, ThemeDir, d).subdir);
		g_array_free(theme->dirs, TRUE);
		g_free(theme->name);
		g_free(theme);
	}
	g_ptr_array_free(ic->themes, TRUE);
	g_ptr_array_free(ic->basedirs, TRUE);

	if (ic->fallback)
		WMReleasePixmap(ic->fallback);
	g_free(ic);
}

WMPixmap *icons_get(IconCache *ic, GAppInfo *info)
{
	const char *path;
	WMPixmap *pm;

	path = resolve_icon(ic, g_app_info_get_icon(info));
	if (!path)
		return ic->fallback;

	pm = g_hash_table_lookup(ic->pixmaps, path);
	if (pm)
		return pm;

	pm = load_pixmap(ic, path);
	if (!pm)
		return ic->fallback;

	g_hash_table_insert(ic->pixmaps, g_strdup(path), pm);

	return pm;
}

char *icons_resolve(IconCache *ic, GAppInfo *info)
{
	const char *path = resolve_icon(ic, g_app_info_get_icon(info));

	return path ? g_strdup(path) : NULL;
}
