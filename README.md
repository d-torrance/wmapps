# wmapps

An application launcher for Window Maker.

Window Maker's root menu is a hand-maintained static tree, and `Mod+F2` is a
bare run dialog that knows nothing about what is installed. Meanwhile every
`.desktop` file on the system is already indexed by GIO, which exposes the same
relevance-ranked search GNOME Shell's overview uses.

`wmapps` puts the two together: type-to-filter application search with the
behaviour of a modern launcher, drawn with WINGs widgets so it looks like it
belongs on the desktop it runs on.

## Behaviour

Opening the launcher lists every installed application alphabetically. Typing
switches to a relevance-ranked search over names, generic names, keywords,
comments and executables. This is deliberately what the GNOME Shell overview
does -- `g_desktop_app_info_search()` returns nothing at all for an empty
query, so browsing fills the gap rather than presenting an empty window.

| Key | Action |
|---|---|
| any text | filter |
| Up / Down | move the selection |
| Page Up / Page Down | move a screenful |
| Return | launch the selected application and exit |
| Escape | quit |

Double-clicking a row launches it. Home and End are left to the text field for
cursor movement.

## Building

Requires libWINGs (Window Maker), glib/GIO 2.40 or newer, gdk-pixbuf and Xlib.

    ./autogen.sh
    ./configure
    make
    sudo make install

`configure` reports what it found and fails early with a clear message if a
dependency is missing. Two WINGs entry points are probed rather than assumed
(`WMGetScaleBaseFromSystemFont` for HiDPI scaling and
`WMCreateBlendedPixmapFromRImage` for antialiased icons); both have fallbacks
for older releases.

Install from the repository rather than a tarball and you need autoconf,
automake and pkg-config; `autogen.sh` generates the rest.

## Using it from Window Maker

`wmapps` is a one-shot program: it starts, launches something, and exits. There
is no daemon and no keyboard grab, because Window Maker's own root menu already
provides both a menu entry and a shortcut. Add this to
`~/GNUstep/Defaults/WMRootMenu`:

    (Applications..., SHORTCUT, "Mod4+space", EXEC, wmapps)

Then reload the menu from the Window Maker menu, or restart it.

## The Dock

`wmapps` supplies its own application icon -- Amanda, the Window Maker panda,
holding a magnifying glass, with the GNUstep logo's stepped disc marking each
ear as it does on the mascot -- so its appicon shows that rather than Window
Maker's default tile, and it can be dragged from the appicon straight into the
Dock. Nothing else is needed: on docking,
Window Maker caches the client-supplied icon into
`~/GNUstep/Library/WindowMaker/CachedPixmaps/wmapps.Wmapps.xpm` and records it
in `WMWindowAttributes` itself, so the icon is still shown when the docked
application is not running.

Note that Window Maker identifies an application by the `WM_CLASS` of its group
leader window, not of its visible window, and WINGs labels that leader
`("groupLeader", appname)` by default. `wmapps` relabels it so it is filed as
`wmapps.Wmapps` like any other application rather than as `groupLeader.wmapps`.

The icon file is also installed into `$(datadir)/WindowMaker/Icons`, which is
useful for referring to it by name from configuration, but is not required for
the Dock to work.

The icon is generated rather than hand-drawn:

    cd icon && python3 make-icon.py

That writes both `wmapps.png` (installed for Window Maker) and `wmapps_icon.h`
(the same RGBA pixels compiled into the binary, so the appicon is correct even
when nothing has been installed). Editing the script and re-running it is the
supported way to change the artwork. It draws vector shapes with pycairo at the
final 48x48 size, so edges are antialiased and surfaces are shaded with
gradients; the soft edges and translucent glass need real alpha, which is why the
output is PNG rather than XPM. `python3 make-icon.py --preview FILE` also writes
an 8x enlargement over a dock-tile grey for judging individual pixels. The
script's comments record why the fiddly bits are the way they are -- the
handle's angle and alignment, the literal staircase in the ears.

## Installing as a desktop application

`make install` also installs a desktop entry and an icon on the XDG paths, so
`wmapps` is advertised to the desktop-file index like any other application --
which includes its own, so it turns up in its own list:

    $(datadir)/applications/wmapps.desktop
    $(datadir)/icons/hicolor/48x48/apps/wmapps.png

One thing to know if the entry ever seems to be ignored: GIO refuses to load a
desktop entry whose `Exec` program it cannot resolve, and this entry uses a
bare `Exec=wmapps`. So the binary has to be on `PATH` before the entry becomes
visible -- installing to a prefix whose `bin` is not on `PATH` will leave the
entry silently inert rather than reporting an error.

## Options

    -t, --icon-theme NAME  use this icon theme instead of the configured one
        --print [QUERY]    print matching applications and exit
        --print-icons      print each application's resolved icon file and exit
    -h, --help             show usage
    -V, --version          show version

`--print` and `--print-icons` exist for checking the search and icon lookup
against what GIO itself reports, without having to read the window. The icon
theme can also be set with `$WMAPPS_ICON_THEME`.

## Notes

Icons are resolved through the XDG icon theme specification: the configured
theme, then its `Inherits` chain, then `hicolor`, then the legacy flat
directories. `index.theme` is parsed for each theme's directory list rather than
globbing the filesystem, which matters more than it sounds -- the generic
fallback icon lives under `mimetypes/` rather than `apps/`, and `@2x` directories
carry a scale factor that must not be read as a pixel size. A single theme is
routinely split across several base directories, so all of them are searched for
every theme.

Icons load lazily, when a row is first drawn, so opening the launcher does not
stall while a hundred image files are read.
