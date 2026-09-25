ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = wmapps

wmapps_SOURCES = main.c appdb.c appdb.h icons.c icons.h ui.c ui.h \
                 icon/wmapps_icon.h

AM_CPPFLAGS = $(WINGS_CFLAGS) $(GIO_CFLAGS) $(GDKPIXBUF_CFLAGS) $(X11_CFLAGS)
AM_CFLAGS   = -Wall -Wextra -Wno-unused-parameter
wmapps_LDADD = $(WINGS_LIBS) $(GIO_LIBS) $(GDKPIXBUF_LIBS) $(X11_LIBS)

# Advertise wmapps to the desktop-file index, which includes our own: with this
# installed, wmapps turns up in its own list.
desktopdir = $(datadir)/applications
dist_desktop_DATA = wmapps.desktop

# Window Maker looks for appicon and Dock images here.
icondir = $(datadir)/WindowMaker/Icons
dist_icon_DATA = icon/wmapps.png

# The same image again, where the XDG icon theme spec expects it, so that
# Icon=wmapps resolves for us and for any other launcher.
xdgicondir = $(datadir)/icons/hicolor/48x48/apps
xdgicon_DATA = icon/wmapps.png

EXTRA_DIST = autogen.sh README.md icon/make-icon.py
