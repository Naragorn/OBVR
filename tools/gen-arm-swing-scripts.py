#!/usr/bin/env python3
"""Writes the arm-swing scenarios with a weapon drawn (controls-spec, "a pump of
the run or a weapon's stroke"): tools/hand-scripts/arm-swing-weapon.txt, and
arm-swing-torch-block.txt with "block".

Usage: python3 tools/gen-arm-swing-scripts.py tools/hand-scripts/arm-swing-weapon.txt
       python3 tools/gen-arm-swing-scripts.py tools/hand-scripts/arm-swing-torch-block.txt block
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

RIGHT_C = (0.22, -0.50, -0.15)
LEFT_C = (-0.22, -0.50, -0.15)
AZ, AY = 0.28, 0.10  # forward-back and up-down of a pump, metres


def pumps(seconds, hz, left_scale, dt=DT, up=0.0):
    """Both arms swung against each other as a runner's: the right hand
    forward-up while the left goes back-down, sinusoidal; the left's swing
    `left_scale` of the right's; both `up` metres higher than RIGHT_C/LEFT_C."""
    out = []
    n = int(round(seconds / dt))
    for i in range(1, n + 1):
        s = math.sin(2.0 * math.pi * hz * i * dt)
        rx, ry, rz = RIGHT_C[0], RIGHT_C[1] + up + AY * s, RIGHT_C[2] - AZ * s
        lx, ly, lz = LEFT_C[0], LEFT_C[1] + up - AY * left_scale * s, LEFT_C[2] + AZ * left_scale * s
        out.append(f"right pos {rx:.3f} {ry:.3f} {rz:.3f}")
        out.append(f"left pos {lx:.3f} {ly:.3f} {lz:.3f}")
        out.append(f"wait {dt:.3f}")
    return out


HEADER = """# Running by swung arms with a weapon drawn (the tester, Windows 2026-11-26:
# "wenn man die waffe gezogen hat dann ist 'swing arms' zum sprinten
# schwierig oder buggy, weil er nicht weiss ob sprint oder schwung, sodass er
# den lauf abbricht usw."). His log: six weapon swings inside his arm-swing
# sprints at 2.7-3.7 m/s and 0.22-0.75 m (the median 2.9 m/s, 0.34 m)
# against 5.5 m/s and 0.71 m outside them - the run's pumps taken as
# attacks, each charged 8.8 fatigue (17.6 the one forward power swing).
# An Iron Longsword (00000C0C) drawn by the holster, Torch02 (0002CF9F) in
# the left hand, AnalogWalk and ArmSwingRun on, the left stick pushed a
# third forward. Written by tools/gen-arm-swing-scripts.py: the hands
# sinusoidal, a step every 11 ms (run with --hz 90).
#   pumps      - 4 s of both arms swung against each other at 1.5 Hz, 0.28 m
#                forward and back and 0.10 m up and down, alike: a runner's
#                pumps, the hands at 2.8 m/s at their fastest
#   lopsided   - 3 s the same with the torch arm at 60 % of the sword arm
#   counter    - walking, one slash of the sword from the right across and
#                down (6.5 m/s) while the torch hand draws back and up (2 m/s)
#   slash      - the same slash with the torch hand still
#   block      - the stick pulled fully back (the push runs, away from the
#                wall the sprints end at) 1.5 s with the torch low, then 1.5
#                s with it raised into the block: whether the torch's block
#                slows the run (his torch hand rose into the block with his
#                pumps)
# A pump is no swing and a stroke starts no run (vr/ArmSwing.h,
# ArmSwingBalance): the two slashes are the only swings charged (the draw's
# jump from the hip logs a swing of its own before the sword is out), the two
# pump runs the only runs.
# Run with: python3 tools/hand-script-run.py tools/hand-scripts/arm-swing-weapon.txt --headless --no-steamvr --parallel --hz 90 --plugin build-linux-clangcl/OBVR.dll

ini Hands Enabled=1
ini Hands ArmSwingRun=1
ini Hands AnalogWalk=1
ini Hands ReachOpens=0
# The torch equipped by the console comes into an open hand: no grace (TorchGraceSeconds, 2026-10-27).
ini Hands TorchGraceSeconds=0
ini Hands Holsters=1
ini Hands GripByPull=1
ini Hands WeaponHold=1
ini Hands TorchDrop=0
ini Hands OffHandGrabs=0
ini Hands TwoHandOneHanders=0
console player.additem 00000C0C 1
console player.equipitem 00000C0C
console player.additem 0002CF9F 1
console-at ready player.equipitem 0002CF9F
# Fatigue for every part, so none ends spent.
console-at ready player.setav fatigue 600

expect HandScript: finished
expect Hands: ready weapon done
count 2 Fatigue: a weapon light swing cost
count 2 Arm swing: running
count 2 AnalogWalk: sprinting
reject HandScript: WARNING

wait 12
left curl 0.8
left rot 0 0 0
left pos -0.20 -0.40 -0.20
right rot 0 0 0
right pos 0.20 -0.40 -0.30
wait 1
mark ready
wait 18
mark placed
# The sword drawn from the hip (weapon-drop.txt's draw), the grip kept closed
# (WeaponHold lets go of an opened one).
right pos -0.18 -0.62 -0.02
wait 0.3
right grip 0.9
wait 0.3
right pos 0.30 -0.40 -0.35
wait 3
right rot -20 0 0
left rot -10 0 0
"""


def main():
    out = [HEADER.rstrip("\n")]
    out += stroke("right", [0.30, -0.40, -0.35], list(RIGHT_C), [-20, 0, 0], [-20, 0, 0], 0.5)
    out += stroke("left", [-0.20, -0.40, -0.20], list(LEFT_C), [-10, 0, 0], [-10, 0, 0], 0.5)
    out += ["wait 1", "mark drawn"]

    # The pumps, the stick a third forward.
    out += ["# pumps: both arms alike, 4 s", "left stick 0 0.33", "wait 0.5", "mark pumps_start"]
    out += pumps(4.0, 1.5, 1.0)
    out += [f"right pos {RIGHT_C[0]} {RIGHT_C[1]} {RIGHT_C[2]}", f"left pos {LEFT_C[0]} {LEFT_C[1]} {LEFT_C[2]}",
            "mark pumps_end", "left stick 0 0", "wait 2"]

    out += ["# lopsided: the torch arm at 60 %, 3 s", "left stick 0 0.33", "wait 0.5", "mark lopsided_start"]
    out += pumps(3.0, 1.5, 0.6)
    out += [f"right pos {RIGHT_C[0]} {RIGHT_C[1]} {RIGHT_C[2]}", f"left pos {LEFT_C[0]} {LEFT_C[1]} {LEFT_C[2]}",
            "mark lopsided_end", "left stick 0 0", "wait 2"]

    # The slash: from the right, across and down; the counter-move: the torch hand back and up.
    ra, rb = [0.35, -0.40, -0.30], [-0.25, -0.65, -0.40]
    la, lb = [-0.22, -0.55, -0.25], [-0.22, -0.40, -0.10]
    out += ["# counter: walking, the slash at %.1f m/s, the torch hand back and up at %.1f m/s"
            % (peak_speed(ra, rb, 0.16), peak_speed(la, lb, 0.16))]
    out += stroke("right", list(RIGHT_C), ra, [-20, 0, 0], [-20, 0, 0], 0.5)
    out += stroke("left", list(LEFT_C), la, [-10, 0, 0], [-10, 0, 0], 0.3)
    out += ["left stick 0 0.33", "wait 0.8", "mark counter_start"]
    out += stroke("right", ra, rb, [-20, 0, 0], [-30, 0, 0], 0.16, other=("left", la, lb))
    out += ["wait 0.8", "mark counter_end", "left stick 0 0", "wait 1.5"]
    out += stroke("right", rb, ra, [-30, 0, 0], [-20, 0, 0], 0.8)
    out += stroke("left", lb, la, [-10, 0, 0], [-10, 0, 0], 0.5)
    out += ["wait 1.5", "mark slash_start"]
    out += stroke("right", ra, rb, [-20, 0, 0], [-30, 0, 0], 0.16)
    out += ["wait 1.0", "mark slash_end", "wait 1"]

    # The block: the push runs, the torch low, then raised.
    out += stroke("right", rb, list(RIGHT_C), [-30, 0, 0], [-20, 0, 0], 0.6)
    out += stroke("left", la, [-0.22, -0.60, -0.10], [-10, 0, 0], [-10, 0, 0], 0.4)
    out += ["wait 0.5", "mark block_low_start", "left stick 0 -1", "wait 1.5", "mark block_low_end"]
    out += ["left pos -0.15 -0.22 -0.35", "left rot 20 0 0", "wait 1.5", "mark block_up_end", "left stick 0 0",
            "wait 1", "mark end"]
    path = sys.argv[1]
    with open(path, "w") as f:
        f.write("\n".join(out) + "\n")
    print("wrote", path, len(out), "lines")


BLOCK_HEAD = """# Running by swung arms with the torch in the left hand, the hands high (the
# tester, 2026-11-26: "... sodass er den lauf abbricht usw."). His log has the
# torch hand going "up - blocking" and "down" with his pumps, and the engine
# walks a blocking player: arm-swing-weapon.txt measured the push's run
# backwards at 322 units a second with the torch low and 107 with it raised
# into the block - the walk's speed. Here both arms pump 0.15 m higher than
# there (the hands between 45 and 25 cm under the eyes, at rest 35), so the
# torch rises past the block's height (30 cm under the eyes, 12 cm ahead) for
# a third of each forward swing; the
# sword drawn, the stick a third forward, 2.5 s. With ArmSwingBalance a hand
# raised while the arms run blocks nothing (vr/ArmSwing.h ArmSwingHoldsBlocks).
# Written by tools/gen-arm-swing-scripts.py, its argument "block".
# Run with: python3 tools/hand-script-run.py tools/hand-scripts/arm-swing-torch-block.txt --headless --no-steamvr --parallel --hz 90 --plugin build-linux-clangcl/OBVR.dll

ini Hands Enabled=1
ini Hands ArmSwingRun=1
ini Hands AnalogWalk=1
ini Hands ReachOpens=0
# The torch equipped by the console comes into an open hand: no grace (TorchGraceSeconds, 2026-10-27).
ini Hands TorchGraceSeconds=0
ini Hands Holsters=1
ini Hands GripByPull=1
ini Hands WeaponHold=1
ini Hands TorchDrop=0
ini Hands OffHandGrabs=0
ini Hands TwoHandOneHanders=0
console player.additem 00000C0C 1
console player.equipitem 00000C0C
console player.additem 0002CF9F 1
console-at ready player.equipitem 0002CF9F
console-at ready player.setav fatigue 600

expect HandScript: finished
expect Hands: ready weapon done
count 1 Arm swing: running
reject Hands: the left hand is up - blocking
reject HandScript: WARNING
"""


def block_main(path):
    out = [BLOCK_HEAD.rstrip("\n"), "", HEADER.split("wait 12", 1)[1].join(["wait 12", ""]) if False else ""]
    out = [BLOCK_HEAD.rstrip("\n"), ""]
    setup = HEADER[HEADER.index("wait 12"):]
    out += [setup.rstrip("\n")]
    up = 0.15
    out += stroke("right", [0.30, -0.40, -0.35], [RIGHT_C[0], RIGHT_C[1] + up, RIGHT_C[2]], [-20, 0, 0], [-20, 0, 0], 0.5)
    out += stroke("left", [-0.20, -0.40, -0.20], [LEFT_C[0], LEFT_C[1] + up, LEFT_C[2]], [-10, 0, 0], [-10, 0, 0], 0.5)
    out += ["wait 1", "mark drawn"]
    out += ["# high pumps: both arms alike, 0.15 m higher, 2.5 s", "left stick 0 0.33", "wait 0.5", "mark pumps_start"]
    out += pumps(2.5, 1.5, 1.0, up=up)
    out += [f"right pos {RIGHT_C[0]} {RIGHT_C[1] + up:.2f} {RIGHT_C[2]}",
            f"left pos {LEFT_C[0]} {LEFT_C[1] + up:.2f} {LEFT_C[2]}", "mark pumps_end", "left stick 0 0", "wait 1",
            "mark end"]
    with open(path, "w") as f:
        f.write("\n".join(out) + "\n")
    print("wrote", path, len(out), "lines")


if len(sys.argv) > 2 and sys.argv[2] == "block":
    block_main(sys.argv[1])
else:
    main()
