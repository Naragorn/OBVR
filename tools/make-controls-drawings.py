#!/usr/bin/env python3
"""Draws the controls card's pictures (ui/NativeControlsCard.h) and writes them
as DXT5 DDS under assets/textures/Menus/OBVR/, the game's Data\\Textures\\Menus\\OBVR
(the case of Menus as the game's own folder has it, for Linux file systems).

Five pages, each drawn for a right-hander and mirrored for a left-hander:

  page 1  the weapon hand's controller: trigger, grip, stick, A, B
  page 2  the places weapons are drawn from, the body seen from behind
  page 3  the other hand's controller (A / X and B / Y: Quest's left names)
  page 4  holding: an item brought to the chest's gold ring, a chest reached into
  page 5  both controllers: the stick clicks for the two rings, both for settings

Our own line drawings in the parchment's ink - no vendor artwork: a generic
controller (a head with a stick and two buttons, a trigger under it, a grip on
the handle) that reads for both Index and Quest. The places on page 2 are the
places' defaults (vr/Holster.h, vr/Archery.h); a side chosen in the settings
is not drawn. The labels are Noto Serif Bold (SIL Open Font License).

Run:    python3 tools/make-controls-drawings.py [--preview DIR]
Needs:  Pillow and ImageMagick (magick) for the DDS.
"""

import argparse
import math
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "textures" / "Menus" / "OBVR"
W, H = 1024, 512
INK = (70, 45, 25, 255)
SOFT = (70, 45, 25, 70)
FILL = (240, 225, 190, 140)
GOLD = (196, 150, 40, 255)
FONT = "/usr/share/fonts/noto/NotoSerif-Bold.ttf"
LINE = 6

PAGES = ["weapon_buttons", "weapon_places", "other_buttons", "holding", "both_hands"]


def font(size):
    return ImageFont.truetype(FONT, size)


class Pen:
    """Draws in right-hander coordinates; `left` mirrors every shape across
    the picture's middle, while text stays readable."""

    def __init__(self, left):
        self.left = left
        self.img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)

    def x(self, x):
        return W - 1 - x if self.left else x

    def pts(self, points):
        return [(self.x(px), py) for px, py in points]

    def box(self, x0, y0, x1, y1):
        a, b = self.x(x0), self.x(x1)
        return (min(a, b), y0, max(a, b), y1)

    def ellipse(self, cx, cy, rx, ry, fill=FILL, outline=INK, width=LINE):
        self.d.ellipse(self.box(cx - rx, cy - ry, cx + rx, cy + ry), fill=fill, outline=outline, width=width)

    def poly(self, points, fill=FILL, outline=INK, width=LINE):
        p = self.pts(points)
        self.d.polygon(p, fill=fill)
        self.d.line(p + [p[0]], fill=outline, width=width, joint="curve")

    def line(self, points, fill=INK, width=LINE):
        self.d.line(self.pts(points), fill=fill, width=width, joint="curve")

    def arrow(self, a, b, fill=INK, width=LINE):
        self.line([a, b], fill, width)
        (ax, ay), (bx, by) = self.pts([a, b])
        ang = math.atan2(by - ay, bx - ax)
        for s in (-1, 1):
            t = ang + math.pi + s * 0.45
            self.d.line([(bx, by), (bx + 26 * math.cos(t), by + 26 * math.sin(t))], fill=fill, width=width)

    def text(self, x, y, s, size=38, anchor_right=None, centre=False):
        """Text at (x, y) in right-hander coordinates. A label that starts at x
        for a right-hander ends at the mirrored x for a left-hander."""
        f = font(size)
        w = self.d.textlength(s, font=f)
        if centre:
            px = self.x(x) - w / 2
        elif self.left:
            px = self.x(x) - w
        else:
            px = x
        if anchor_right is not None and not self.left:
            px = anchor_right - w
        self.d.text((px, y), s, font=f, fill=INK)

    def label(self, x, y, s, to, size=38):
        """A label at the picture's side with a leader line to the point `to`."""
        self.text(x, y, s, size)
        self.line([(x - 14, y + size * 0.62), to], fill=INK, width=3)
        self.ellipse(to[0], to[1], 7, 7, fill=INK, outline=INK, width=1)

    def save(self, name):
        return self.img


def controller(p, cx, cy, scale=1.0, buttons=("A", "B")):
    """A generic controller seen from above-behind, as held in the right hand:
    the head with the stick and two buttons, the trigger hooked under its
    front, the grip down the handle. Returns the feature points."""
    s = scale
    # The handle, slanting back and down from under the head.
    p.poly([(cx - 40 * s, cy + 40 * s), (cx + 55 * s, cy + 45 * s), (cx + 75 * s, cy + 300 * s),
            (cx - 5 * s, cy + 305 * s)])
    # The grip band on the handle's outer side.
    p.poly([(cx + 40 * s, cy + 110 * s), (cx + 66 * s, cy + 112 * s), (cx + 76 * s, cy + 270 * s),
            (cx + 48 * s, cy + 272 * s)], fill=SOFT, width=4)
    # The trigger: a hook under the head's front, where the index finger lies.
    p.poly([(cx - 150 * s, cy + 10 * s), (cx - 115 * s, cy - 5 * s), (cx - 85 * s, cy + 60 * s),
            (cx - 112 * s, cy + 95 * s), (cx - 140 * s, cy + 70 * s)], fill=SOFT, width=4)
    # The head, its face plate.
    p.ellipse(cx, cy - 20 * s, 160 * s, 95 * s)
    # The stick.
    p.ellipse(cx - 55 * s, cy - 40 * s, 38 * s, 34 * s)
    p.ellipse(cx - 55 * s, cy - 40 * s, 16 * s, 14 * s, fill=INK)
    # The two buttons, the lower first.
    a = (cx + 55 * s, cy + 5 * s)
    b = (cx + 80 * s, cy - 60 * s)
    for (bx, by), name in ((a, buttons[0]), (b, buttons[1])):
        p.ellipse(bx, by, 25 * s, 25 * s)
        f = font(int(28 * s))
        t = name[0]
        w = p.d.textlength(t, font=f)
        p.d.text((p.x(bx) - w / 2, by - 18 * s), t, font=f, fill=INK)
    return {
        "stick": (cx - 55 * s, cy - 40 * s),
        "a": a,
        "b": b,
        "trigger": (cx - 125 * s, cy + 45 * s),
        "grip": (cx + 62 * s, cy + 190 * s),
    }


def page_buttons(left, other):
    p = Pen(left)
    names = ("A / X", "B / Y") if other else ("A", "B")
    f = controller(p, 330, 190, 1.0, names)
    lx = 600
    p.label(lx, 40, "Stick", f["stick"])
    p.label(lx, 110, names[1], f["b"])
    p.label(lx, 180, names[0], f["a"])
    p.label(lx, 270, "Trigger", f["trigger"])
    p.label(lx, 380, "Grip", f["grip"])
    # The controller drawn: the right one, mirrored into the left.
    hand = "left" if left else "right"
    p.text(600, 455, "your " + hand + " controller", size=30)
    return p.img


def figure(p, cx=512, top=40):
    """A body seen from behind: the wearer's right is the picture's right."""
    p.ellipse(cx, top + 45, 42, 48)  # head
    p.poly([(cx - 150, top + 120), (cx + 150, top + 120), (cx + 115, top + 320), (cx - 115, top + 320)])  # back
    p.poly([(cx - 115, top + 320), (cx + 115, top + 320), (cx + 105, top + 380), (cx - 105, top + 380)])  # hips
    p.line([(cx - 150, top + 125), (cx - 190, top + 300), (cx - 175, top + 400)])  # left arm
    p.line([(cx + 150, top + 125), (cx + 190, top + 300), (cx + 175, top + 400)])  # right arm
    p.line([(cx - 60, top + 380), (cx - 70, top + 470)])
    p.line([(cx + 60, top + 380), (cx + 70, top + 470)])


def place(p, at, text_at, s, size=34):
    """A gold place on the body, its name at the picture's side and a line to it."""
    p.ellipse(at[0], at[1], 22, 22, fill=(196, 150, 40, 120), outline=GOLD, width=5)
    p.text(text_at[0], text_at[1], s, size=size)
    f = font(size)
    w = p.d.textlength(s, font=f)
    # The line from the label's inner end to the place.
    inner = text_at[0] + w + 10 if text_at[0] < at[0] else text_at[0] - 10
    p.line([(inner, text_at[1] + size * 0.62), (at[0], at[1])], width=3)


def page_places(left):
    p = Pen(left)
    figure(p)
    # Seen from behind, the wearer's left is the picture's left.
    place(p, (402, 380), (40, 400), "one-hander")         # the left hip
    place(p, (665, 170), (720, 200), "two-hander, staff")   # over the right shoulder
    place(p, (362, 170), (40, 150), "bow")                # behind the left shoulder
    place(p, (585, 90), (720, 60), "arrows")              # the quiver: right, by the ear
    p.text(512, 470, "seen from behind", size=28, centre=True)
    return p.img


def page_holding(left):
    p = Pen(left)
    figure(p, cx=420)
    # The chest's gold ring, where a held item is let go to be kept.
    p.ellipse(420, 200, 40, 40, fill=(196, 150, 40, 60), outline=GOLD, width=7)
    p.text(80, 60, "let go here:", size=34)
    p.text(80, 100, "kept", size=34)
    p.line([(250, 120), (390, 185)], width=3)
    # An item in the weapon hand, brought to it.
    p.poly([(590, 400), (620, 400), (625, 470), (585, 470)])  # a bottle
    p.poly([(597, 370), (613, 370), (613, 400), (597, 400)], width=4)
    p.arrow((590, 360), (470, 230))
    # A chest to reach into.
    p.poly([(760, 330), (980, 330), (980, 470), (760, 470)])
    p.line([(760, 380), (980, 380)], width=4)
    p.arrow((850, 230), (870, 360))
    p.text(760, 180, "reach in: open", size=34)
    return p.img


def page_both(left):
    p = Pen(False)
    # The physical controllers side by side as held: the right one on the
    # picture's right, the left one - the right one mirrored - on its left.
    left_pen = Pen(True)
    fl = controller(left_pen, W - 1 - 330, 170, 0.75, ("X", "Y"))
    fr = controller(p, W - 1 - 330, 170, 0.75, ("A", "B"))
    p.img.alpha_composite(left_pen.img)
    weapon_is_right = not left
    # The stick clicks, each under its controller.
    lstick = (W - 1 - fl["stick"][0], fl["stick"][1])
    rstick = fr["stick"]
    for stick in (lstick, rstick):
        p.d.ellipse((stick[0] - 40, stick[1] - 36, stick[0] + 40, stick[1] + 36), outline=GOLD, width=6)
    f = font(32)
    weapon_text = "click: hotkey ring"
    other_text = "click: spell ring"
    left_text = other_text if weapon_is_right else weapon_text
    right_text = weapon_text if weapon_is_right else other_text
    p.d.text((330 - p.d.textlength(left_text, font=f) / 2, 400), left_text, font=f, fill=INK)
    p.d.text((W - 1 - 330 - p.d.textlength(right_text, font=f) / 2, 400), right_text, font=f, fill=INK)
    both = "both clicked: settings - held 3 s: recenter"
    p.d.text((W / 2 - p.d.textlength(both, font=f) / 2, 455), both, font=f, fill=INK)
    return p.img


def draw(page, left):
    if page == "weapon_buttons":
        return page_buttons(left, other=False)
    if page == "weapon_places":
        return page_places(left)
    if page == "other_buttons":
        return page_buttons(not left, other=True)
    if page == "holding":
        return page_holding(left)
    return page_both(left)


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
                bg = Image.new("RGBA", (W, H), (226, 206, 160, 255))
                bg.alpha_composite(img)
                bg.convert("RGB").save(args.preview / f"{name}.png")
            print(f"wrote {dds.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
