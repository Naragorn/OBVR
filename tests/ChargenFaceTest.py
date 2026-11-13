#!/usr/bin/env python3
"""Checks tools/chargen-face.py: every flow of its verdict."""
import importlib.util
import pathlib
import sys

spec = importlib.util.spec_from_file_location(
    "chargen_face", pathlib.Path(__file__).resolve().parent.parent / "tools" / "chargen-face.py")
cf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cf)

failures = 0


def check(cond, what):
    global failures
    print(("  ok    " if cond else "  FAIL  ") + what)
    if not cond:
        failures += 1


opening = ("Chargen view: laid before the head as it stands - the head turned 60.0 degrees from the recentre "
           "(left positive), stood 0.0 0.0 units from its place; the face -20.1 degrees from the view (left positive; "
           "the engine's own aim -20.1)")
later = "Chargen view: the face -21.5 degrees from the view (left positive), 450 frames into the menu"
off = ("Chargen view: not laid before the head (ChargenFacesHead off or no head read) - the head turned 60.0 degrees "
       "from the recentre (left positive), stood 0.0 0.0 units from its place; the face -80.1 degrees from the view")
other = "Chargen view: the race menu is up - the eyes 63.0 units (0.90 m) in front of the face"

check(cf.face_angles([opening, later, other]) == [-20.1, -21.5], "the opening's and a later angle read, others not")
check(cf.verdict([-20.1, -21.5], 3.0)[0], "within 3 degrees of the engine's aim: pass")
check(not cf.verdict([-20.1, -80.1], 3.0)[0], "one 60 degrees off: fail")
check(not cf.verdict(cf.face_angles([off]), 3.0)[0], "not laid before the head, the head turned: fail")
check(not cf.verdict([], 3.0)[0], "no line: fail")
check(cf.verdict([-17.5], 3.0)[0] and not cf.verdict([-16.9], 3.0)[0], "the bound: 2.6 off passes, 3.2 off fails")

print("all checks passed" if failures == 0 else f"{failures} check(s) failed")
sys.exit(1 if failures else 0)
