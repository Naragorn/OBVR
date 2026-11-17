"""Checks the pure part of the harness's parallel runs (tools/harness_mirror.py): what
a slot's mirror of the game folder links, copies and leaves out, which game process is
the run's own, which of the player's files the slot's prefix takes again at every run,
and when a parallel run is refused."""

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import harness_mirror as m  # noqa: E402


class MirrorPlan(unittest.TestCase):
    def plan(self):
        return m.mirror_plan(
            root=[("Data", True), ("directx", True), ("Crash Logs", True), ("OBNG-logs", True),
                  ("OBVR-performance-1-2", True), ("OBVR-backup", True),
                  ("Oblivion.exe", False), ("OblivionLauncher.exe", False), ("Version.dll", False),
                  ("dxvk.conf", False), ("OBVR.ini", False), ("OBVR.log", False), ("OBVR.log.prev", False),
                  ("AddActorValues.temp", False), ("OBVR-samples.csv", False), ("OBVR-samples-modules.csv", False)],
            data=["Oblivion.esm", "Meshes", "OBSE", "Oblivion - Meshes.bsa"],
            obse=["Plugins", "obse.ini", "WalkersDisabled"])

    def test_the_way_to_the_plugins_is_real_folders(self):
        plan = self.plan()
        self.assertEqual(plan["Data"], "dir")
        self.assertEqual(plan["Data/OBSE"], "dir")

    def test_the_plugins_are_a_copy_of_the_whole_folder(self):
        # Plugins write beside themselves (logs, caches, the dome's atlas): a link would write into the player's game.
        self.assertEqual(self.plan()["Data/OBSE/Plugins"], "tree")

    def test_everything_only_read_is_a_link(self):
        plan = self.plan()
        for rel in ("directx", "Data/Oblivion.esm", "Data/Meshes", "Data/Oblivion - Meshes.bsa",
                    "Data/OBSE/obse.ini", "Data/OBSE/WalkersDisabled"):
            self.assertEqual(plan[rel], "link", rel)

    def test_the_game_folders_own_files_are_copies(self):
        plan = self.plan()
        for rel in ("Oblivion.exe", "OblivionLauncher.exe", "Version.dll", "dxvk.conf", "OBVR.ini"):
            self.assertEqual(plan[rel], "copy", rel)

    def test_what_a_game_writes_is_left_out_or_starts_empty(self):
        plan = self.plan()
        for rel in ("OBVR.log", "OBVR.log.prev", "AddActorValues.temp", "OBVR-samples.csv",
                    "OBVR-samples-modules.csv", "OBVR-performance-1-2", "OBVR-backup"):
            self.assertEqual(plan[rel], "skip", rel)
        self.assertEqual(plan["Crash Logs"], "dir")
        self.assertEqual(plan["OBNG-logs"], "dir")

    def test_names_in_another_case(self):
        plan = m.mirror_plan(root=[("data", True), ("OBVR.LOG", False)], data=["obse", "meshes"], obse=["plugins"])
        self.assertEqual(plan, {"data": "dir", "OBVR.LOG": "skip", "data/obse": "dir", "data/meshes": "link",
                                "data/obse/plugins": "tree"})

    def test_an_install_without_obse_has_no_plugins_to_copy(self):
        plan = m.mirror_plan(root=[("Data", True)], data=["Oblivion.esm"], obse=[])
        self.assertEqual(plan, {"Data": "dir", "Data/Oblivion.esm": "link"})


class PluginsLeftOut(unittest.TestCase):
    def test_a_runs_own_files_and_what_plugins_wrote(self):
        for name in ("OBVR-test.ini", "OBVR-HandScript.txt", "openvr_mock.dll", "Modern Engine Fixes.log",
                     "SkyBSA.LOG", "OBVR-Recording-20261114-190905.txt"):
            self.assertTrue(m.plugin_left_out(name), name)

    def test_everything_else_stays(self):
        for name in ("OBVR.dll", "OBVR.ini", "OBVR.map", "openvr_api.dll", "OBVR-crosshair.cache", "OBVR_Stage",
                     "OBNG-unused-bones.txt", "OBVR_Hints.ini"):
            self.assertFalse(m.plugin_left_out(name), name)


class OwnGame(unittest.TestCase):
    ROOT = "/home/n/.local/share/obvr-harness"
    LIVE = "/home/n/.local/share/Steam/steamapps/common/Oblivion"
    SLOT1 = ROOT + "/slot-1/game"
    SLOT2 = ROOT + "/slot-2/game"

    def test_a_parallel_run_knows_only_the_game_in_its_own_folder(self):
        games = [(10, self.LIVE), (20, self.SLOT1), (30, self.SLOT2)]
        self.assertEqual(m.own_game_pid(games, self.SLOT2, self.ROOT, parallel=True), 30)
        self.assertEqual(m.own_game_pid(games, self.SLOT1, self.ROOT, parallel=True), 20)
        self.assertIsNone(m.own_game_pid([(10, self.LIVE), (20, self.SLOT1)], self.SLOT2, self.ROOT, parallel=True))

    def test_a_trailing_slash_is_the_same_folder(self):
        self.assertEqual(m.own_game_pid([(20, self.SLOT1 + "/")], self.SLOT1, self.ROOT, parallel=True), 20)

    def test_a_plain_run_knows_every_game_that_is_not_a_slots(self):
        games = [(20, self.SLOT1), (10, self.LIVE), (30, self.SLOT2)]
        self.assertEqual(m.own_game_pid(games, self.LIVE, self.ROOT, parallel=False), 10)
        self.assertIsNone(m.own_game_pid([(20, self.SLOT1), (30, self.SLOT2)], self.LIVE, self.ROOT, parallel=False))
        # A game started from somewhere else altogether still counts, as before the slots.
        self.assertEqual(m.own_game_pid([(40, "/mnt/other/Oblivion")], self.LIVE, self.ROOT, parallel=False), 40)

    def test_a_game_whose_folder_cannot_be_read(self):
        # Counted by a plain run (it may be the player's), never by a parallel one.
        self.assertEqual(m.own_game_pid([(50, None)], self.LIVE, self.ROOT, parallel=False), 50)
        self.assertIsNone(m.own_game_pid([(50, None)], self.SLOT1, self.ROOT, parallel=True))

    def test_a_folder_that_only_begins_like_the_slots_root(self):
        self.assertEqual(m.own_game_pid([(60, self.ROOT + "-old/game")], self.LIVE, self.ROOT, parallel=False), 60)

    def test_no_game(self):
        self.assertIsNone(m.own_game_pid([], self.LIVE, self.ROOT, parallel=False))
        self.assertIsNone(m.own_game_pid([], self.SLOT1, self.ROOT, parallel=True))


class PrefixRefresh(unittest.TestCase):
    def test_the_settings_the_load_order_and_the_runs_saves(self):
        files = m.prefix_refresh(["OBVRHarness.ess", "OBVRHarness.obse", "OBVRHarness.bak", "Save 12.ess",
                                  "OBVRHarnessMOO.ess", "Walkers1.ess", "Walkers1.obse"], save="Walkers1")
        self.assertEqual(files, [
            "Documents/My Games/Oblivion/Oblivion.ini",
            "AppData/Local/Oblivion/Plugins.txt",
            "Documents/My Games/Oblivion/Saves/OBVRHarness.ess",
            "Documents/My Games/Oblivion/Saves/OBVRHarness.obse",
            "Documents/My Games/Oblivion/Saves/OBVRHarnessMOO.ess",
            "Documents/My Games/Oblivion/Saves/Walkers1.ess",
            "Documents/My Games/Oblivion/Saves/Walkers1.obse",
        ])

    def test_no_save_named_or_a_harness_save_named(self):
        base = ["Documents/My Games/Oblivion/Oblivion.ini", "AppData/Local/Oblivion/Plugins.txt",
                "Documents/My Games/Oblivion/Saves/OBVRHarness.ess"]
        for save in ("", "continue", "OBVRHarness"):
            self.assertEqual(m.prefix_refresh(["OBVRHarness.ess", "Save 12.ess"], save=save), base, save)

    def test_a_named_save_that_is_not_there(self):
        self.assertEqual(m.prefix_refresh([], save="Gone"),
                         ["Documents/My Games/Oblivion/Oblivion.ini", "AppData/Local/Oblivion/Plugins.txt"])


class Slots(unittest.TestCase):
    def test_the_slots_folders(self):
        self.assertEqual(m.slot_name(3), "slot-3")
        self.assertGreaterEqual(m.SLOTS, 2)


class ParallelProblem(unittest.TestCase):
    def ok(self, **changes):
        args = dict(parallel=True, no_steamvr=True, headless=True, plugins=[], game_dir_given=False)
        args.update(changes)
        return m.parallel_problem(**args)

    def test_a_parallel_run_as_it_should_be(self):
        self.assertIsNone(self.ok())
        self.assertIsNone(self.ok(plugins=["/b/OBVR.dll"]))

    def test_a_plain_run_needs_nothing(self):
        self.assertIsNone(m.parallel_problem(parallel=False, no_steamvr=False, headless=False, plugins=[],
                                             game_dir_given=True))

    def test_it_needs_the_stand_in_runtime(self):
        self.assertEqual(self.ok(no_steamvr=False),
                         "--parallel needs --no-steamvr: SteamVR takes one game at a time and ends the older one")

    def test_it_needs_the_headless_start(self):
        self.assertEqual(self.ok(headless=False),
                         "--parallel needs --headless: two games' windows would fight for the desktop's focus")

    def test_it_makes_its_own_game_folder(self):
        self.assertEqual(self.ok(game_dir_given=True),
                         "--parallel makes its own game folder; --game-dir cannot be given with it")

    def test_plugins_go_into_a_mirror_only(self):
        self.assertEqual(
            m.parallel_problem(parallel=False, no_steamvr=True, headless=True, plugins=["/b/OBVR.dll"],
                               game_dir_given=False),
            "--plugin puts a file into a parallel run's own game folder; without --parallel it would overwrite "
            "the player's (deploy it instead)")


if __name__ == "__main__":
    unittest.main()
