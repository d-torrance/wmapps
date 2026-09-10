#!/usr/bin/env python3
"""Generate wmapps' application icon: Amanda, the Window Maker panda, with a
magnifying glass.

Window Maker icons are 48x48 with a transparent background, so the dock tile's
bevel shows around the artwork, and are drawn with hard 1px outlines and no
antialiasing.  This draws to a palette-indexed grid and emits XPM directly,
which preserves that crispness; scaling or antialiasing a vector source would
lose it.

Writes wmapps.xpm (installed where Window Maker looks for icons) and
wmapps_icon.h (the same image compiled into the binary, so the appicon is
correct even when nothing has been installed).
"""

import math

W = H = 48
TRANSPARENT = ' '

PALETTE = [
    (' ', None),
    # Outlines and Amanda's black markings.  Her "black" is a warm charcoal
    # rather than pure black, which is both truer to the animal and leaves
    # room for a lifted tone above it.
    ('k', '#0B0B0F'),    # outlines, darkest
    ('K', '#16161C'),     ('J', '#2E2E38'),   # marking base, lit side
    # Fur, lit from the upper left.
    ('W', '#FFFFFF'),     ('F', '#F2F2F4'),
    ('S', '#D6D6DC'),     ('T', '#B2B2BC'),
    # Lens rim, a bevelled metal ring.
    ('L', '#F0F0F6'),     ('l', '#D4D4DC'),
    ('m', '#8C8C96'),     ('d', '#46464E'),
    # Glass.
    ('w', '#F4F8FC'),     ('C', '#C6DDF4'),
    ('c', '#A8C8E8'),     ('b', '#7FA6CE'),
]

grid = [[TRANSPARENT] * W for _ in range(H)]


def put(x, y, ch):
    if 0 <= x < W and 0 <= y < H:
        grid[y][x] = ch


def rect(x0, y0, x1, y1, ch):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            put(x, y, ch)


def ellipse(cx, cy, rx, ry, fill, outline=None):
    """Filled ellipse, optionally with a 1px outline just inside the edge."""
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            n = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if n <= 1.0:
                put(x, y, outline if (outline and n > 0.62) else fill)


def disc(cx, cy, r, ch):
    ellipse(cx, cy, r, r, ch)


def outline_silhouette(ch):
    """Draw a 1px outline around everything drawn so far.

    Deriving the edge from the silhouette keeps the line exactly one pixel
    thick all the way round; thresholding on a normalized radius makes it
    thicken wherever the curve runs shallow.
    """
    edge = []
    for y in range(H):
        for x in range(W):
            if grid[y][x] == TRANSPARENT:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < W and 0 <= ny < H) or grid[ny][nx] == TRANSPARENT:
                    edge.append((x, y))
                    break
    for x, y in edge:
        put(x, y, ch)


def ellipse_outline(cx, cy, rx, ry, ch):
    """Draw the ellipse's own 1px boundary.

    Distinct from outline_silhouette(), which traces the outer edge of
    everything drawn.  This one draws the head's edge wherever it runs,
    including across the ears behind it, so the ears read as being attached
    behind the head rather than stuck on the front of it.
    """
    def inside(x, y):
        return ((x - cx) / float(rx)) ** 2 + ((y - cy) / float(ry)) ** 2 <= 1.0

    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            if not inside(x, y):
                continue
            if not all(inside(x + dx, y + dy)
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                put(x, y, ch)


def ring(cx, cy, outer, inner):
    """A bevelled lens rim, lit from the upper left."""
    for y in range(cy - outer - 1, cy + outer + 2):
        for x in range(cx - outer - 1, cx + outer + 2):
            dx, dy = x - cx, y - cy
            d = math.hypot(dx, dy)
            if d > outer + 0.5 or d < inner - 0.5:
                continue
            if d > outer - 1.0 or d < inner + 1.0:
                put(x, y, 'k')
                continue
            a = math.atan2(dy, dx) - math.radians(-135)
            a = (a + math.pi) % (2 * math.pi) - math.pi
            if abs(a) < math.radians(28):
                put(x, y, 'L')
            elif abs(a) < math.radians(70):
                put(x, y, 'l')
            elif abs(a) > math.radians(120):
                put(x, y, 'd')
            else:
                put(x, y, 'm')


# --- Amanda ------------------------------------------------------------------
# A big head is what makes her recognizable at 48px, so there is no room for a
# body or a drawn paw; the glass emerges from beside her, which implies the paw
# without spending pixels on it.
HX, HY = 17, 21
HRX, HRY = 14, 14


def gnustep_ear(cx, cy, r):
    """An ear carrying the GNUstep logo: a disc split by a stepped diagonal,
    dark on the upper left and light on the lower right.

    Amanda's ears carry this mark, and the staircase is the whole point of the
    logo, so it is drawn as literal steps rather than approximated with a
    diagonal.  Three risers is the fewest that still reads as a staircase and
    the most that fits across an 11px opening.  The ears are drawn over the
    head, not behind it, because the light half of the mark would otherwise
    merge into the white of her face and the boundary would vanish.
    """
    disc(cx, cy, r, 'k')

    inset = r - 1
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if dx * dx + dy * dy > inset * inset:
                continue                     # leaves a dark rim as the outline
            if dx >= 2:
                lit = True
            elif dx >= -1:
                lit = dy >= -1
            elif dx >= -4:
                lit = dy >= 2
            else:
                lit = False

            # Same upper-left light source as everything else, so the ears sit
            # in the same lighting as the head rather than reading as decals.
            u = (dx + dy) / float(r)
            if lit:
                put(cx + dx, cy + dy, 'W' if u < -0.3 else ('F' if u < 0.7 else 'S'))
            else:
                put(cx + dx, cy + dy, 'J' if u < -0.5 else 'K')


# Ears first: the head is drawn over them so its edge cuts across.  They sit
# far enough in to overlap the head, or they look like they are floating.
gnustep_ear(7,  8, 6)
gnustep_ear(27, 8, 6)

ellipse(HX, HY, HRX, HRY, 'F')
for y in range(H):
    for x in range(W):
        if grid[y][x] != 'F':
            continue
        dx, dy = x - HX, y - HY
        n = (dx / float(HRX)) ** 2 + (dy / float(HRY)) ** 2
        # u runs along the light direction: negative toward the upper left.
        u = dx / float(HRX) + dy / float(HRY)
        if u > 0.85:
            put(x, y, 'T')
        elif u > 0.30:
            put(x, y, 'S')
        elif u < -0.30:
            put(x, y, 'W')

outline_silhouette('k')                      # outer edge of the whole shape
ellipse_outline(HX, HY, HRX, HRY, 'k')       # head's edge, over the ears

# Eye patches: tilted ovals, the hallmark of the face.
for sx, tilt in ((-7, -1), (7, 1)):
    for y in range(-8, 9):
        for x in range(-6, 7):
            xr = x - tilt * y * 0.28
            if (xr / 4.4) ** 2 + (y / 6.0) ** 2 <= 1.0:
                u = (x + y) / 6.0
                put(HX + sx + x, HY - 2 + y, 'J' if u < -0.55 else 'K')

for sx in (-6, 6):
    disc(HX + sx, HY - 3, 2, 'W')
    put(HX + sx, HY - 3, 'k')
    put(HX + sx + 1, HY - 2, 'S')            # catchlight offset from the pupil

# Muzzle: pale snout, solid nose, and a smile under it.  The ends of the mouth
# have to sit *above* its middle -- ends below the middle draw a frown, which
# is what made an earlier version look miserable.
ellipse(HX, HY + 7, 7, 5, 'W')
ellipse(HX, HY + 9, 6, 3, 'F')
ellipse(HX + 2, HY + 10, 4, 2, 'S')
ellipse(HX, HY + 5, 3, 2, 'K')
put(HX - 1, HY + 4, 'J')
put(HX, HY + 7, 'k')
for dx in range(-4, 5):
    put(HX + dx, HY + 8 + (1 if abs(dx) <= 1 else 0), 'k')

# --- the magnifying glass ----------------------------------------------------
# Orientation is the whole trick, and it took a few tries.  The handle end sits
# against Amanda and the lens angles up and away from her.  Aim the lens at her
# face and it reads as someone else inspecting her; run the handle down away
# from her mouth and it reads as a cigarette.
#
# A true 45 degree shaft: one pixel across for every pixel up, which is also
# the one diagonal that renders cleanly without antialiasing.  Each step's
# centre satisfies x + y == CX + CY, so the axis passes through the middle of
# the lens; one step off that line reads as the handle meeting the rim at a
# tangent instead of pointing at its centre.  Its length is kept close to the
# lens diameter, which is how a real magnifier is proportioned.
CX, CY, OUTER, INNER = 39, 26, 8, 5

for step in range(0, 13):
    x = 33 - step
    y = 30 + step
    rect(x, y, x + 2, y + 2, 'k')
    if 1 <= step <= 11:
        put(x, y, 'l')                       # lit along the upper left edge
        put(x + 1, y + 1, 'm')
        put(x + 2, y + 2, 'd')               # shadowed along the lower right
disc(22, 43, 1, 'k')                         # rounded cap on the handle end

# Glass shaded along the same light direction: a bright crescent at the upper
# left falling away to a cool shadow at the lower right, which is what stops it
# reading as a flat blue disc.
for y in range(CY - INNER, CY + INNER + 1):
    for x in range(CX - INNER, CX + INNER + 1):
        dx, dy = x - CX, y - CY
        if dx * dx + dy * dy > INNER * INNER:
            continue
        u = (dx + dy) / float(INNER)
        if u < -0.85:
            put(x, y, 'w')
        elif u < -0.15:
            put(x, y, 'C')
        elif u < 0.75:
            put(x, y, 'c')
        else:
            put(x, y, 'b')

ring(CX, CY, OUTER, INNER)

# --- emit --------------------------------------------------------------------
used = {ch for row in grid for ch in row}
palette = [(c, v) for c, v in PALETTE if c in used]
rows = [''.join(r) for r in grid]
colors = ['"%s c %s"' % (c, v if v else 'None') for c, v in palette]

with open('wmapps.xpm', 'w') as f:
    f.write('/* XPM */\n')
    f.write('static char *wmapps[] = {\n')
    f.write('"%d %d %d 1",\n' % (W, H, len(palette)))
    f.write(',\n'.join(colors) + ',\n')
    f.write(',\n'.join('"%s"' % r for r in rows))
    f.write('\n};\n')

with open('wmapps_icon.h', 'w') as f:
    f.write('/* Generated by icon/make-icon.py -- do not edit by hand. */\n\n')
    f.write('#ifndef WMAPPS_ICON_H\n#define WMAPPS_ICON_H\n\n')
    f.write('static char *wmapps_icon_xpm[] = {\n')
    f.write('\t"%d %d %d 1",\n' % (W, H, len(palette)))
    f.write(''.join('\t%s,\n' % c for c in colors))
    f.write(''.join('\t"%s",\n' % r for r in rows))
    f.write('};\n\n#endif\n')

print('%d colours, %dx%d' % (len(palette), W, H))
