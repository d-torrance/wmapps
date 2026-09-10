/* ui.h - the launcher window. */

#ifndef WMAPPS_UI_H
#define WMAPPS_UI_H

#include <WINGs/WINGs.h>

#include "appdb.h"
#include "icons.h"

typedef struct UI UI;

/* Build the window and its widgets.  Does not map it. */
UI *ui_new(WMScreen *scr, const char *icon_theme);

/* Map the window, populate the list and focus the entry.  Split from ui_new()
 * so a future --daemon mode can show and hide without rebuilding.
 */
void ui_show(UI *ui);

/* Tear everything down and exit; never returns.  WMScreenMainLoop is
 * _Noreturn, so this is the only way out of the program.
 */
void ui_quit(UI *ui, int status) __attribute__((noreturn));

#endif /* WMAPPS_UI_H */
