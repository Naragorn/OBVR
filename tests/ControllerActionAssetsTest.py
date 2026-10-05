"""Verify the shipped OpenVR action manifest and both controller bindings."""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "assets" / "input"

manifest = json.loads((INPUT / "actions.json").read_text())
names = {action["name"] for action in manifest["actions"]}
buttons = {
    f"/actions/obvr/in/{hand}_{action}"
    for hand in ("left", "right")
    for action in ("stick_click", "a", "b", "grip", "trackpad", "trigger", "stick")
}
# The hands' skeletons, for the finger curls a fist is read from.
skeletons = {f"/actions/obvr/in/{hand}_skeleton" for hand in ("left", "right")}
# The pulse when a held thing reaches the hand.
haptics = {f"/actions/obvr/out/{hand}_haptic" for hand in ("left", "right")}
assert names == buttons | skeletons | haptics, sorted(names ^ (buttons | skeletons | haptics))
assert all(a["type"] == "vibration" for a in manifest["actions"] if a["name"] in haptics)
assert {binding["controller_type"] for binding in manifest["default_bindings"]} == {
    "knuckles",
    "oculus_touch",
}

for filename, controller_type in (("knuckles.json", "knuckles"), ("oculus_touch.json", "oculus_touch")):
    binding = json.loads((INPUT / filename).read_text())
    assert binding["controller_type"] == controller_type
    sources = binding["bindings"]["/actions/obvr"]["sources"]
    bound = {
        (h["output"], h["path"]) for h in binding["bindings"]["/actions/obvr"]["haptics"]
    }
    assert bound == {
        (f"/actions/obvr/out/{hand}_haptic", f"/user/hand/{hand}/output/haptic") for hand in ("left", "right")
    }, (filename, bound)
    outputs = {
        output["output"]
        for source in sources
        for input_value in source["inputs"].values()
        for output in (input_value,)
    }
    required = {
        f"/actions/obvr/in/{hand}_{action}"
        for hand in ("left", "right")
        for action in ("stick_click", "a", "b", "grip", "trigger", "stick")
    }
    assert required <= outputs, (filename, sorted(required - outputs))

print("Controller action manifest and Index/Touch bindings verified")
