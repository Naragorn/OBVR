"""Verify the shipped OpenVR action manifest and both controller bindings."""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "assets" / "input"

manifest = json.loads((INPUT / "actions.json").read_text())
actions = {action["name"]: action for action in manifest["actions"]}
names = set(actions)
buttons = {
    f"/actions/obvr/in/{hand}_{action}"
    for hand in ("left", "right")
    for action in ("stick_click", "a", "b", "grip", "trackpad", "trigger", "stick")
}
# The grip's analog signals (docs/free-weapons-spec.md section 5.4): the pull
# every analog grip has - an Index's capacitive value, a Touch's grip trigger -
# and the force an Index's sensor reads. Optional: a controller without one
# leaves the action inactive, and OBVR reads 0 for it.
grip_signals = {
    f"/actions/obvr/in/{hand}_{signal}"
    for hand in ("left", "right")
    for signal in ("grip_pull", "grip_force")
}
# The hands' skeletons, for the finger curls a fist is read from.
skeletons = {f"/actions/obvr/in/{hand}_skeleton" for hand in ("left", "right")}
# The pulse when a held thing reaches the hand.
haptics = {f"/actions/obvr/out/{hand}_haptic" for hand in ("left", "right")}
expected = buttons | grip_signals | skeletons | haptics
assert names == expected, sorted(names ^ expected)
assert all(actions[name]["type"] == "vibration" for name in haptics)
assert all(
    actions[name]["type"] == "vector1" and actions[name]["requirement"] == "optional" for name in grip_signals
), sorted(name for name in grip_signals if actions[name]["type"] != "vector1" or actions[name]["requirement"] != "optional")
# Every action has its words in SteamVR's binding UI.
localized = set(manifest["localization"][0]) - {"language_tag"}
assert names <= localized, sorted(names - localized)
assert {binding["controller_type"] for binding in manifest["default_bindings"]} == {
    "knuckles",
    "oculus_touch",
}

pulls = {f"/actions/obvr/in/{hand}_grip_pull" for hand in ("left", "right")}
forces = {f"/actions/obvr/in/{hand}_grip_force" for hand in ("left", "right")}

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
    # The grip's pull on both controllers, as the trigger mode's pull.
    assert pulls <= outputs, (filename, sorted(pulls - outputs))
    grip_sources = [s for s in sources if s["path"].endswith("/input/grip")]
    pull_modes = {s["mode"] for s in grip_sources if "pull" in s["inputs"]}
    assert pull_modes == {"trigger"}, (filename, pull_modes)
    if controller_type == "knuckles":
        # The force sensor in the force_sensor mode, and the click as before:
        # the button mode's click made from the force at 0.3, let go at 0.2.
        assert forces <= outputs, (filename, sorted(forces - outputs))
        force_modes = {s["mode"] for s in grip_sources if "force" in s["inputs"]}
        assert force_modes == {"force_sensor"}, (filename, force_modes)
        clicks = [s for s in grip_sources if "click" in s["inputs"]]
        assert len(clicks) == 2 and all(
            s["mode"] == "button"
            and s["parameters"]["force_input"] == "force"
            and s["parameters"]["click_activate_threshold"] == "0.3"
            and s["parameters"]["click_deactivate_threshold"] == "0.2"
            for s in clicks
        ), (filename, clicks)
    else:
        # A Touch has no force sensor: nothing bound, the action stays inactive.
        assert not (forces & outputs), (filename, sorted(forces & outputs))

print("Controller action manifest and Index/Touch bindings verified")
