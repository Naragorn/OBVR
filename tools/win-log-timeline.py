#!/usr/bin/env python3
"""Timeline of a Windows OBVR.log, which has no timestamps: the ten-second
"Frame clock" lines cut it into windows; each window gets its frames, mean and
max engine ms, the half-rate episodes that ended in it, the bone rows the lock
replaced, the actor shadows per render, and the marks (loads, config, captures).
Usage: win-log-timeline.py <OBVR.log>
"""
import re, sys
# Timeline of a Windows OBVR.log without timestamps: the Frame clock line comes every ten seconds.
path = sys.argv[1]
win = {"n": 0, "frames": 0, "mean": 0.0, "max": 0.0, "eps": 0, "hrframes": 0, "bone": None, "shadow": None, "marks": []}
rows = []
reFC = re.compile(r"^Frame clock: (\d+) frames, .*given ms mean ([\d.]+) min ([\d.]+) max ([\d.]+)")
reHR = re.compile(r"^Compositor: back to full rate after (\d+) frames at half rate \((\d+) frames at half rate so far in (\d+) episodes\)")
reBone = re.compile(r"^Bone lock at scene call (\d+): replaced (\d+)")
reSh = re.compile(r"^Shadows: actor shadow maps drawn (\d+) times, kept for the second eye (\d+) times in the last (\d+) world renders")
reMark = re.compile(r"^(Save|Load|Cell|World|Hand script|Performance|OpenVR: the headset|Hands: the physics|Walkers|Config)")
total_hr = 0; total_eps = 0; eplens = []
cur = dict(win); idx = 0
for line in open(path, encoding="utf-8", errors="replace"):
    m = reFC.match(line)
    if m:
        idx += 1
        cur["n"] = idx; cur["frames"] = int(m.group(1)); cur["mean"] = float(m.group(2)); cur["max"] = float(m.group(4))
        rows.append(cur); cur = dict(win); cur["marks"] = []
        continue
    m = reHR.match(line)
    if m:
        cur["eps"] += 1; cur["hrframes"] += int(m.group(1)); eplens.append(int(m.group(1))); total_hr = int(m.group(2)); total_eps = int(m.group(3)); continue
    m = reBone.match(line)
    if m: cur["bone"] = int(m.group(2)); continue
    m = reSh.match(line)
    if m: cur["shadow"] = (int(m.group(1)), int(m.group(3))); continue
    if reMark.match(line): cur["marks"].append(line.strip()[:110])
if cur["eps"] or cur["marks"]: cur["n"] = idx + 1; rows.append(cur)
ingame = 0; slow = 0
print("win frames  mean   max  halfrate(eps/frames) bone shadows/render marks")
for r in rows:
    kind = "LOAD" if r["frames"] > 3000 or r["frames"] == 0 else ""
    if not kind: ingame += r["frames"]
    sh = "%.1f" % (r["shadow"][0] / r["shadow"][1]) if r["shadow"] and r["shadow"][1] else "-"
    flag = " <<" if (not kind and r["mean"] > 12.0) else ""
    print("%3d %6d %6.2f %6.1f  %2d/%-5d %5s %5s %s%s" % (r["n"], r["frames"], r["mean"], r["max"], r["eps"], r["hrframes"], r["bone"] if r["bone"] is not None else "-", sh, kind, flag))
    for mk in r["marks"]: print("        | " + mk)
print("in-game frames (windows under 3000):", ingame, " half-rate frames:", total_hr, " episodes:", total_eps, " share: %.1f %%" % (100.0 * total_hr / ingame if ingame else 0))
eplens.sort()
if eplens: print("episode lengths: min", eplens[0], "median", eplens[len(eplens)//2], "max", eplens[-1], " under 30:", sum(1 for e in eplens if e < 30))
