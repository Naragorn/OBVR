"""The controls matrix's generator and reader (tools/gen-controls-matrix.py,
tools/controls-matrix.py) on logs made from the table: every row ok, an action
that did not fire, an old input that still fired it, a missing note, a
missing run, and the written scripts current."""
import importlib.util
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / file)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


reader = load("controls_matrix", "controls-matrix.py")
gen = load("gen_controls_matrix", "gen-controls-matrix.py")
table = load("controls_matrix_table", "controls_matrix_table.py")


def good_log(run):
    lines = [run["config"], *run["notes"], "HandScript: finished"]
    for step in run["steps"]:
        if step["fires"]:
            lines.append(table.fire_line(step["fires"], step["input"]))
        lines.extend(step.get("extra", []))
    return "\n".join(lines) + "\n"


class ControlsMatrixTest(unittest.TestCase):
    def write(self, folder, name, text):
        d = os.path.join(folder, name)
        os.makedirs(d)
        with open(os.path.join(d, "OBVR.log"), "w") as f:
            f.write(text)
        return d

    def run_reader(self, texts):
        with tempfile.TemporaryDirectory() as tmp:
            paths = [self.write(tmp, "r%d" % i, t) for i, t in enumerate(texts)]
            return reader.main(paths)

    def test_all_good(self):
        self.assertEqual(self.run_reader([good_log(r) for r in table.RUNS]), 0)

    def test_not_fired(self):
        run = table.RUNS[0]
        text = good_log(run).replace(table.fire_line("Jump", "RightA"), "")
        self.assertEqual(self.run_reader([text, good_log(table.RUNS[1])]), 1)

    def test_old_input_still_fires(self):
        run = table.RUNS[0]
        text = good_log(run) + table.fire_line("Jump", "RightB") + "\n"
        self.assertEqual(self.run_reader([text, good_log(table.RUNS[1])]), 1)

    def test_note_missing(self):
        run = table.RUNS[1]
        text = good_log(run).replace(run["notes"][0], "")
        self.assertEqual(self.run_reader([good_log(table.RUNS[0]), text]), 1)

    def test_warning(self):
        text = good_log(table.RUNS[0]) + "HandScript: WARNING something\n"
        self.assertEqual(self.run_reader([text, good_log(table.RUNS[1])]), 1)

    def test_missing_run(self):
        self.assertEqual(self.run_reader([good_log(table.RUNS[0])]), 1)

    def test_unknown_run(self):
        self.assertEqual(self.run_reader(["nothing here\n"]), 1)

    def test_every_action_swapped_once(self):
        # Every action OBVR_Controls.ini knows is put on a new input, or unbound
        # ("Jump=None"), in some run (vr::ControlAction).
        actions = ["Jump", "Kick", "PauseMenu", "Menus", "Activate", "Sneak", "ReadyWeapon", "Run", "Journal",
                   "TogglePov", "QuickSave"]
        bound = set()
        for run in table.RUNS:
            for line in run["buttons"]:
                bound.add(line.split("=")[0])
        self.assertEqual(sorted(bound), sorted(actions))

    def test_every_step_input_pressable(self):
        for run in table.RUNS:
            for step in run["steps"]:
                self.assertIn(step["input"], table.PRESS)
                self.assertIn(step["input"], table.PHRASE)

    def test_scripts_current(self):
        self.assertEqual(gen.main(["--check"]), 0)

    def test_script_expects_and_rejects(self):
        text = gen.script_for(table.RUNS[0])
        self.assertIn("expect Buttons: Jump from the right A", text)
        self.assertIn("reject Buttons: Jump from the right B", text)
        self.assertIn("ini Buttons Jump=RightA", text)
        self.assertIn("console-at closed-11 CloseAllMenus", text)


if __name__ == "__main__":
    unittest.main()
