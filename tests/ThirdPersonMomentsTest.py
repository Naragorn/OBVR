"""tools/third-person-moments.py: its reading of a run's marks and its checks
of the option's two states, over made-up logs."""

import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("tp_moments", ROOT / "tools" / "third-person-moments.py")
tp = importlib.util.module_from_spec(spec)
sys.modules["tp_moments"] = tp
spec.loader.exec_module(tp)

failures = 0


def check(condition, what):
    global failures
    print(f"  {'ok  ' if condition else 'FAIL'}  {what}")
    if not condition:
        failures += 1


def mark(name, bytes_="0000", sit=0, knock=0, third=0, level=6.0, up=5.0, alpha=None):
    lines = [f"HandScript: mark {name} (t=1.00 s)",
             f"Hands: at the mark - sit/mount bytes {bytes_}, sit/sleep state {sit}, knock state {knock}, third person {third}",
             f"Camera: at the mark - the body's head {level} units level from the eyes (0.1 {level}), {up} up"]
    if alpha is not None:
        lines.append(f"Camera: at the mark the animation vignette at alpha {alpha}")
    return lines


def run(*marks):
    return tp.read_marks([line for m in marks for line in m])


print("third-person moments")
off = run(mark("ready"), mark("sit", "0001", 3, 0, 1, 4.0, 5.0, 0.7), mark("after"))
check([k.moment for k in off] == [False, True, False], "the moment read from the bytes and the state")
check(tp.check(off, "off") == [], "off: the eyes in the head, the vignette up, first person after: pass")
check(tp.check(off, "on") != [], "the same run read as on: fails (the eyes in the head)")
on = run(mark("ready"), mark("down", knock=3, third=1, level=200.0, up=10.0, alpha=0.7), mark("up"))
check(tp.check(on, "on") == [], "on: third person, the eyes 200 off, first person after: pass")
check(tp.check(on, "off") != [], "the same run read as off: fails")
novig = run(mark("sit", "0001", 3, 0, 1, 4.0, 5.0), mark("after"))
check(any("no vignette" in p for p in tp.check(novig, "off")), "off without the vignette: named")
stuck = run(mark("sit", "0001", 3, 0, 1, 200.0, 5.0, 0.7), mark("after", third=1, level=200.0))
check(any("after" in p for p in tp.check(stuck, "on")), "still third person after the moment: named")
check(tp.check(run(mark("ready")), "on") == ["no mark fell inside an engine moment"], "no moment: named")
check(tp.check(run(mark("sit", "0001", 3, 0, 1, 200.0, 5.0)), "on") == ["no mark after the last moment"],
      "no mark after: named")
check(run(mark("feed", "0100"))[0].moment and run(mark("get", sit=5))[0].moment, "the get-up byte and state 5 are moments")
check(not run(mark("seated", sit=4))[0].moment and not run(mark("unread", knock=-1))[0].moment,
      "seated (4) and an unread knock state are not")
print(f"{failures} check(s) failed" if failures else "all checks passed")
sys.exit(1 if failures else 0)
