#!/usr/bin/env python3
# Writes tools/hand-scripts/yield-gear.txt (the yield gesture with and without gear):
#   python3 tools/gen-yield-gear-script.py tools/hand-scripts/yield-gear.txt
import sys
out=[]
w=out.append
w("""# The yield by gesture with and without gear (vr/Yield.h, yield_test;
# 2026-11-11): both hands open, stretched toward the attacker with the palms
# to the front (right rot 90 0 90, left rot 90 0 -90), then
# waved like a wiper - both controllers tilted 18 degrees either way about
# the axis to the front, together, a full wave in 1 s, four waves (the hands
# stay where they are; the fingertips move 3.7 cm either way).
# A melee bandit (00069AD1) attacks an untouchable player (tgm). (a) the hands empty: the yield. (b) the longsword drawn by
# the left hip, the gesture with it in hand: the yield. (c) a shield
# (000733ED) equipped too - on the arm while the sword is drawn, the left
# hand round its strap (closed): the yield, since 2026-11-11 (the shield
# hand counted as holding, vr::YieldHandHolds). (d) a block-and-swing series:
# the shield hand held up, the sword hand swung hard between - no yield.
# Eye pictures at every mark ([Debug] EyeDump): up, waved right, waved
# left, the yield.
# Run with: python3 tools/hand-script-run.py --headless tools/hand-scripts/yield-gear.txt

ini Hands Enabled=1
ini Hands Holsters=1
ini Hands CombatReach=85
ini Hands OffHandStrikes=0
ini Hands Yield=1
ini Debug EyeDump=1
# Both hands open at the start (curl 0); the sword hand closes round the
# controller on the draw, the shield hand round the strap - each counted as
# holding, not asked to open.
right curl 0
left curl 0
console-at ready CloseAllMenus
console-at ready set timescale to 0
console-at ready set gamehour to 12
console-at ready fw 00038EEE
console-at ready player.placeatme 00069AD1 1
console-at ready player.additem 00000C0C 1
console-at ready player.equipitem 00000C0C
console-at ready tgm
console-at shield player.additem 000733ED 1
console-at shield player.equipitem 000733ED

expect HandScript: finished
count 3 Yield: the open hands rocked at someone in combat
expect the hands empty)
count 2 (a weapon in hand)
expect Yield: activate goes down
reject menu Container
# The shield hand closed round its strap is no gate: the third yield at
# shield-yield, "hand curls 1.00/1.00" at its mark (the pose lines run out
# before this phase, 60 a session).
reject HandScript: WARNING
""")
def ready():
    w("""wait 12
right pos 0.20 -0.40 -0.30
right rot 50 0 0
left pos -0.20 -0.40 -0.30
left rot 50 0 0
wait 1
mark ready
wait 22
mark placed
head face 1
wait 2
# The head faces the bandit once, then holds its yaw (face 0): the script's
# hands follow the head's heading, so a head that kept turning would carry
# them round it - which no real hand does - and the yield reads the hands in
# the body's frame since 2026-11-11 (vr/YieldBody.h): turned with the head
# they moved in the room, and that counted as a wave.
head face 0
""")
Y,Z=-0.12,-0.50  # hands at chest height, stretched forward
def pose(rx=0.20,lx=-0.20):
    w(f"right pos {rx:.3f} {Y} {Z}\nright rot 90 0 90\nleft pos {lx:.3f} {Y} {Z}\nleft rot 90 0 -90\n")
import math
def _rx(a):
    c,s_=math.cos(a),math.sin(a); return [[1,0,0],[0,c,-s_],[0,s_,c]]
def _ry(a):
    c,s_=math.cos(a),math.sin(a); return [[c,0,s_],[0,1,0],[-s_,0,c]]
def _rz(a):
    c,s_=math.cos(a),math.sin(a); return [[c,-s_,0],[s_,c,0],[0,0,1]]
def _mul(a,b):
    return [[sum(a[i][k]*b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
def wiper_rot(roll_base, tilt_deg):
    """The script's rot (pitch yaw roll; yaw * pitch * roll, HandScriptPose.h)
    of a controller pointing up (pitch 90) with its palm side to the front
    (roll_base), turned about the axis to the front (OpenVR z) by tilt_deg -
    a wiper's tilt."""
    m=_mul(_rz(math.radians(tilt_deg)),_mul(_rx(math.radians(90)),_rz(math.radians(roll_base))))
    p=math.asin(max(-1,min(1,-m[1][2])))
    y=math.atan2(m[0][2],m[2][2])
    r=math.atan2(m[1][0],m[1][1])
    return f"{math.degrees(p):.1f} {math.degrees(y):.1f} {math.degrees(r):.1f}"
def rock(tag, amp=18.0, steps=10, cycles=4, dt=0.025):
    """The wave: both hands still, tilted like a wiper `amp` degrees either way,
    together, `steps` steps of `dt` a quarter stroke (0.25 s: about 1 Hz a
    full wave)."""
    w("""# Face the bandit again before the pose, the palms turned away meanwhile
# (face 0 holds the yaw during the gesture - see above).
right rot 50 0 0
left rot 50 0 0
head face 1
wait 1.5
head face 0
""")
    pose(); w("wait 1.2\n"); w(f"mark {tag}-up\n")
    seq=[]
    for c in range(cycles):
        seq += [amp*(k+1)/steps for k in range(steps)]
        seq += [amp - 2*amp*(k+1)/(2*steps) for k in range(2*steps)]
        seq += [-amp + amp*(k+1)/steps for k in range(steps)]
    for i,a in enumerate(seq):
        w(f"right rot {wiper_rot(90,a)}\nleft rot {wiper_rot(-90,a)}\nwait {dt}\n")
        if i==steps-1: w(f"mark {tag}-right\n")
        if i==3*steps-1: w(f"mark {tag}-left\n")
    w(f"right rot 90 0 90\nleft rot 90 0 -90\nwait 0.3\nmark {tag}-yield\nwait 4\n")
ready()
w("# (a) The hands empty.\n")
rock("empty")
w("""# (b) The longsword drawn by the holster (the left hip), the grip kept closed.
right pos -0.18 -0.62 -0.02
right rot 50 0 0
wait 0.3
right grip 1
right curl 1
wait 0.2
right pos 0.20 -0.12 -0.50
right rot 90 0 90
wait 2
mark sword-drawn
""")
rock("sword")
w("""# (c) The shield equipped: on the left arm while the sword is drawn, the left hand round its strap.
mark shield
wait 3
left grip 1
left curl 1
wait 1
mark shield-on
""")
rock("shield")
w("""# (d) A block-and-swing series: the shield up and still, the sword hand swung hard between - no yield.
left pos -0.15 -0.05 -0.40
left rot 90 0 -90
""")
for k in range(6):
    w("right pos 0.45 0.05 -0.30\nwait 0.08\nright pos -0.10 -0.30 -0.55\nwait 0.08\nright pos 0.20 -0.12 -0.50\nwait 0.4\n")
w("mark swings\nwait 2\nmark end\n")
w("""# The pose held still for 5 s more after the phases, the bandit at it: no
# yield (2026-11-11: a fourth came here, uncounted, while the hands were read
# against a turning head). The count covers it.
right pos 0.20 -0.12 -0.50
right rot 90 0 90
left pos -0.20 -0.12 -0.50
left rot 90 0 -90
wait 5
mark after
""")
open(sys.argv[1],'w').write("".join(out))
