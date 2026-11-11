#!/usr/bin/env python3
# Writes tools/hand-scripts/yield-gear.txt (the yield gesture with and without gear):
#   python3 tools/gen-yield-gear-script.py tools/hand-scripts/yield-gear.txt
import sys
out=[]
w=out.append
w("""# The yield by gesture with and without gear (vr/Yield.h, yield_test;
# 2026-11-11): both hands open, stretched toward the attacker with the palms
# to the front (right rot 90 0 90, left rot 90 0 -90), then rocked left and
# right together like a "no no no" - 6 cm either way, 0.6 cm every 0.025 s
# (0.24 m/s), four rocks. A melee bandit (00069AD1) attacks an untouchable
# player (tgm). (a) the hands empty: the yield. (b) the longsword drawn by
# the left hip, the gesture with it in hand: the yield. (c) a shield
# (000733ED) equipped too - on the arm while the sword is drawn, the left
# hand round its strap (closed): the yield, since 2026-11-11 (the shield
# hand counted as holding, vr::YieldHandHolds). (d) a block-and-swing series:
# the shield hand held up, the sword hand swung hard between - no yield.
# Eye pictures at every mark ([Debug] EyeDump): up, rocked left, rocked
# right, the yield.
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
""")
Y,Z=-0.12,-0.50  # hands at chest height, stretched forward
def pose(rx=0.20,lx=-0.20):
    w(f"right pos {rx:.3f} {Y} {Z}\nright rot 90 0 90\nleft pos {lx:.3f} {Y} {Z}\nleft rot 90 0 -90\n")
def rock(tag, amp=0.06, step=0.006, cycles=4, dt=0.025):
    pose(); w("wait 1.2\n"); w(f"mark {tag}-up\n")
    x=0.0; n=int(round(amp/step))
    seq=[]
    for c in range(cycles):
        seq += [ +1]*n + [-1]*(2*n) + [+1]*n
    for i,d in enumerate(seq):
        x+=d*step
        w(f"right pos {0.20+x:.3f} {Y} {Z}\nleft pos {-0.20+x:.3f} {Y} {Z}\nwait {dt}\n")
        if c_mark(i,n,seq):
            w(f"mark {tag}-{c_mark(i,n,seq)}\n")
    w(f"wait 0.3\nmark {tag}-yield\nwait 4\n")
def c_mark(i,n,seq):
    # first cycle: at the right extreme and the left extreme
    if i==n-1: return "right"
    if i==3*n-1: return "left"
    return None
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
open(sys.argv[1],'w').write("".join(out))
