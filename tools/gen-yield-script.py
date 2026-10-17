#!/usr/bin/env python3
"""Writes tools/hand-scripts/yield.txt: the yield scenario with the phases of
2026-10-16 (the open hands' palms sideways still yield; the sword hand's
palm sideways does not)."""
import sys

HEADER = """# The yield by gesture ([Hands]; vr/Yield.h, yield_test; the tester, 2026-10-12:
# "die mit offenen handflächen zum gegner (egal ob mit oder ohne waffe oder
# schild) und beide hände leicht nach rechts und links wippen für ein paar
# sekunden - geste, muss etwas mehr gehärtet werden damit es nicht einfach so
# im combat passiert"): a melee bandit (00069AD1) attacks an untouchable
# player (tgm). First with the hands empty: both hands held up before the
# chest with the palms to the front (right rot 90 0 90, left rot 90 0 -90:
# the controllers pointing up, the side the fingers close over - the palm,
# the right's -x and the left's +x - turned forward) and rocked together
# 8 cm left and right, half a centimetre every 0.025 s (0.2 m/s), four strokes - the
# yield, block held and activate pressed on the bandit. Then the same with
# both open palms turned sideways (rot 90 0 0): the yield again - since
# 2026-10-16 an open hand is shown by its open fingers, its palm not asked
# (the tester's left palm never read to the front in his headset). Then fast
# sideways swings of both open hands at the same height (40 cm in one step,
# metres a second): no yield, "too fast" at the mark. Then the longsword
# drawn by the holster and the rocking with the weapon in hand, its palm to
# the front: the yield (the sword hand's closed fingers are no gate since
# 2026-10-16). Last the sword hand's palm turned sideways (right rot 90 0 0,
# the blade's grip as in a guard): no yield, "the right palm not to the
# front" at the mark - the gripping hand's palm is the one asked.
# Run with: tools\\hand-script-run.ps1 -Script tools\\hand-scripts\\yield.txt
#           python3 tools/hand-script-run.py tools/hand-scripts/yield.txt

ini Hands Enabled=1
ini Hands Holsters=1
ini Hands CombatReach=85
ini Hands OffHandStrikes=0
console-at ready player.placeatme 00069AD1 1
console-at ready player.additem 00000C0C 1
console-at ready player.equipitem 00000C0C
console-at ready tgm

expect HandScript: finished
expect Hands: ready weapon done
count 3 Yield: the open hands rocked at someone in combat
expect the hands empty)
expect (a weapon in hand)
expect the fists raised first for the engine's block
expect Yield: the fists are up after
expect Yield: done - the fists raised for it are lowered again
reject menu Container
expect Yield: activate goes down
expect the last reset by: the right hand swinging, the left hand swinging
expect the right hand swinging, the left hand swinging
expect the last reset by: the right palm not to the front
expect Yield: the pose - palms to the front
reject HandScript: WARNING

wait 12
right pos 0.20 -0.40 -0.30
right rot 50 0 0
left pos -0.20 -0.40 -0.30
left rot 50 0 0
wait 1
mark ready
wait 22
mark placed
head face 1
"""

POSE = "-0.20 -0.35"


def pair(r, l):
    return "right pos %.3f %s\nleft pos %.3f %s\nwait 0.025\n" % (r, POSE, l, POSE)


def rocking():
    """Out 8 cm, in 16, out 16, in 8: four strokes at 0.2 m/s - half a
    centimetre every 0.025 s. A scripted hand jumps, and a whole centimetre
    landing in a short frame read as a swing to the hand mode (2 m/s and
    more over a 4 ms frame; the run of 2026-10-16 lost the armed yield to
    "the right hand swinging" twice in one rocking); half the jump needs a
    frame under 2 ms to read so."""
    out = []
    xs = []
    xs += [0.22 + 0.005 * i for i in range(1, 17)]     # 0.225 .. 0.30
    xs += [0.30 - 0.005 * i for i in range(1, 33)]     # 0.295 .. 0.14
    xs += [0.14 + 0.005 * i for i in range(1, 33)]     # 0.145 .. 0.30
    xs += [0.30 - 0.005 * i for i in range(1, 17)]     # 0.295 .. 0.22
    for x in xs:
        out.append(pair(x, -(0.44 - x)))
    return "".join(out)


def gesture(comment, right_rot, left_rot, mark):
    s = "# %s\n" % comment
    # 1.2 s before the rocking: the jump into the pose reads as a swing, and
    # the yield holds for a second after one.
    s += "right pos 0.22 %s\nright rot %s\nleft pos -0.22 %s\nleft rot %s\nwait 1.2\n" % (POSE, right_rot, POSE, left_rot)
    s += rocking()
    s += "mark %s\nwait 2.5\n" % mark
    return s


def swings():
    s = "# Fast sideways swings of both open hands at the same height: no yield.\n"
    for _ in range(3):
        s += "right pos 0.62 %s\nleft pos 0.18 %s\nwait 0.1\n" % (POSE, POSE)
        s += "right pos 0.22 %s\nleft pos -0.22 %s\nwait 0.1\n" % (POSE, POSE)
        s += "right pos -0.18 %s\nleft pos -0.62 %s\nwait 0.1\n" % (POSE, POSE)
        s += "right pos 0.22 %s\nleft pos -0.22 %s\nwait 0.1\n" % (POSE, POSE)
    s += "mark swung\nwait 3.5\n"
    return s


def draw_sword():
    s = "# The longsword drawn by the holster (the left hip).\n"
    s += "right pos -0.18 -0.62 -0.02\nright rot 50 0 0\nwait 0.3\nright grip 1\nwait 0.2\nright grip 0\n"
    s += "right pos 0.22 %s\nright rot 90 0 90\nwait 2\nmark drawn\n" % POSE
    return s


def main():
    out = HEADER
    out += gesture("The gesture with the hands empty, the palms to the front.", "90 0 90", "90 0 -90", "rocked")
    out += gesture("The hands empty, both open palms turned sideways: the yield all the same.", "90 0 0", "90 0 0",
                   "rocked-sideways")
    out += swings()
    out += draw_sword()
    out += gesture("The gesture with the sword in hand, its palm to the front.", "90 0 90", "90 0 -90", "rocked-armed")
    out += gesture("The sword hand's palm turned sideways, as a guard holds it: no yield.", "90 0 0", "90 0 -90",
                   "armed-sideways")
    out += "mark end\n"
    with open(sys.argv[1], "w", encoding="utf-8") as f:
        f.write(out)
    print("wrote", sys.argv[1], out.count("\n"), "lines")


if __name__ == "__main__":
    main()
