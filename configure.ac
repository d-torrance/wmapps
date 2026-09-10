AC_PREREQ([2.69])
AC_INIT([wmapps], [0.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([1.16 foreign -Wall])
AM_SILENT_RULES([yes])

AC_PROG_CC
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG([0.24])

dnl ---- glib / gio -------------------------------------------------------
dnl g_desktop_app_info_search() was added in glib 2.40.
PKG_CHECK_MODULES([GIO], [glib-2.0 >= 2.40 gio-2.0 >= 2.40 gio-unix-2.0 >= 2.40])
PKG_CHECK_MODULES([GDKPIXBUF], [gdk-pixbuf-2.0 >= 2.24])

dnl ---- WINGs ------------------------------------------------------------
dnl Modern Window Maker ships WINGs.pc; older installs only had the
dnl get-wings-flags / get-wraster-flags scripts, so fall back to those.
PKG_CHECK_MODULES([WINGS], [WINGs], [], [
  AC_MSG_NOTICE([no WINGs.pc found, trying get-wings-flags])
  AC_PATH_PROG([GET_WINGS_FLAGS], [get-wings-flags])
  AC_PATH_PROG([GET_WRASTER_FLAGS], [get-wraster-flags])
  AS_IF([test -z "$GET_WINGS_FLAGS"],
        [AC_MSG_ERROR([WINGs not found: install Window Maker's dev package])])
  WINGS_CFLAGS="`$GET_WINGS_FLAGS --cflags`"
  WINGS_LIBS="`$GET_WINGS_FLAGS --libs` `$GET_WRASTER_FLAGS --libs`"
])

dnl WINGs' pkg-config output does NOT include -lX11, and we call Xlib directly.
PKG_CHECK_MODULES([X11], [x11])

dnl ---- confirm the symbols we actually depend on -------------------------
save_LIBS="$LIBS"; LIBS="$GIO_LIBS $LIBS"
AC_CHECK_FUNCS([g_desktop_app_info_search], [],
  [AC_MSG_ERROR([gio lacks g_desktop_app_info_search (need glib >= 2.40)])])
LIBS="$save_LIBS"

save_CPPFLAGS="$CPPFLAGS"; CPPFLAGS="$WINGS_CFLAGS $CPPFLAGS"
AC_CHECK_HEADERS([WINGs/WINGs.h WINGs/WUtil.h wraster.h], [],
  [AC_MSG_ERROR([WINGs/wraster headers not found])])
CPPFLAGS="$save_CPPFLAGS"

dnl Optional WINGs entry points; absent in older releases, code has fallbacks.
save_LIBS="$LIBS"; LIBS="$WINGS_LIBS $LIBS"
AC_CHECK_FUNCS([WMGetScaleBaseFromSystemFont WMCreateBlendedPixmapFromRImage])
LIBS="$save_LIBS"

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AC_MSG_RESULT([
  wmapps $VERSION
    prefix:       $prefix
    CC:           $CC
    WINGs:        $WINGS_LIBS
    gio:          $GIO_LIBS
    gdk-pixbuf:   $GDKPIXBUF_LIBS
])
