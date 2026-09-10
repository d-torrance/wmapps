#include "config.h"

#include "ui.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <WINGs/WUtil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ICON_SIZE	32
#define PAD		6

struct UI {
	WMScreen *scr;
	WMWindow *win;
	WMBox *box;
	WMTextField *entry;
	WMList *list;
	WMLabel *status;

	AppList apps;
	IconCache *icons;

	WMFont *bold;
	WMFont *normal;
	WMColor *text;
	WMColor *dim;
	WMColor *bg;
	WMColor *selbg;

	WMTextFieldDelegate delegate;
	gboolean launched;
};

#ifndef HAVE_WMGETSCALEBASEFROMSYSTEMFONT
/* Older WINGs has no scale base; fall back to unscaled pixel values. */
#define WMGetScaleBaseFromSystemFont(s, w, h)	do { *(w) = 177; *(h) = 15; } while (0)
#endif

/* ---- drawing ---------------------------------------------------------- */

/* WINGs does not clip the user draw proc, so anything too wide has to be
 * shortened by hand.  Steps back over whole characters so a multi-byte name is
 * never cut mid-sequence.
 */
static void draw_clipped(UI *ui, Drawable d, WMFont *font, WMColor *color,
			 int x, int y, int avail, const char *s)
{
	int len = strlen(s);
	char *buf;

	if (avail <= 0 || !len)
		return;

	if (WMWidthOfString(font, s, len) <= avail) {
		WMDrawString(ui->scr, d, color, font, x, y, s, len);
		return;
	}

	buf = wmalloc(len + 4);
	memcpy(buf, s, len);

	while (len > 0) {
		const char *prev = g_utf8_find_prev_char(buf, buf + len);

		len = prev ? (int)(prev - buf) : 0;
		strcpy(buf + len, "...");
		if (WMWidthOfString(font, buf, len + 3) <= avail)
			break;
	}

	if (len > 0)
		WMDrawString(ui->scr, d, color, font, x, y, buf, len + 3);

	wfree(buf);
}

static void draw_row(WMList *list, int index, Drawable d, char *text,
		     int state, WMRect *rect)
{
	UI *ui = WMGetHangedData(list);
	WMListItem *item = WMGetListItem(list, index);
	AppEntry *e;
	WMPixmap *icon;
	WMSize isize;
	Display *dpy = WMScreenDisplay(ui->scr);
	int x, y, avail;
	intptr_t row;

	if (!item)
		return;

	/* clientData holds the row index rather than an AppEntry pointer: the
	 * entry array is reallocated on every re-query, so a stored pointer
	 * would dangle.
	 */
	row = (intptr_t)item->clientData;
	if (row < 0 || row >= ui->apps.count)
		return;
	e = &ui->apps.items[row];

	XFillRectangle(dpy, d, WMColorGC(state & WLDSSelected ? ui->selbg : ui->bg),
		       rect->pos.x, rect->pos.y, rect->size.width, rect->size.height);

	/* Icons are loaded here, on first paint, rather than up front: browsing
	 * every installed application would otherwise stall the window opening
	 * while a hundred files are read.
	 */
	icon = icons_get(ui->icons, e->info);
	isize = WMGetPixmapSize(icon);
	WMDrawPixmap(icon, d, rect->pos.x + PAD,
		     rect->pos.y + (rect->size.height - isize.height) / 2);

	x = rect->pos.x + PAD + ICON_SIZE + PAD;
	avail = rect->size.width - (x - rect->pos.x) - PAD;

	if (e->comment && *e->comment) {
		y = rect->pos.y + (rect->size.height -
				   WMFontHeight(ui->bold) - WMFontHeight(ui->normal)) / 2;
		draw_clipped(ui, d, ui->bold, ui->text, x, y, avail,
			     e->name ? e->name : "");
		draw_clipped(ui, d, ui->normal, ui->dim, x,
			     y + WMFontHeight(ui->bold), avail, e->comment);
	} else {
		y = rect->pos.y + (rect->size.height - WMFontHeight(ui->bold)) / 2;
		draw_clipped(ui, d, ui->bold, ui->text, x, y, avail,
			     e->name ? e->name : "");
	}
}

/* ---- list contents ---------------------------------------------------- */

static void set_status(UI *ui)
{
	char buf[64];

	if (ui->apps.count == 0)
		snprintf(buf, sizeof(buf), "No matches");
	else if (ui->apps.count == 1)
		snprintf(buf, sizeof(buf), "1 application");
	else
		snprintf(buf, sizeof(buf), "%d applications", ui->apps.count);

	WMSetLabelText(ui->status, buf);
}

static void refill(UI *ui, const char *query)
{
	int i;

	appdb_query(&ui->apps, query);

	WMClearList(ui->list);
	for (i = 0; i < ui->apps.count; i++) {
		WMListItem *item = WMInsertListItem(ui->list, -1,
			ui->apps.items[i].name ? ui->apps.items[i].name : "");

		if (item)
			item->clientData = (void *)(intptr_t)i;
	}

	if (ui->apps.count > 0) {
		WMSelectListItem(ui->list, 0);
		WMSetListPosition(ui->list, 0);
	}

	set_status(ui);
}

/* ---- actions ---------------------------------------------------------- */

static void launch_selected(UI *ui)
{
	int row = WMGetListSelectedItemRow(ui->list);
	char *err = NULL;

	if (row < 0 || row >= ui->apps.count)
		return;

	if (appdb_launch(&ui->apps.items[row], &err)) {
		ui->launched = TRUE;
		ui_quit(ui, 0);
	}

	WMSetLabelText(ui->status, err ? err : "Could not launch application");
	g_free(err);
}

static void move_selection(UI *ui, int delta)
{
	int row = WMGetListSelectedItemRow(ui->list);
	int rows = WMGetListNumberOfRows(ui->list);
	int height, visible, pos;

	if (rows <= 0)
		return;

	row = CLAMP(row + delta, 0, rows - 1);
	WMSelectListItem(ui->list, row);

	/* Keep the selection on screen.  The list scrolls itself for mouse
	 * input only, so moving by keyboard has to adjust the origin.
	 */
	height = WMGetListItemHeight(ui->list);
	visible = height > 0 ? (int)WMWidgetHeight(ui->list) / height : 1;
	if (visible < 1)
		visible = 1;

	pos = WMGetListPosition(ui->list);
	if (row < pos)
		WMSetListPosition(ui->list, row);
	else if (row >= pos + visible)
		WMSetListPosition(ui->list, row - visible + 1);
}

static void close_action(WMWidget *w, void *data)
{
	ui_quit(data, 0);
}

static void list_double_action(WMWidget *w, void *data)
{
	launch_selected(data);
}

/* The text field ignores Up and Down -- it relays anything it does not handle
 * to the next responder -- so arrow navigation is ours to implement.  Return
 * and Escape are handled by the delegate instead, so they cannot be acted on
 * twice: WINGs event handlers cannot consume an event, and every handler
 * registered for a view runs.
 */
static void key_press(XEvent *event, void *data)
{
	UI *ui = data;
	KeySym ks = XLookupKeysym(&event->xkey, 0);
	int height, visible;

	height = WMGetListItemHeight(ui->list);
	visible = height > 0 ? (int)WMWidgetHeight(ui->list) / height : 1;
	if (visible < 1)
		visible = 1;

	switch (ks) {
	case XK_Up:
		move_selection(ui, -1);
		break;
	case XK_Down:
		move_selection(ui, 1);
		break;
	case XK_Page_Up:
		move_selection(ui, -visible);
		break;
	case XK_Page_Down:
		move_selection(ui, visible);
		break;
	default:
		break;
	}
}

static void entry_changed(WMTextFieldDelegate *self, WMNotification *notif)
{
	UI *ui = self->data;
	char *text = WMGetTextFieldText(ui->entry);

	/* Searching is fast enough at this scale to run on every keystroke; if
	 * a very large XDG_DATA_DIRS ever makes it lag, debounce it behind a
	 * one-shot WMAddTimerHandler.
	 */
	refill(ui, text);
	wfree(text);
}

static void entry_done(WMTextFieldDelegate *self, WMNotification *notif)
{
	UI *ui = self->data;
	int movement = (int)(uintptr_t)WMGetNotificationClientData(notif);

	if (movement == WMReturnTextMovement)
		launch_selected(ui);
	else if (movement == WMEscapeTextMovement)
		ui_quit(ui, 0);
}

/* ---- construction ----------------------------------------------------- */

UI *ui_new(WMScreen *scr, const char *icon_theme)
{
	UI *ui = g_new0(UI, 1);
	RColor bg;
	int wmScaleWidth, wmScaleHeight;
	int width, height, entry_height, status_height, row_height;

	ui->scr = scr;

	WMGetScaleBaseFromSystemFont(scr, &wmScaleWidth, &wmScaleHeight);

	ui->bold = WMRetainFont(WMDefaultBoldSystemFont(scr));
	ui->normal = WMRetainFont(WMDefaultSystemFont(scr));
	ui->text = WMRetainColor(WMBlackColor(scr));
	ui->dim = WMRetainColor(WMDarkGrayColor(scr));
	ui->bg = WMRetainColor(WMGrayColor(scr));
	ui->selbg = WMRetainColor(WMWhiteColor(scr));

	bg = WMGetRColorFromColor(ui->bg);
	ui->icons = icons_new(scr, icon_theme, ICON_SIZE, &bg);

	width = WMScaleX(450);
	height = WMScaleY(400);
	entry_height = WMFontHeight(ui->normal) + WMScaleY(10);
	status_height = WMFontHeight(ui->normal) + WMScaleY(4);
	row_height = WMAX(ICON_SIZE + PAD,
			  WMFontHeight(ui->bold) + WMFontHeight(ui->normal) + PAD);

	ui->win = WMCreateWindowWithStyle(scr, "wmapps",
					  WMTitledWindowMask | WMClosableWindowMask |
					  WMResizableWindowMask);
	WMSetWindowTitle(ui->win, "Applications");
	WMSetWindowCloseAction(ui->win, close_action, ui);
	WMResizeWidget(ui->win, width, height);
	WMSetWindowMinSize(ui->win, WMScaleX(280), WMScaleY(200));

	/* One third down reads better than dead centre.  Must be set before
	 * the window is realized for the hint to be seen.
	 */
	WMSetWindowUserPosition(ui->win,
				((int)WMScreenWidth(scr) - width) / 2,
				((int)WMScreenHeight(scr) - height) / 3);

	/* Reuse the icon main.c already installed, so a miniaturized window
	 * matches the appicon instead of reverting to the default.
	 */
	{
		RImage *appicon = WMGetApplicationIconImage(scr);

		if (appicon)
			WMSetWindowMiniwindowImage(ui->win, appicon);
	}

	ui->box = WMCreateBox(ui->win);
	WMSetBoxHorizontal(ui->box, False);
	WMSetBoxBorderWidth(ui->box, WMScaleX(8));
	WMResizeWidget(ui->box, width, height);
	WMSetViewExpandsToParent(WMWidgetView(ui->box), 0, 0, 0, 0);

	ui->entry = WMCreateTextField(ui->box);
	WMSetTextFieldBeveled(ui->entry, True);
	ui->delegate.data = ui;
	ui->delegate.didChange = entry_changed;
	ui->delegate.didEndEditing = entry_done;
	WMSetTextFieldDelegate(ui->entry, &ui->delegate);
	WMCreateEventHandler(WMWidgetView(ui->entry), KeyPressMask, key_press, ui);

	ui->list = WMCreateList(ui->box);
	WMHangData(ui->list, ui);
	WMSetListUserDrawItemHeight(ui->list, row_height);
	WMSetListUserDrawProc(ui->list, draw_row);
	WMSetListDoubleAction(ui->list, list_double_action, ui);

	ui->status = WMCreateLabel(ui->box);
	WMSetLabelTextAlignment(ui->status, WALeft);
	WMSetLabelFont(ui->status, ui->normal);

	WMAddBoxSubview(ui->box, WMWidgetView(ui->entry), False, True,
			entry_height, 0, WMScaleY(6));
	WMAddBoxSubview(ui->box, WMWidgetView(ui->list), True, True,
			row_height * 2, 0, WMScaleY(4));
	WMAddBoxSubview(ui->box, WMWidgetView(ui->status), False, True,
			status_height, 0, 0);

	return ui;
}

/* WINGs labels the group leader window ("groupLeader", appname).  Window Maker
 * takes an application's identity from that leader, so it would file us under
 * "groupLeader.wmapps" -- the name its Dock records, and the name it caches our
 * icon under.  Relabel it so we are identified the way any other application
 * is.  The leader is only reachable once a window of ours is realized and
 * carries the group hint.
 */
static void name_group_leader(UI *ui)
{
	Display *dpy = WMScreenDisplay(ui->scr);
	XClassHint hint;
	XWMHints *hints;

	hints = XGetWMHints(dpy, WMWidgetXID(ui->win));
	if (!hints)
		return;

	if (hints->flags & WindowGroupHint) {
		hint.res_name = (char *)"wmapps";
		hint.res_class = (char *)"Wmapps";
		XSetClassHint(dpy, hints->window_group, &hint);
	}
	XFree(hints);
}

void ui_show(UI *ui)
{
	WMRealizeWidget(ui->win);
	name_group_leader(ui);
	WMMapSubwidgets(ui->box);
	WMMapSubwidgets(ui->win);
	WMMapWidget(ui->win);

	refill(ui, NULL);

	WMSetFocusToWidget(ui->entry);
}

void ui_quit(UI *ui, int status)
{
	if (ui->launched)
		appdb_flush_launch();

	icons_free(ui->icons);
	appdb_free(&ui->apps);

	WMReleaseFont(ui->bold);
	WMReleaseFont(ui->normal);
	WMReleaseColor(ui->text);
	WMReleaseColor(ui->dim);
	WMReleaseColor(ui->bg);
	WMReleaseColor(ui->selbg);

	XSync(WMScreenDisplay(ui->scr), False);

	g_free(ui);

	WMReleaseApplication();
	exit(status);
}
