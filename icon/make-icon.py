#!/usr/bin/env python3
"""Generate wmapps' application icon: Amanda, the Window Maker panda, with a
magnifying glass.

The artwork is drawn as vector shapes with cairo at the final 48x48 size, so
every edge is antialiased and every surface carries a gradient rather than a
few flat bands.  Lighting is from the upper left throughout.  Soft edges and
the translucent glass need real alpha, which XPM cannot express, so the
output is RGBA:

  wmapps.png      installed where Window Maker and the XDG icon theme look
  wmapps_icon.h   the same pixels compiled into the binary, so the appicon is
                  right even when nothing has been installed

Pass --preview FILE to also write an 8x nearest-neighbour enlargement over a
dock-tile grey, which is the easiest way to judge individual pixels.

Requires pycairo.
"""

import math
import struct
import sys
import zlib

import cairo

W = H = 48
TAU = 2 * math.pi


def rgb(hexstr, a=1.0):
    h = hexstr.lstrip('#')
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)) + (a,)


def linear(x0, y0, x1, y1, *stops):
    g = cairo.LinearGradient(x0, y0, x1, y1)
    for off, col in stops:
        g.add_color_stop_rgba(off, *col)
    return g


def radial(cx0, cy0, r0, cx1, cy1, r1, *stops):
    g = cairo.RadialGradient(cx0, cy0, r0, cx1, cy1, r1)
    for off, col in stops:
        g.add_color_stop_rgba(off, *col)
    return g


def ellipse_path(cr, cx, cy, rx, ry, angle=0.0):
    cr.save()
    cr.translate(cx, cy)
    cr.rotate(angle)
    cr.scale(rx, ry)
    cr.new_sub_path()
    cr.arc(0, 0, 1, 0, TAU)
    cr.restore()


def fill_ellipse(cr, cx, cy, rx, ry, src, angle=0.0):
    ellipse_path(cr, cx, cy, rx, ry, angle)
    cr.set_source(src) if isinstance(src, cairo.Pattern) else cr.set_source_rgba(*src)
    cr.fill()


def stroke_ellipse(cr, cx, cy, rx, ry, col, width, angle=0.0):
    ellipse_path(cr, cx, cy, rx, ry, angle)
    cr.set_source_rgba(*col)
    cr.set_line_width(width)
    cr.stroke()


# --- palette -----------------------------------------------------------------
# Amanda is black and white, which on its own gives a grey icon.  The colour
# comes from tinting rather than recolouring her: fur shades toward a cool
# blue-grey and lights toward cream, her black is a warm charcoal with a
# violet lift, and the props -- brass rim, lacquered red handle, sky-blue
# glass -- carry the saturated accents.
INK = rgb('#15121c')                 # outlines
CHAR_LIT, CHAR, CHAR_DARK = rgb('#5a5268'), rgb('#2c2733'), rgb('#141118')
FUR_LIT, FUR, FUR_MID, FUR_SHADE = (rgb('#fffdf6'), rgb('#f4f0e6'),
                                    rgb('#d9d8de'), rgb('#9fa6ba'))
BLUSH = rgb('#f28aa0', 0.45)
NOSE_LIT = rgb('#6d5d74')


def draw(cr):
    ears(cr)
    head(cr)
    face(cr)
    magnifier(cr)


# --- ears ----------------------------------------------------------------------
def gnustep_mark(cr, cx, cy, r):
    """The GNUstep logo on an ear: a disc split by a staircase, dark upper
    left and light lower right.  The steps are the point of the logo, so they
    are drawn as literal risers rather than smoothed into a diagonal."""
    s = r / 5.5
    cr.save()
    ellipse_path(cr, cx, cy, r, r)
    cr.clip()

    # Dark half: a soft sheen at the top left, falling to near-black.
    cr.set_source(radial(cx - 2.5 * s, cy - 3 * s, 0.5, cx, cy, r * 1.3,
                         (0, CHAR_LIT), (0.45, CHAR), (1, CHAR_DARK)))
    cr.paint()

    # Light half: the lower right, bounded by three steps.
    cr.move_to(cx + r + 2, cy - r - 2)
    cr.line_to(cx + 1.6 * s, cy - r - 2)
    cr.line_to(cx + 1.6 * s, cy - 1.4 * s)
    cr.line_to(cx - 1.2 * s, cy - 1.4 * s)
    cr.line_to(cx - 1.2 * s, cy + 1.4 * s)
    cr.line_to(cx - 4.0 * s, cy + 1.4 * s)
    cr.line_to(cx - 4.0 * s, cy + r + 2)
    cr.line_to(cx + r + 2, cy + r + 2)
    cr.close_path()
    cr.set_source(linear(cx - r, cy - r, cx + r, cy + r,
                         (0.2, FUR_LIT), (0.65, FUR), (1, FUR_SHADE)))
    cr.fill()
    cr.restore()

    stroke_ellipse(cr, cx, cy, r - 0.5, r - 0.5, INK, 1.1)


def ears(cr):
    gnustep_mark(cr, 7.8, 8.6, 6.2)
    gnustep_mark(cr, 29.6, 8.6, 6.2)


# --- head ----------------------------------------------------------------------
HX, HY, HRX, HRY = 18.7, 21.5, 14.6, 13.6


def head(cr):
    # Contact shadow where the ears go behind the head.
    for ex in (7.8, 29.6):
        fill_ellipse(cr, ex + (1 if ex < HX else -1), 11.5, 4.5, 2.5,
                     rgb('#000000', 0.18))

    # Fur: warm highlight at the upper left rolling into a cool blue-grey
    # terminator at the lower right.
    fill_ellipse(cr, HX, HY, HRX, HRY,
                 radial(HX - 6, HY - 7, 1, HX - 1, HY - 1, HRX * 1.25,
                        (0, FUR_LIT), (0.45, FUR), (0.78, FUR_MID), (1, FUR_SHADE)))

    # Reflected light along the lower right rim, so the shadow side doesn't
    # die into the outline.
    cr.save()
    ellipse_path(cr, HX, HY, HRX, HRY)
    cr.clip()
    stroke_ellipse(cr, HX + 0.8, HY + 0.9, HRX - 0.6, HRY - 0.6,
                   rgb('#c8d3ea', 0.55), 1.2)
    cr.restore()

    stroke_ellipse(cr, HX, HY, HRX - 0.5, HRY - 0.5, INK, 1.1)


# --- face ----------------------------------------------------------------------
def eye(cr, cx, cy, tilt, look):
    # Patch: a tilted teardrop oval, the hallmark of a panda face.
    fill_ellipse(cr, cx, cy, 4.3, 5.7,
                 radial(cx - 2, cy - 3, 0.3, cx, cy, 6.5,
                        (0, CHAR_LIT), (0.5, CHAR), (1, CHAR_DARK)), tilt)

    # The eye itself: glossy and slightly oversized, like Wilber's, because at
    # this size an eye that reads is worth more than a realistic one.
    ex, ey = cx + look, cy - 0.8
    fill_ellipse(cr, ex, ey, 2.7, 2.9,
                 radial(ex - 1, ey - 1.2, 0.2, ex, ey, 3.0,
                        (0, rgb('#ffffff')), (0.7, rgb('#e6e8f0')), (1, rgb('#9aa0b8'))))
    px, py = ex + 0.9, ey + 0.4
    fill_ellipse(cr, px, py, 1.55, 1.75,
                 radial(px - 0.4, py - 0.5, 0.1, px, py, 1.8,
                        (0, rgb('#4a3a5c')), (0.6, rgb('#1c1426')), (1, rgb('#0a0710'))))
    fill_ellipse(cr, px - 0.6, py - 0.7, 0.62, 0.62, rgb('#ffffff'))
    fill_ellipse(cr, px + 0.6, py + 0.8, 0.3, 0.3, rgb('#ffffff', 0.6))


def face(cr):
    eye(cr, HX - 7.0, HY - 1.2, 0.5, 0.0)
    eye(cr, HX + 7.0, HY - 1.2, -0.5, 0.5)

    # Blush under each patch -- colour in a black-and-white animal, and it
    # makes her friendlier.
    for sx in (-9.5, 9.8):
        fill_ellipse(cr, HX + sx, HY + 6.3, 2.4, 1.4,
                     radial(HX + sx, HY + 6.3, 0, HX + sx, HY + 6.3, 2.4,
                            (0, BLUSH), (1, rgb('#f28aa0', 0.0))))

    # Muzzle: a pale, slightly raised snout.
    mx, my = HX, HY + 7.4
    fill_ellipse(cr, mx + 0.5, my + 0.8, 6.6, 4.5, rgb('#8e94aa', 0.35))
    fill_ellipse(cr, mx, my, 6.4, 4.4,
                 radial(mx - 2, my - 2, 0.5, mx, my, 6.6,
                        (0, FUR_LIT), (0.7, rgb('#f6f1e8')), (1, rgb('#d6d4dc'))))

    # Nose: a rounded triangle with a glossy highlight.
    nx, ny = mx, my - 2.0
    cr.move_to(nx - 2.9, ny - 1.1)
    cr.curve_to(nx - 2.9, ny - 2.4, nx + 2.9, ny - 2.4, nx + 2.9, ny - 1.1)
    cr.curve_to(nx + 2.9, ny + 0.2, nx + 0.9, ny + 1.8, nx, ny + 1.8)
    cr.curve_to(nx - 0.9, ny + 1.8, nx - 2.9, ny + 0.2, nx - 2.9, ny - 1.1)
    cr.close_path()
    cr.set_source(radial(nx - 0.8, ny - 1.4, 0.2, nx, ny - 0.3, 3.2,
                         (0, NOSE_LIT), (0.5, CHAR), (1, CHAR_DARK)))
    cr.fill()
    fill_ellipse(cr, nx - 0.9, ny - 1.2, 0.9, 0.5, rgb('#ffffff', 0.7), -0.2)

    # Mouth: the corners sit above the middle, or it reads as a frown.
    cr.set_line_cap(cairo.LINE_CAP_ROUND)
    cr.set_line_width(0.9)
    cr.set_source_rgba(*INK)
    cr.move_to(nx, ny + 1.6)
    cr.line_to(nx, ny + 2.6)
    cr.move_to(nx - 3.2, ny + 2.4)
    cr.curve_to(nx - 2.2, ny + 3.8, nx - 0.6, ny + 3.6, nx, ny + 2.6)
    cr.curve_to(nx + 0.6, ny + 3.6, nx + 2.2, ny + 3.8, nx + 3.2, ny + 2.4)
    cr.stroke()


# --- the magnifying glass --------------------------------------------------------
# The handle end sits against Amanda and the lens angles up and away from her.
# Aimed at her face it reads as someone else inspecting her; run down away from
# her mouth it reads as a cigarette.  The handle's axis passes through the lens
# centre, or it looks like it meets the rim at a tangent.
LX, LY, LR = 37.6, 28.4, 8.2


def magnifier(cr):
    ang = math.radians(135)                  # from the lens toward the handle
    ux, uy = math.cos(ang), math.sin(ang)

    def along(d):
        return LX + ux * d, LY + uy * d

    # Handle, drawn in its own frame: x runs along the shaft.
    cr.save()
    cr.translate(LX, LY)
    cr.rotate(ang)

    # Brass ferrule joining the rim.
    cr.rectangle(LR - 0.5, -1.6, 3.6, 3.2)
    cr.set_source(linear(0, -1.6, 0, 1.6, (0, rgb('#fff1b0')), (0.35, rgb('#e8b640')),
                         (1, rgb('#7a4e10'))))
    cr.fill_preserve()
    cr.set_source_rgba(*INK)
    cr.set_line_width(0.8)
    cr.stroke()

    # Lacquered red grip, tapering to a rounded end.
    x0, x1 = LR + 3.0, LR + 12.6
    cr.move_to(x0, -2.1)
    cr.line_to(x1, -2.5)
    cr.arc(x1, 0, 2.5, -math.pi / 2, math.pi / 2)
    cr.line_to(x0, 2.1)
    cr.close_path()
    cr.set_source(linear(0, -2.5, 0, 2.5, (0, rgb('#ff9a7a')), (0.3, rgb('#e0352b')),
                         (0.75, rgb('#9c1620')), (1, rgb('#5a0a14'))))
    cr.fill_preserve()
    cr.set_source_rgba(*INK)
    cr.set_line_width(0.9)
    cr.stroke()
    # Specular streak along the lit edge.
    cr.move_to(x0 + 1, -1.2)
    cr.line_to(x1 - 0.5, -1.45)
    cr.set_source_rgba(1, 1, 1, 0.55)
    cr.set_line_width(0.7)
    cr.stroke()
    cr.restore()

    # Glass: sky blue, deeper toward the lower right, with Amanda's fur faintly
    # magnified behind it so it reads as a lens rather than a blue disc.
    fill_ellipse(cr, LX, LY, LR - 1.2, LR - 1.2,
                 radial(LX - 3, LY - 3, 0.5, LX, LY, LR,
                        (0, rgb('#eef8ff', 0.92)), (0.45, rgb('#9fd0f5', 0.85)),
                        (0.85, rgb('#4f93d6', 0.85)), (1, rgb('#2e5f9e', 0.9))))

    # Reflections: a curved window glint at the upper left and a small
    # counter-glint low on the right.
    cr.save()
    ellipse_path(cr, LX, LY, LR - 1.2, LR - 1.2)
    cr.clip()
    cr.new_path()
    cr.arc(LX, LY, LR - 2.8, math.radians(190), math.radians(260))
    cr.arc_negative(LX + 0.8, LY + 0.8, LR - 3.0, math.radians(255), math.radians(195))
    cr.close_path()
    cr.set_source_rgba(1, 1, 1, 0.9)
    cr.fill()
    fill_ellipse(cr, LX - 3.6, LY - 4.4, 0.9, 0.9, rgb('#ffffff', 0.95))
    cr.new_path()
    cr.arc(LX, LY, LR - 2.6, math.radians(20), math.radians(60))
    cr.set_source_rgba(1, 1, 1, 0.45)
    cr.set_line_width(0.9)
    cr.set_line_cap(cairo.LINE_CAP_ROUND)
    cr.stroke()
    cr.restore()

    # Brass rim: a conical sweep, bright where it faces the light.
    rim_w = 2.2
    cr.new_path()
    cr.arc(LX, LY, LR - rim_w / 2, 0, TAU)
    cr.set_line_width(rim_w)
    cr.set_source(linear(LX - LR, LY - LR, LX + LR, LY + LR,
                         (0, rgb('#fff6c8')), (0.3, rgb('#f0c24e')),
                         (0.6, rgb('#b27a1c')), (1, rgb('#5e3a08'))))
    cr.stroke()
    # Inner bevel catches light on the opposite side.
    cr.arc(LX, LY, LR - rim_w + 0.3, 0, TAU)
    cr.set_line_width(0.6)
    cr.set_source(linear(LX - LR, LY - LR, LX + LR, LY + LR,
                         (0, rgb('#6a4410', 0.9)), (1, rgb('#ffe590', 0.9))))
    cr.stroke()

    stroke_ellipse(cr, LX, LY, LR - 0.2, LR - 0.2, INK, 0.9)
    stroke_ellipse(cr, LX, LY, LR - rim_w - 0.1, LR - rim_w - 0.1, INK, 0.7)


# --- emit ----------------------------------------------------------------------
def render():
    surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, W, H)
    cr = cairo.Context(surf)
    draw(cr)
    surf.flush()

    # cairo stores premultiplied native-endian ARGB; un-premultiply to RGBA.
    data = bytes(surf.get_data())
    stride = surf.get_stride()
    out = []
    for y in range(H):
        row = []
        for x in range(W):
            b, g, r, a = data[y * stride + 4 * x: y * stride + 4 * x + 4] \
                if sys.byteorder == 'little' else \
                data[y * stride + 4 * x: y * stride + 4 * x + 4][::-1]
            if a:
                r, g, b = (min(255, (c * 255 + a // 2) // a) for c in (r, g, b))
            row.append((r, g, b, a))
        out.append(row)
    return out


def write_png(path, px, scale=1, bg=None):
    w, h = W * scale, H * scale
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            r, g, b, a = px[y // scale][x // scale]
            if bg:
                # Composite over the preview background, with a faint grid.
                base = bg if (x % scale and y % scale) or scale < 4 else \
                    tuple(max(0, c - 10) for c in bg)
                r, g, b = (int(c * a / 255 + base[i] * (255 - a) / 255)
                           for i, c in enumerate((r, g, b)))
                raw += bytes((r, g, b))
            else:
                raw += bytes((r, g, b, a))

    def chunk(tag, body):
        c = struct.pack('>I', len(body)) + tag + body
        return c + struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2 if bg else 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(bytes(raw), 9)))
        f.write(chunk(b'IEND', b''))


def write_header(path, px):
    with open(path, 'w') as f:
        f.write('/* Generated by icon/make-icon.py -- do not edit by hand. */\n\n')
        f.write('#ifndef WMAPPS_ICON_H\n#define WMAPPS_ICON_H\n\n')
        f.write('#define WMAPPS_ICON_WIDTH  %d\n#define WMAPPS_ICON_HEIGHT %d\n\n' % (W, H))
        f.write('/* Straight (not premultiplied) RGBA, row-major. */\n')
        f.write('static const unsigned char wmapps_icon_rgba[] = {\n')
        for row in px:
            flat = [c for p in row for c in p]
            for i in range(0, len(flat), 16):
                f.write('\t' + ', '.join('0x%02x' % c for c in flat[i:i + 16]) + ',\n')
        f.write('};\n\n#endif\n')


def main():
    px = render()
    write_png('wmapps.png', px)
    write_header('wmapps_icon.h', px)
    if len(sys.argv) == 3 and sys.argv[1] == '--preview':
        write_png(sys.argv[2], px, scale=8, bg=(0x7a, 0x7a, 0x8a))
    colours = {p for row in px for p in row if p[3]}
    print('%d colours, %dx%d' % (len(colours), W, H))


if __name__ == '__main__':
    main()
