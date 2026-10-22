#!/usr/bin/env python3
"""Draws the controls card's pictures (ui/NativeControlsCard.h) and writes them
as DXT5 DDS under assets/textures/Menus/OBVR/, the game's Data\\Textures\\Menus\\OBVR
(the case of Menus as the game's own folder has it, for Linux file systems).

Five pages, each drawn for a right-hander and mirrored for a left-hander:

  page 1  the weapon hand's controller: trigger, grip, stick, A, B
  page 2  the places weapons are drawn from, the body seen from behind, with
          the weapons in them
  page 3  the other hand's controller (A / X and B / Y: Quest's left names)
  page 4  holding: a potion brought to the chest's gold ring, a chest reached into
  page 5  both controllers: the stick clicks for the two rings, both for settings

Our own drawings in the manner of the game's parchment: sepia ink with a
pen's swelling line, the shadow side hatched as an engraving, a thin wash
under it and the game's gold for what to reach - no vendor artwork: a generic
controller (a head with a stick and two buttons, a trigger under it, a grip on
the handle) that reads for both Index and Quest. The places on page 2 are the
places' defaults (vr/Holster.h, vr/Archery.h); a side chosen in the settings
is not drawn. Drawn four times over and scaled down, for clean lines on the
card's 640 x 320 pixels. The labels are Noto Serif Display (SIL Open Font
License). The higher-quality pass of 2026-10-22 (Nadi: "mach die mal
qualitativ höherwertig und behalte den oblivion style bei").

Run:    python3 tools/make-controls-drawings.py [--preview DIR]
Needs:  Pillow and ImageMagick (magick) for the DDS.
"""

import argparse
import math
import random
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "textures" / "Menus" / "OBVR"
W, H = 1024, 512
SS = 4  # drawn at four times the size, scaled down at the end

INK = (58, 36, 20)
WASH = (196, 160, 104)
SHADE = (120, 84, 46)
GOLD = (201, 152, 44)
GOLD_DARK = (128, 88, 22)
POTION = (150, 38, 30)
FONT = "/usr/share/fonts/noto/NotoSerifDisplay-Bold.ttf"
FONT_ITALIC = "/usr/share/fonts/noto/NotoSerifDisplay-Italic.ttf"
LIGHT = (-0.55, -0.83)  # towards the light, the upper left; the shadow side is the other
PAPER = (226, 206, 160, 255)  # the preview's parchment only

PAGES = ["weapon_buttons", "weapon_places", "other_buttons", "holding", "both_hands"]


def font(size, italic=False):
    return ImageFont.truetype(FONT_ITALIC if italic else FONT, int(size * SS))


def catmull(points, closed, steps=10):
    """A smooth curve through the points."""
    m = len(points)
    out = []
    spans = range(m) if closed else range(m - 1)
    for i in spans:
        p0 = points[(i - 1) % m] if closed else points[max(i - 1, 0)]
        p1 = points[i]
        p2 = points[(i + 1) % m]
        p3 = points[(i + 2) % m] if closed else points[min(i + 2, m - 1)]
        for k in range(steps):
            t = k / steps
            t2, t3 = t * t, t * t * t
            out.append(tuple(
                0.5 * (2 * p1[j] + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t2 +
                       (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t3) for j in (0, 1)))
    if not closed:
        out.append(points[-1])
    return out


def straight(points, closed, step=3.0):
    """The points joined by straight lines, sampled every `step`."""
    out = []
    m = len(points)
    for i in range(m if closed else m - 1):
        a, b = points[i], points[(i + 1) % m]
        n = max(1, int(math.dist(a, b) / step))
        out.extend((a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n) for k in range(n))
    if not closed:
        out.append(points[-1])
    return out


def ellipse_points(cx, cy, rx, ry, turn=0.0, n=20):
    c, s = math.cos(turn), math.sin(turn)
    pts = []
    for k in range(n):
        a = 2 * math.pi * k / n
        x, y = rx * math.cos(a), ry * math.sin(a)
        pts.append((cx + x * c - y * s, cy + x * s + y * c))
    return pts


def signed_area(pts):
    return 0.5 * sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1]
                     for i in range(len(pts)))


class Pen:
    """Draws in right-hander coordinates on the 1024 x 512 picture; `left`
    mirrors every shape across the picture's middle, while text stays
    readable. Three layers: the wash under everything, the hatching, the ink."""

    def __init__(self, left, seed):
        self.left = left
        self.rng = random.Random(seed)
        size = (W * SS, H * SS)
        self.wash = Image.new("RGBA", size, (0, 0, 0, 0))
        self.hatch = Image.new("RGBA", size, (0, 0, 0, 0))
        self.ink = Image.new("RGBA", size, (0, 0, 0, 0))
        self.di = ImageDraw.Draw(self.ink)
        self.patterns = {}

    # -- coordinates
    def x(self, x):
        return W - 1 - x if self.left else x

    def place_pts(self, points):
        return [(self.x(px) * SS, py * SS) for px, py in points]

    def wobble(self, pts, amount=0.7):
        """A hand's unsteadiness: a slow sideways drift along the line."""
        if amount <= 0 or len(pts) < 3:
            return pts
        f1, f2 = self.rng.uniform(0, 6.28), self.rng.uniform(0, 6.28)
        out = []
        s = 0.0
        for i, p in enumerate(pts):
            if i:
                s += math.dist(pts[i - 1], p)
            a = pts[max(i - 1, 0)]
            b = pts[min(i + 1, len(pts) - 1)]
            dx, dy = b[0] - a[0], b[1] - a[1]
            d = math.hypot(dx, dy) or 1.0
            off = amount * SS * (0.6 * math.sin(s / (38 * SS) + f1) + 0.4 * math.sin(s / (13 * SS) + f2))
            out.append((p[0] - dy / d * off, p[1] + dx / d * off))
        return out

    # -- the ink line
    def _stroke(self, pts, widths, colour=INK):
        fill = colour + (255,)
        for i in range(len(pts) - 1):
            (ax, ay), (bx, by) = pts[i], pts[i + 1]
            wa, wb = widths[i] / 2, widths[i + 1] / 2
            dx, dy = bx - ax, by - ay
            d = math.hypot(dx, dy)
            if d < 1e-6:
                continue
            nx, ny = -dy / d, dx / d
            self.di.polygon([(ax + nx * wa, ay + ny * wa), (bx + nx * wb, by + ny * wb),
                             (bx - nx * wb, by - ny * wb), (ax - nx * wa, ay - ny * wa)], fill=fill)
        for (px, py), w in zip(pts, widths):
            r = w / 2
            self.di.ellipse((px - r, py - r, px + r, py + r), fill=fill)

    def outline(self, pts, width, closed=True, colour=INK, wobble=0.7):
        """A pen line: heavier on the shadow side of a closed shape, tapered at
        the ends of an open one."""
        p = self.wobble(pts, wobble)
        widths = []
        if closed:
            p = p + [p[0]]
            orient = 1 if signed_area(p) > 0 else -1
            for i in range(len(p)):
                a, b = p[max(i - 1, 0)], p[min(i + 1, len(p) - 1)]
                dx, dy = b[0] - a[0], b[1] - a[1]
                d = math.hypot(dx, dy) or 1.0
                # The outward normal; the line swells where it faces away from the light.
                nx, ny = dy / d * orient, -dx / d * orient
                shade = max(0.0, -(nx * LIGHT[0] + ny * LIGHT[1]))
                widths.append(width * SS * (0.62 + 0.75 * shade))
        else:
            n = len(p)
            total = sum(math.dist(p[i], p[i + 1]) for i in range(n - 1)) or 1.0
            s = 0.0
            for i in range(n):
                if i:
                    s += math.dist(p[i - 1], p[i])
                t = s / total
                ends = min(1.0, t / 0.18, (1 - t) / 0.18)
                widths.append(width * SS * (0.35 + 0.65 * ends))
        self._stroke(p, widths, colour)

    # -- fills
    def _mask(self, pts):
        m = Image.new("L", self.ink.size, 0)
        ImageDraw.Draw(m).polygon(pts, fill=255)
        return m

    def _pattern(self, angle, spacing):
        key = (angle, spacing)
        if key not in self.patterns:
            pat = Image.new("L", self.ink.size, 0)
            d = ImageDraw.Draw(pat)
            w, h = self.ink.size
            sp = spacing * SS
            c, s = math.cos(angle), math.sin(angle)
            reach = w + h
            k = -reach
            while k < reach:
                ox, oy = -s * k, c * k  # lines along (c, s), stepped across them
                d.line([(w / 2 + ox - c * reach, h / 2 + oy - s * reach),
                        (w / 2 + ox + c * reach, h / 2 + oy + s * reach)], fill=255, width=max(1, int(1.15 * SS)))
                k += sp
            self.patterns[key] = pat
        return self.patterns[key]

    def shape(self, points, smooth=True, width=4.0, wash=WASH, wash_alpha=120, hatch=True, depth=14,
              ink=True, wobble=0.7):
        """A closed shape: its wash, its shadow side hatched, its pen outline."""
        design = catmull(points, True) if smooth else straight(points, True)
        pts = self.place_pts(design)
        mask = self._mask(pts)
        if wash is not None:
            soft = mask.filter(ImageFilter.GaussianBlur(0.8 * SS))
            layer = Image.new("RGBA", self.ink.size, wash + (0,))
            layer.putalpha(soft.point(lambda v: v * wash_alpha // 255))
            self.wash.alpha_composite(layer)
        if hatch:
            for share, angle, alpha in ((1.0, math.radians(-50), 170), (0.45, math.radians(35), 140)):
                off = depth * share * SS
                shifted = Image.new("L", mask.size, 0)
                shifted.paste(mask, (int(round(LIGHT[0] * off)), int(round(LIGHT[1] * off))))
                shadow = ImageChops.subtract(mask, shifted).filter(ImageFilter.GaussianBlur(1.2 * SS))
                lines = ImageChops.multiply(shadow, self._pattern(angle, 5.5))
                layer = Image.new("RGBA", self.ink.size, SHADE + (0,))
                layer.putalpha(lines.point(lambda v: v * alpha // 255))
                self.hatch.alpha_composite(layer)
                if share == 1.0:
                    tone = Image.new("RGBA", self.ink.size, SHADE + (0,))
                    tone.putalpha(shadow.point(lambda v: v * 45 // 255))
                    self.wash.alpha_composite(tone)
        if ink:
            self.outline(pts, width, closed=True, wobble=wobble)

    def circle(self, cx, cy, r, **kw):
        self.shape(ellipse_points(cx, cy, r, r, n=16), **kw)

    def line(self, points, width=3.0, smooth=True, colour=INK, wobble=0.5):
        design = catmull(points, False) if smooth and len(points) > 2 else straight(points, False)
        self.outline(self.place_pts(design), width, closed=False, colour=colour, wobble=wobble)

    def dot(self, x, y, r=5.0, colour=INK):
        px, py = self.x(x) * SS, y * SS
        self.di.ellipse((px - r * SS, py - r * SS, px + r * SS, py + r * SS), fill=colour + (255,))

    def arrow(self, points, width=4.5):
        """A curved arrow through the points, its head at the last."""
        self.line(points, width)
        design = catmull(points, False) if len(points) > 2 else straight(points, False)
        (ax, ay), (bx, by) = self.place_pts([design[-5], design[-1]])
        ang = math.atan2(by - ay, bx - ax)
        size = 24 * SS
        wing = [(bx + size * math.cos(ang + math.pi + s * 0.42), by + size * math.sin(ang + math.pi + s * 0.42))
                for s in (-1, 1)]
        back = (bx + 0.6 * size * math.cos(ang + math.pi), by + 0.6 * size * math.sin(ang + math.pi))
        self.di.polygon([(bx, by), wing[0], back, wing[1]], fill=INK + (255,))

    def gold_place(self, x, y, r=24):
        """Where to reach: the game's gold, a ring with a soft glow."""
        px, py = self.x(x) * SS, y * SS
        glow = Image.new("L", self.ink.size, 0)
        g = r * 1.5 * SS
        ImageDraw.Draw(glow).ellipse((px - g, py - g, px + g, py + g), fill=150)
        glow = glow.filter(ImageFilter.GaussianBlur(r * 0.5 * SS))
        layer = Image.new("RGBA", self.ink.size, GOLD + (0,))
        layer.putalpha(glow)
        self.wash.alpha_composite(layer)
        ring = ellipse_points(x, y, r, r, n=18)
        self.shape(ring, wash=GOLD, wash_alpha=90, hatch=False, ink=False)
        self.outline(self.place_pts(catmull(ring, True)), 5.0, closed=True, colour=GOLD_DARK, wobble=0.3)
        inner = self.place_pts(catmull(ellipse_points(x, y, r * 0.55, r * 0.55, n=14), True))
        self.outline(inner, 2.5, closed=True, colour=GOLD_DARK, wobble=0.2)

    # -- words
    def text(self, x, y, s, size=38, centre=False, italic=False):
        """Text at (x, y) in right-hander coordinates. A label that starts at x
        for a right-hander ends at the mirrored x for a left-hander."""
        f = font(size, italic)
        w = self.di.textlength(s, font=f) / SS
        if centre:
            px = self.x(x) - w / 2
        elif self.left:
            px = self.x(x) - w
        else:
            px = x
        self.di.text((px * SS, y * SS), s, font=f, fill=INK + (255,))
        return w

    def label(self, x, y, s, to, size=38):
        """A label at the picture's side with a leader line to the point `to`."""
        w = self.text(x, y, s, size)
        # The line leaves the label on the side towards its point, wherever the
        # mirror put the two.
        left_edge = self.x(x) - w if self.left else x
        towards_right = self.x(to[0]) > left_edge + w / 2
        sx = left_edge + w + 10 if towards_right else left_edge - 10
        start = (self.x(sx), y + size * 0.66)
        mid = ((start[0] + to[0]) / 2, (start[1] + to[1]) / 2 - 6)
        self.line([start, mid, to], width=2.6)
        self.dot(to[0], to[1], 6)

    def picture(self):
        """The layers put together and scaled down to the card's picture."""
        out = Image.new("RGBA", self.ink.size, (0, 0, 0, 0))
        out.alpha_composite(self.wash)
        out.alpha_composite(self.hatch)
        bleed = self.ink.filter(ImageFilter.GaussianBlur(1.6 * SS))
        bleed.putalpha(bleed.getchannel("A").point(lambda v: v * 70 // 255))
        out.alpha_composite(bleed)
        out.alpha_composite(self.ink)
        return out.resize((W, H), Image.LANCZOS)


def controller(p, cx, cy, s=1.0, buttons=("A", "B")):
    """A generic controller seen from above-behind, as held in the right hand:
    the head with the stick and two buttons, the trigger hooked under its
    front, the grip down the handle. Returns the feature points."""
    def at(dx, dy):
        return (cx + dx * s, cy + dy * s)

    # The trigger, under the head's front where the index finger lies.
    p.shape([at(-152, 14), at(-118, -4), at(-92, 40), at(-96, 92), at(-122, 112), at(-146, 78)],
            width=4 * s, wash=SHADE, wash_alpha=90, depth=9 * s)
    # The handle, curving back and down from under the head.
    p.shape([at(-52, 50), at(40, 55), at(60, 150), at(74, 268), at(60, 312), at(18, 322), at(-6, 300),
             at(-14, 200), at(-34, 110)], width=5 * s, depth=22 * s)
    # The grip on the handle's outer side, with its ribs.
    p.shape([at(44, 120), at(64, 126), at(76, 262), at(62, 290), at(50, 276), at(40, 170)],
            width=3.5 * s, wash=SHADE, wash_alpha=110, depth=8 * s)
    for k in range(6):
        y = 150 + k * 22
        p.line([at(46 + k * 1.5, y), at(66 + k * 2.0, y + 4)], width=1.8 * s, smooth=False)
    # The head and its face plate.
    p.shape(ellipse_points(cx, cy - 20 * s, 165 * s, 100 * s, math.radians(-6), n=22), width=5.5 * s,
            depth=24 * s)
    p.shape(ellipse_points(cx + 4 * s, cy - 24 * s, 138 * s, 76 * s, math.radians(-6), n=20), width=2.2 * s,
            wash=None, hatch=False, wobble=0.4)
    # The stick: its well, the cap and the cap's top.
    st = at(-55, -40)
    p.circle(st[0], st[1], 42 * s, width=2.5 * s, wash=SHADE, wash_alpha=70, depth=10 * s)
    p.circle(st[0] - 3 * s, st[1] - 3 * s, 29 * s, width=4.5 * s, depth=10 * s)
    p.circle(st[0] - 3 * s, st[1] - 3 * s, 15 * s, width=2 * s, wash=SHADE, wash_alpha=60, hatch=False)
    # The two buttons, the lower first.
    a, b = at(55, 5), at(84, -62)
    f = font(28 * s)
    for (bx, by), name in ((a, buttons[0]), (b, buttons[1])):
        p.circle(bx, by, 25 * s, width=3.8 * s, depth=8 * s)
        t = name[0]
        w = p.di.textlength(t, font=f) / SS
        p.di.text(((p.x(bx) - w / 2) * SS, (by - 19 * s) * SS), t, font=f, fill=INK + (255,))
    # The buttons' points on their rims, so the letters stay clear.
    rim = (22 * s, 6 * s)
    return {"stick_centre": st, "stick": (st[0] + 4 * s, st[1] - 24 * s), "a": (a[0] + rim[0], a[1] + rim[1]),
            "b": (b[0] + rim[0], b[1] + rim[1]),
            "trigger": at(-122, 50), "grip": at(60, 205)}


def page_buttons(p, other):
    names = ("A / X", "B / Y") if other else ("A", "B")
    f = controller(p, 330, 190, 1.0, names)
    lx = 610
    p.label(lx, 32, "Stick", f["stick"])
    p.label(lx, 104, names[1], f["b"])
    p.label(lx, 176, names[0], f["a"])
    p.label(lx, 268, "Trigger", f["trigger"])
    p.label(lx, 372, "Grip", f["grip"])
    hand = "left" if p.left else "right"
    p.text(610, 450, "your " + hand + " controller", size=30, italic=True)


def figure(p, cx=512, top=40, back=True):
    """A body, seen from behind (the wearer's right on the picture's right) or
    from the front, the legs cut by the picture's edge."""
    def at(dx, dy):
        return (cx + dx, top + dy)

    for side in (-1, 1):  # the legs, running out of the picture
        p.shape([at(side * 12, 372), at(side * 110, 368), at(side * 104, 430), at(side * 96, 500),
                 at(side * 90, 530), at(side * 26, 530), at(side * 20, 440)], width=4.5, depth=16)
    for side in (-1, 1):  # the arms, hanging, and the hands
        p.shape([at(side * 138, 118), at(side * 174, 146), at(side * 188, 250), at(side * 182, 350),
                 at(side * 186, 380), at(side * 174, 406), at(side * 154, 400), at(side * 150, 356),
                 at(side * 154, 252), at(side * 134, 190)], width=4.5, depth=14)
    # The torso: shoulders, a tunic to the hips.
    p.shape([at(-38, 92), at(38, 92), at(118, 112), at(152, 140), at(142, 220), at(118, 300), at(116, 370),
             at(0, 378), at(-116, 370), at(-118, 300), at(-142, 220), at(-152, 140), at(-118, 112)],
            width=5.5, depth=26)
    # The belt.
    p.shape([at(-120, 318), at(120, 318), at(119, 344), at(-119, 344)], smooth=False, width=3.5,
            wash=SHADE, wash_alpha=110, depth=8)
    if back:
        p.line([at(0, 100), at(-4, 200), at(0, 312)], width=2.0)  # the tunic's back seam
    # The neck and the head.
    p.shape([at(-24, 70), at(24, 70), at(28, 100), at(-28, 100)], width=4, depth=8)
    p.shape(ellipse_points(cx, top + 42, 40, 48, n=16), width=5, depth=18)
    if back:  # the hair seen from behind
        for k in range(-3, 4):
            p.line([at(k * 9, 4 + abs(k) * 3), at(k * 11, 40), at(k * 10 + 2, 80 - abs(k) * 4)], width=1.6)


def sword_hilt(p, x, y, length, angle, guard=46):
    """A hilt with its cross-guard and pommel, the guard at (x, y), the grip
    along `angle` (radians, 0 = right)."""
    c, s = math.cos(angle), math.sin(angle)
    nx, ny = -s, c
    end = (x + c * length, y + s * length)
    p.shape([(x + nx * 6, y + ny * 6), (end[0] + nx * 5, end[1] + ny * 5), (end[0] - nx * 5, end[1] - ny * 5),
             (x - nx * 6, y - ny * 6)], smooth=False, width=3, wash=SHADE, wash_alpha=120, depth=5)
    for k in range(1, 5):  # the grip's binding
        t = k / 5 * length
        p.line([(x + c * t + nx * 6, y + s * t + ny * 6), (x + c * t - nx * 6, y + s * t - ny * 6)],
               width=1.6, smooth=False)
    g = guard / 2
    p.shape([(x + nx * g, y + ny * g), (x + nx * g + c * 9, y + ny * g + s * 9),
             (x - nx * g + c * 9, y - ny * g + s * 9), (x - nx * g, y - ny * g)], smooth=False, width=3.5, depth=5)
    p.circle(end[0] + c * 9, end[1] + s * 9, 10, width=3.5, depth=5)


def scabbard(p, x, y, length, angle, wide=16):
    """A scabbard from its mouth at (x, y) along `angle`, narrowing to the chape."""
    c, s = math.cos(angle), math.sin(angle)
    nx, ny = -s, c
    tip = (x + c * length, y + s * length)
    p.shape([(x + nx * wide / 2, y + ny * wide / 2), (tip[0] + nx * 5, tip[1] + ny * 5),
             (tip[0] + c * 8, tip[1] + s * 8), (tip[0] - nx * 5, tip[1] - ny * 5),
             (x - nx * wide / 2, y - ny * wide / 2)], smooth=False, width=3.5, wash=SHADE, wash_alpha=100, depth=7)
    for t in (0.18, 0.86):  # its bands
        q = (x + c * length * t, y + s * length * t)
        w = wide / 2 * (1 - t * 0.5) + 2
        p.line([(q[0] + nx * w, q[1] + ny * w), (q[0] - nx * w, q[1] - ny * w)], width=2.4, smooth=False)


def page_places(p):
    figure(p)
    # Seen from behind, the wearer's left is the picture's left.
    # The bow, behind the left shoulder: its stave and string.
    bow = [(372, 52), (346, 92), (322, 170), (318, 250), (332, 330), (352, 372)]
    p.line(bow, width=7)
    p.line([bow[0], bow[-1]], width=1.6, smooth=False)
    p.shape([(318, 196), (330, 196), (330, 228), (318, 228)], smooth=False, width=2.5, wash=SHADE, depth=4)
    # The two-hander across the back, its hilt over the right shoulder.
    scabbard(p, 652, 186, 300, math.radians(124), wide=22)
    sword_hilt(p, 652, 186, 70, math.radians(-56), guard=58)
    # The quiver on the right, its fletchings by the ear.
    p.shape([(570, 108), (604, 112), (590, 270), (560, 266)], smooth=False, width=4, wash=SHADE, wash_alpha=110,
            depth=9)
    for dx in (0, 11, 22):
        bx, by = 576 + dx, 112
        p.line([(bx, by), (bx + 4, by - 50)], width=2, smooth=False)
        for side in (-1, 1):
            p.shape([(bx + 4, by - 50), (bx + 4 + side * 10, by - 66), (bx + 4 + side * 6, by - 38)], smooth=False,
                    width=2, hatch=False)
    # The one-hander at the left hip, its hilt up and forward.
    scabbard(p, 404, 372, 150, math.radians(112))
    sword_hilt(p, 404, 372, 46, math.radians(-68))
    # The places, where the hand goes.
    for at in ((402, 362), (668, 156), (322, 212), (588, 74)):
        p.gold_place(*at, r=20)
    p.label(40, 392, "one-hander", (388, 366), size=34)
    p.label(730, 186, "two-hander, staff", (684, 160), size=34)
    p.label(40, 150, "bow", (306, 208), size=34)
    p.label(730, 56, "arrows", (604, 74), size=34)
    p.text(40, 470, "seen from behind", size=28, italic=True)


def potion(p, x, y):
    """A potion of the game's shape: a round flask, a long neck, a cork; its
    liquid red. (x, y) is the neck's middle."""
    p.shape(ellipse_points(x, y + 70, 48, 50, n=18), width=4.5, depth=16)
    p.shape([(x - 40, y + 78), (x + 40, y + 78), (x + 44, y + 98), (x, y + 116), (x - 44, y + 98)],
            wash=POTION, wash_alpha=160, hatch=False, ink=False)
    p.line([(x - 40, y + 76), (x, y + 70), (x + 40, y + 76)], width=1.8)  # the liquid's surface
    p.line([(x - 26, y + 48), (x - 32, y + 64)], width=2.2)  # a gleam
    p.shape([(x - 10, y - 34), (x + 10, y - 34), (x + 11, y + 26), (x - 11, y + 26)], smooth=False, width=3.5,
            depth=5)
    p.shape([(x - 13, y - 52), (x + 13, y - 52), (x + 11, y - 34), (x - 11, y - 34)], smooth=False, width=3,
            wash=SHADE, wash_alpha=130, depth=4)


def chest(p, x, y, w=220, h=120):
    """A chest of iron-bound boards, its lid up; (x, y) its lower left."""
    p.shape([(x, y - h), (x + w, y - h), (x + w, y), (x, y)], smooth=False, width=5, depth=18)
    for k in range(1, 4):  # the boards
        yy = y - h + k * h / 4
        p.line([(x + 6, yy), (x + w - 6, yy)], width=1.5, smooth=False)
    for bx in (x + 30, x + w - 50):  # the iron bands and their rivets
        p.shape([(bx, y - h), (bx + 20, y - h), (bx + 20, y), (bx, y)], smooth=False, width=3, wash=SHADE,
                wash_alpha=140, depth=5)
        for ry in (y - h + 18, y - 18):
            p.dot(bx + 10, ry, 3.2)
    p.shape([(x + w / 2 - 16, y - h + 10), (x + w / 2 + 16, y - h + 10), (x + w / 2 + 16, y - h + 48),
             (x + w / 2 - 16, y - h + 48)], smooth=False, width=3, wash=GOLD, wash_alpha=120, depth=5)
    p.dot(x + w / 2, y - h + 30, 4)
    # The lid, opened back, the dark inside under it.
    p.shape([(x + 4, y - h), (x + w - 4, y - h), (x + w + 18, y - h - 26), (x + 22, y - h - 36)], smooth=False,
            width=2, wash=INK, wash_alpha=170, hatch=False)
    lid = [(x - 4, y - h), (x + w + 4, y - h), (x + w + 30, y - h - 62), (x + 26, y - h - 84)]
    p.shape(lid, smooth=False, width=4.5, depth=12)
    for bx in (30, w - 50):
        q0 = (x + bx, y - h)
        q1 = (x + 26 + bx * (w + 4) / w, y - h - 84 + bx * 22 / w)
        p.shape([q0, (q0[0] + 20, q0[1]), (q1[0] + 20, q1[1] + 2), q1], smooth=False, width=2.5, wash=SHADE,
                wash_alpha=140, hatch=False)


def page_holding(p):
    figure(p, cx=400, back=False)
    # The chest's gold ring, where a held item is let go to be kept.
    p.gold_place(400, 214, r=34)
    p.text(40, 52, "let go here:", size=34)
    p.text(40, 92, "kept", size=34)
    p.line([(170, 120), (300, 150), (366, 202)], width=2.6)
    # A potion in the weapon hand, brought to it.
    potion(p, 640, 300)
    p.arrow([(600, 262), (540, 222), (456, 214)])
    # A chest to reach into.
    chest(p, 770, 470)
    p.arrow([(900, 150), (880, 230), (876, 300)])
    p.text(740, 100, "reach in: open", size=34)


def page_both(p, left_pen, weapon_is_right):
    # The physical controllers side by side as held: the right one on the
    # picture's right, the left one - the right one mirrored - on its left.
    fl = controller(left_pen, W - 1 - 330, 140, 0.72, ("X", "Y"))
    fr = controller(p, W - 1 - 330, 140, 0.72, ("A", "B"))
    lstick = (W - 1 - fl["stick_centre"][0], fl["stick_centre"][1])
    for stick in (lstick, fr["stick_centre"]):
        p.gold_place(stick[0], stick[1], r=40)
    weapon_text = "click: hotkey ring"
    other_text = "click: spell ring"
    left_text = other_text if weapon_is_right else weapon_text
    right_text = weapon_text if weapon_is_right else other_text
    p.text(330, 400, left_text, size=32, centre=True)
    p.text(W - 1 - 330, 400, right_text, size=32, centre=True)
    p.text(W / 2, 452, "both clicked: settings - held 3 s: recenter", size=32, centre=True, italic=True)


def draw(page, left):
    seed = PAGES.index(page) * 2 + (1 if left else 0)
    if page == "both_hands":
        p = Pen(False, seed)
        left_pen = Pen(True, seed + 100)
        page_both(p, left_pen, weapon_is_right=not left)
        img = left_pen.picture()
        img.alpha_composite(p.picture())
        return img
    if page == "other_buttons":
        p = Pen(not left, seed)
        page_buttons(p, other=True)
    else:
        p = Pen(left, seed)
        {"weapon_buttons": lambda: page_buttons(p, other=False), "weapon_places": lambda: page_places(p),
         "holding": lambda: page_holding(p)}[page]()
    return p.picture()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", type=Path, help="also write PNGs on parchment into this folder")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    for i, page in enumerate(PAGES):
        for left in (False, True):
            img = draw(page, left)
            name = f"controls_{i + 1}_{'left' if left else 'right'}"
            png = OUT / f"{name}.png"
            img.save(png)
            dds = OUT / f"{name}.dds"
            subprocess.run(["magick", str(png), "-define", "dds:compression=dxt5", "-define", "dds:mipmaps=0",
                            str(dds)], check=True)
            png.unlink()
            if args.preview:
                args.preview.mkdir(parents=True, exist_ok=True)
                bg = Image.new("RGBA", (W, H), PAPER)
                bg.alpha_composite(img)
                bg.convert("RGB").save(args.preview / f"{name}.png")
            print(f"wrote {dds.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
