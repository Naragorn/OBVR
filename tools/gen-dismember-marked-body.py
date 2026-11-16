#!/usr/bin/env python3
# Writes tools/hand-scripts/dismember-marked-body.txt (every part cut off a body that was struck first).
# Run: python3 tools/gen-dismember-marked-body.py
from pathlib import Path

REST = ["right rot 0 0 0", "right pos 0.20 -0.40 -0.30"]


def stab(name):
    out = [f"mark {name}", "right rot 0 0 0", "right pos 0.05 -0.55 -0.30", "wait 0.5"]
    for i in range(40, 121, 10):
        out += [f"right pos 0.05 -0.55 -{i / 100:.2f}", "wait 0.01"]
    return out + ["wait 0.5", "right pos 0.05 -0.55 -0.30", "wait 1"]


def slash(name, roll):
    out = [f"mark {name}", f"right rot 0 0 {roll}", "right pos 0.60 -0.55 -0.85", "wait 0.8"]
    for i in range(50, -61, -10):
        out += [f"right pos {i / 100:.2f} -0.55 -0.85", "wait 0.01"]
    return out + ["wait 0.5"] + REST + ["wait 1"]


def marked(tag):
    return stab(f"{tag}_stab") + slash(f"{tag}_slash", 0) + slash(f"{tag}_slash_rolled", 90)


def killed_and_cut(n, cuts):
    out = [f"mark marked_{n}", "wait 0.5", f"mark kill_{n}", "wait 3", "head pitch -45", "wait 0.5",
           f"mark lying_{n}", "wait 0.5"]
    for cut in cuts:
        out += [f"mark {cut}", "wait 1.5", f"mark after_{n}_{cut}", "wait 0.5"]
    return out + [f"mark end_{n}", "head pitch 0", "wait 0.5"]


HEAD = '''# Every part cut off a body that was struck first (docs/dismemberment-spec.md, "2026-11-16: crash at a hand
# cut off a marked body"): every dismember scenario before this one cut a body with no mark on it, and the game
# crashed in the headset at the first hand cut off a corpse with cut marks and a blood stain on its skin - the
# engine's decal shapes were copied for the part, and the decal shader reads a copy's empty decal list.
# Here each body is marked as in a fight first, by the real blade (blade-entry.txt's fast stab and its two
# slashes across the chest; MotionHits=0, so no hit is handed to the game and the beggar lives), then killed,
# then cut by [Debug] TestCut. One cut a limb, so the eleven parts are spread over three bodies, each disabled
# before the next is placed:
#   body 1, stripped (the marks on its skin):   the left hand, the right arm, the left lower leg, the right foot
#   body 2, in its clothes (the marks on them): the left forearm, the right hand, the left foot, the right lower leg
#   body 3, stripped:                           the left arm, the right forearm, the head
# After each cut the world is drawn for 1.5 s (the crash came in the cut's own frame). Must be seen: marks and
# a stain laid, every cut made, and at least one decal shape left out of a part's copies ("the decal shape ...
# is not drawn again for the part") - without that line no decal shape held a bone of a cut part and the run
# proved nothing. The eye pictures at the after_* marks show the parts on the ground.
# The game's decals fade after fDecalLifetime (10 s in the game's INI): in the first run (20261116-144150) the
# marks were gone before the cuts and no decal shape was left to copy. The lifetime is 600 s for the run and
# set back to 10 at the end mark (the engine may write its INI at exit).
# Written by tools/gen-dismember-marked-body.py.
# Run with: python3 tools/hand-script-run.py --headless tools/hand-scripts/dismember-marked-body.txt

ini Hands Enabled=1
ini Hands Holsters=1
ini Hands MotionHits=0
ini Hands WeaponStopsAtWalls=1
ini Hands WeaponStopsAtBodies=1
ini Hands SwingsStopAtBodies=1
ini Hands PushPeople=0
ini Hands FittedBodyCapsules=1
ini Hands BladeSlidesOffPeople=1
ini Hands BladeEntry=1
ini Hands BladeEntryCutMarks=1
ini Hands Dismemberment=1
ini Debug TestCut=1
ini Render MonitorMenusOnly=0
ini Debug EyeDump=1
ini Debug EyeDumpDivisor=2
console-at ready CloseAllMenus
console-at ready tgm
console-at ready player.setangle z 0
console-at ready player.placeatme 0001C45B 1 120 0
console-at ready player.removeitem 000229B3 1
console-at ready player.additem 00000C0C 1
console-at ready player.equipitem 00000C0C
# tai is a switch: off while the blade marks a beggar, on again in the frame he is killed.
console-at ready tai
console-at ready SetNumericINISetting "fDecalLifetime:Display" 600
console-at end SetNumericINISetting "fDecalLifetime:Display" 10
console-at placed_1 {actor}.setav health 100000
console-at placed_1 {actor}.removeallitems
console-at kill_1 tai
console-at kill_1 {actor}.kill
console-at end_1 {actor}.disable
console-at next_2 player.placeatme 0001C45B 1 120 0
console-at next_2 tai
console-at placed_2 {actor}.setav health 100000
console-at kill_2 tai
console-at kill_2 {actor}.kill
console-at end_2 {actor}.disable
console-at next_3 player.placeatme 0001C45B 1 120 0
console-at next_3 tai
console-at placed_3 {actor}.setav health 100000
console-at placed_3 {actor}.removeallitems
console-at kill_3 tai
console-at kill_3 {actor}.kill

expect HandScript: finished
expect Blade entry:
expect skin (0 points)
expect cloth (0 points)
expect Cut mark:
expect asked of the engine on "
expect a stain laid
expect Dismember: the engine's joint and list functions as read
count 2 Dismember: test hand cut at the mark - cut
expect 's left hand cut - 
expect 's right hand cut - 
count 2 Dismember: test forearm cut at the mark - cut
expect 's left forearm cut - 1 elbow joint(s) released
expect 's right forearm cut - 1 elbow joint(s) released
count 2 Dismember: test arm cut at the mark - cut
expect 's left arm cut - 1 shoulder joint(s) released
expect 's right arm cut - 1 shoulder joint(s) released
count 2 Dismember: test leg cut at the mark - cut
expect 's left lower leg cut - 1 knee joint(s) released
expect 's right lower leg cut - 1 knee joint(s) released
count 2 Dismember: test foot cut at the mark - cut
expect 's left foot cut - 1 ankle joint(s) released
expect 's right foot cut - 1 ankle joint(s) released
expect Dismember: test cut at the mark - cut
expect 's head cut - 1 neck joint(s) released
expect Dismember: the decal shape "Decal
expect is not drawn again for the part
reject NOT copied
reject HandScript: WARNING
reject Necks: the bound left out
reject CRASH
'''

body = ["wait 12"] + REST + ["left pos -0.20 -0.40 -0.30", "wait 1", "mark ready", "wait 12", "mark placed_1",
                             "right pos 0.18 -0.62 -0.02", "wait 0.3", "right grip 1", "wait 0.2",
                             "right pos 0.20 -0.40 -0.30", "wait 3", "mark drawn"]
body += marked("skin") + killed_and_cut(1, ["cut_hand_far", "cut_arm_far", "cut_leg_far", "cut_foot"])
body += ["mark next_2", "wait 4", "mark placed_2", "wait 0.5"]
body += marked("cloth") + killed_and_cut(2, ["cut_forearm_far", "cut_hand_right", "cut_foot_far", "cut_leg_right"])
body += ["mark next_3", "wait 4", "mark placed_3", "wait 3"]
body += marked("skin3") + killed_and_cut(3, ["cut_arm_left", "cut_forearm_right", "cut"])
body += ["mark end", "wait 1"]

out = Path(__file__).resolve().parent / "hand-scripts" / "dismember-marked-body.txt"
out.write_text(HEAD + "\n" + "\n".join(body) + "\n")
print(f"wrote {out} ({len(body)} lines of motion)")
