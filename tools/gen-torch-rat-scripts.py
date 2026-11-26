#!/usr/bin/env python3
"""Writes the torch-on-a-rat scenarios (free-weapons-spec 5.9.1): tools/hand-scripts/
torch-blow-rat.txt, torch-blow-rat-chop.txt and torch-blow-rat-near.txt.

Usage: python3 tools/gen-torch-rat-scripts.py tools/hand-scripts
"""
import math
import os
import sys

# Smooth, person-like strokes: from pose A to pose B over T seconds with a
# cosine ease (slow start, fastest in the middle, slow end), a step every DT
# seconds, position and rotation together; the peak speed is pi/2 |B-A| / T.

DT = 0.011


def lerp(a, b, t):
    return [x + (y - x) * t for x, y in zip(a, b)]


def stroke(hand, pos_a, pos_b, rot_a, rot_b, seconds, dt=DT, other=None):
    """Lines moving `hand` from A to B. `other` = (hand, pos_a, pos_b) moved
    alongside (the other hand's counter-move), same easing."""
    out = []
    n = max(1, int(round(seconds / dt)))
    for i in range(1, n + 1):
        t = i / n
        e = 0.5 - 0.5 * math.cos(math.pi * t)
        p = lerp(pos_a, pos_b, e)
        r = lerp(rot_a, rot_b, e)
        out.append(f"{hand} rot {r[0]:.1f} {r[1]:.1f} {r[2]:.1f}")
        out.append(f"{hand} pos {p[0]:.3f} {p[1]:.3f} {p[2]:.3f}")
        if other is not None:
            oh, oa, ob = other
            q = lerp(oa, ob, e)
            out.append(f"{oh} pos {q[0]:.3f} {q[1]:.3f} {q[2]:.3f}")
        out.append(f"wait {dt:.3f}")
    return out


def peak_speed(pos_a, pos_b, seconds):
    d = math.dist(pos_a, pos_b)
    return math.pi / 2 * d / seconds

COMMON_HEAD = """# An Iron Longsword (00000C0C) drawn by the holster, Torch02 (0002CF9F) in the
# left hand, a rat (CreatureRat, 00025314) placed 60 units ahead and frozen
# (tai), given health for several blows. Its bound is 96 units round: the
# old rule's column for the hand (half the radius wide, the radius high) is
# a metre and a half across round a rat. The torch hand moves
# as a person's: smooth strokes with their follow-through, written by
# tools/gen-torch-rat-scripts.py (a cosine ease, a step every 11 ms; run with
# --hz 90).
# Run with: python3 tools/hand-script-run.py tools/hand-scripts/{name}.txt --headless --no-steamvr --parallel --hz 90 --plugin build-linux-clangcl/OBVR.dll

ini Hands Enabled=1
ini Debug ShoveTrace=1
ini Debug ContactTrace=300
# The torch equipped by the console comes into an open hand: no grace (TorchGraceSeconds, 2026-10-27).
ini Hands TorchGraceSeconds=0
ini Hands Holsters=1
ini Hands GripByPull=1
ini Hands WeaponHold=1
ini Hands OffHandStrikes=1
ini Hands OffHandGrabs=0
ini Hands TwoHandOneHanders=0
ini Hands PushPeople=0
ini Hands TorchDrop=0
ini Hands VanillaKnockdown=0
ini Hands ReachOpens=0
# The stagger's push off, so each blow finds the rat where it stood.
ini Hands ShoveDistance=0
console player.additem 00000C0C 1
console player.equipitem 00000C0C
console player.additem 0002CF9F 1
console-at ready player.equipitem 0002CF9F
console-at ready tgm
console-at ready player.placeatme 00025314 1 60 0
console-at ready tai
console-at placed {{actor}}.setav health 400
"""

SETUP = """wait 12
left curl 0.8
left rot 0 0 0
left pos -0.20 -0.40 -0.20
right rot 0 0 0
right pos 0.20 -0.40 -0.30
wait 1
mark ready
wait 22
mark placed
# The sword drawn from the hip (weapon-drop.txt's draw) and held low at the
# right side, pointing back, the grip kept closed (WeaponHold lets go of an
# opened one).
right pos -0.18 -0.62 -0.02
wait 0.3
right grip 0.9
wait 0.3
right pos 0.30 -0.40 -0.35
wait 3
right rot 0 180 0
right pos 0.25 -0.55 0.05
wait 1
head pitch -35
wait 1
mark drawn"""

REST = ([-0.20, -0.40, -0.20], [0, 0, 0])


def chop():
    a, ra = [-0.15, -0.45, -0.30], [30, 0, 0]
    b, rb = [-0.12, -1.05, -0.62], [-75, 0, 0]
    c, rc = [-0.10, -1.15, -0.55], [-85, 0, 0]
    out = ["# The chop: the torch raised over the rat (slowly), brought down onto it at %.1f m/s at its fastest, and "
           "on past it." % peak_speed(a, b, 0.22)]
    out += stroke("left", REST[0], a, REST[1], ra, 0.6)
    out += ["wait 0.3", "mark chop_raised"]
    out += stroke("left", a, b, ra, rb, 0.22)
    out += stroke("left", b, c, rb, rc, 0.12)
    out += ["wait 0.5", "mark chop"]
    out += stroke("left", c, REST[0], rc, REST[1], 0.8)
    out += ["wait 2.0", "mark chop_back"]
    return out


def near_miss():
    # The hand 50 units ahead, sweeping from the right towards the rat's
    # middle, the torch pointing nearly straight down: its head stops some
    # 20 units short of the rat's right flank, the hand already inside the
    # old rule's column and going its way at 4 m/s and more.
    a, ra = [1.20, -0.90, -0.74], [-80, 0, 0]
    b, rb = [0.42, -0.90, -0.74], [-80, 0, 0]
    out = ["# The near miss: from the right towards the rat at %.1f m/s at its fastest, the torch's head stopping "
           "short of it." % peak_speed(a, b, 0.18)]
    out += stroke("left", REST[0], a, REST[1], ra, 0.7)
    out += ["wait 0.4", "mark near_ready"]
    out += stroke("left", a, b, ra, rb, 0.18)
    out += ["wait 0.6", "mark near"]
    out += stroke("left", b, REST[0], rb, REST[1], 0.8)
    out += ["wait 2.0", "mark near_back"]
    return out


def sweep():
    # Low, through the rat's middle: the torch nearly straight down, its head
    # at the rat's height, swept from the right to the left across it.
    a, ra = [0.60, -0.90, -0.86], [-80, 0, 0]
    b, rb = [-0.60, -0.90, -0.86], [-80, 0, 0]
    c, rc = [-0.75, -0.85, -0.80], [-75, 0, 0]
    out = ["# The sweep: low across the rat's middle from the right at %.1f m/s at its fastest, and on past it."
           % peak_speed(a, b, 0.35)]
    out += stroke("left", REST[0], a, REST[1], ra, 0.7)
    out += ["wait 0.4", "mark sweep_ready"]
    out += stroke("left", a, b, ra, rb, 0.35)
    out += stroke("left", b, c, rb, rc, 0.12)
    out += ["wait 0.5", "mark sweep"]
    out += stroke("left", c, REST[0], rc, REST[1], 0.8)
    out += ["wait 2.0", "mark sweep_back"]
    return out


def write(path, name, intro, checks, parts):
    head = intro.rstrip("\n") + "\n" + COMMON_HEAD.format(name=name) + "\n" + checks.rstrip("\n") + "\n\n" + SETUP
    out = [head]
    for p in parts:
        out += p
    out += ["wait 1", "mark end"]
    with open(path, "w") as f:
        f.write("\n".join(out) + "\n")
    print("wrote", path)


def main():
    folder = sys.argv[1]
    write(os.path.join(folder, "torch-blow-rat.txt"), "torch-blow-rat",
          """# The torch's blow on a small creature (the tester, Windows 2026-11-26:
# "gegner mit fackel links schlagen ging manchmal nicht" - his log has two
# fast torch blows that met a rat, "a fast torch is held by 2B137C60 at 5.6
# m/s", "rests on 2B137C60 ... 9.12 m/s into it", and no blow landed). Three
# strokes at a rat, in the order that keeps it where it stood for each:
#   near  - towards it from the right, the torch's head stopping short of it
#   chop  - raised over it and brought down onto it
#   sweep - low across its middle from the right
# The blow is the torch's touch (game/TorchTouchLogic.h): the chop and the
# sweep strike, the near miss does not. torch-blow-rat-chop.txt and
# torch-blow-rat-near.txt are the two parts that the old rule (the hand in
# the bound's column, its speed across the ground towards the middle) had
# wrong, alone.
""",
          """expect HandScript: finished
expect Hands: ready weapon done
count 2 Shove: the torch went into
count 2 - a torch, skill 17 (
reject Shove: the left hand at
reject HandScript: WARNING""",
          [near_miss(), chop(), sweep()])
    write(os.path.join(folder, "torch-blow-rat-chop.txt"), "torch-blow-rat-chop",
          """# The torch's chop onto a rat (torch-blow-rat.txt's chop alone): the tester's
# miss of 2026-11-26. The old rule took the hand's speed across the ground
# towards the bound's middle - a chop goes down, 2.25 of its 4.8 m/s went
# that way, under the stagger's 3, and nothing landed while the contact step
# held the torch on the rat ("a fast torch is held by ... at 4.4 m/s").
""",
          """expect HandScript: finished
expect Hands: ready weapon done
expect - a torch, skill 17 (
reject HandScript: WARNING""",
          [chop()])
    write(os.path.join(folder, "torch-blow-rat-near.txt"), "torch-blow-rat-near",
          """# The torch's near miss of a rat (torch-blow-rat.txt's first stroke alone):
# the torch's head stops some 20 units short of the rat's flank, while the
# hand is inside the old rule's column (the rat's bound 96 units round, the
# column 52 units to either side of its middle) going towards its middle at
# 4 m/s and more - the old rule struck the rat with a torch that never met it.
""",
          """expect HandScript: finished
expect Hands: ready weapon done
reject - a torch, skill 17 (
reject HandScript: WARNING""",
          [near_miss()])


main()
