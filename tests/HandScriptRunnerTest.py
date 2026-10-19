"""Checks the pure part of the Linux hand script runner (tools/hand_script_runner.py):
reading a scenario's lines for the runner, the OBVR-test.ini it writes, finding the
game among the processes, following the marks in the log, and every flow of the
verdict - expect, reject, count, and the moved, still and turned checks; and the
headless start (gamescope's command, its refusals, its screenshots)."""

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import hand_script_runner as r  # noqa: E402


SCENARIO = """# a scenario
wait 9
console player.additem 00000C0D 1   # a comment after a line
console-at grabbed {near}.disable
console-at grabbed tgm
ini Hands Enabled=1
ini Debug HandScriptTrace=1
expect HandScript: finished
reject Exception
count 2 HandScript: mark
background-at walking
moved start walking 50
still walking stopped 5.5
turned start walking 30
mark start
right trigger 1
"""


class ParseScenario(unittest.TestCase):
    def test_every_runner_line(self):
        s = r.parse_scenario(SCENARIO)
        self.assertEqual(s.console, ["player.additem 00000C0D 1"])
        self.assertEqual(s.console_at, {"grabbed": ["{near}.disable", "tgm"]})
        self.assertEqual(s.ini, {"Hands": ["Enabled=1"], "Debug": ["HandScriptTrace=1"]})
        self.assertEqual(s.expects, ["HandScript: finished"])
        self.assertEqual(s.rejects, ["Exception"])
        self.assertEqual(s.counts, [(2, "HandScript: mark")])
        self.assertEqual(s.background_at, ["walking"])
        self.assertEqual(
            s.walk_checks,
            [("moved", "start", "walking", 50.0), ("still", "walking", "stopped", 5.5),
             ("turned", "start", "walking", 30.0)],
        )

    def test_the_scripts_own_lines_are_not_the_runners(self):
        s = r.parse_scenario("wait 1\nmark a\nright pos 0 0 0\n# expect nothing\n")
        self.assertEqual((s.expects, s.console, s.ini), ([], [], {}))


class TestIni(unittest.TestCase):
    def test_debug_section_carries_the_script_and_the_inside_work(self):
        text = r.build_test_ini(r.parse_scenario(SCENARIO), "OBVRHarness")
        lines = text.splitlines()
        self.assertTrue(lines[0].startswith("; Written by tools/hand-script-run.py"))
        debug = lines.index("[Debug]")
        hands = lines.index("[Hands]")
        self.assertEqual(
            lines[debug + 1:debug + 5],
            ["HandScript=OBVR-HandScript.txt", "HandScriptConsole=1", "HandScriptLoad=OBVRHarness",
             "HandScriptTrace=1"],
        )
        self.assertEqual(lines[hands + 1], "Enabled=1")

    def test_the_start_guide_is_off_unless_the_scenario_turns_it_on(self):
        off = r.build_test_ini(r.parse_scenario("wait 1\n"), "OBVRHarness").splitlines()
        self.assertEqual(off[off.index("[Onboarding]") + 1], "StartGuide=0")
        on = r.build_test_ini(r.parse_scenario("ini Onboarding StartGuide=1\nwait 1\n"), "OBVRHarness")
        self.assertIn("StartGuide=1", on)
        self.assertNotIn("StartGuide=0", on)

    def test_continue_or_no_save_loads_nothing(self):
        for save in ("", "continue"):
            text = r.build_test_ini(r.parse_scenario("wait 1\n"), save)
            self.assertNotIn("HandScriptLoad", text)
            self.assertIn("HandScriptConsole=1", text)


class FindGame(unittest.TestCase):
    def test_the_process_whose_program_is_oblivion_exe(self):
        procs = [
            (10, "/usr/bin/python3", "python3"),
            (11, "c:\\windows\\system32\\steam.exe", "steam.exe"),
            (12, "/home/x/Oblivion/OblivionLauncher.exe", "OblivionLaunch"),
            (13, "Oblivion.exe", "Main"),
        ]
        self.assertEqual(r.find_game_pid(procs), 13)

    def test_case_and_windows_paths(self):
        self.assertEqual(r.find_game_pid([(5, "S:\\steamapps\\common\\Oblivion\\OBLIVION.EXE", "Main")]), 5)

    def test_none(self):
        self.assertIsNone(r.find_game_pid([(1, "/usr/bin/steam", "steam")]))


class FollowMarks(unittest.TestCase):
    LOG = [
        "OBVR ready",
        "HandScript: started - equipped weapon form 00000C0D, Hands 1",
        "HandScript: mark start (t=0.00 s, line 14)",
        "HandScript: player at 10.0 20.0 30.0, heading 90.0 degrees",
        "HandScript: log something (t=1.00 s, line 15)",
        "HandScript: mark walking (t=2.00 s, line 16)",
        "HandScript: finished (t=3.00 s)",
    ]

    def test_new_marks_since_the_last_look(self):
        marks, finished, seen = r.new_marks(self.LOG[:4], 0)
        self.assertEqual((marks, finished), (["start"], False))
        marks, finished, seen = r.new_marks(self.LOG, seen)
        self.assertEqual((marks, finished), (["walking"], True))
        marks, finished, _ = r.new_marks(self.LOG, seen)
        self.assertEqual((marks, finished), ([], False))


def player_log(points):
    lines = []
    for mark, (x, y, z, heading) in points.items():
        lines.append(f"HandScript: mark {mark} (t=1.00 s, line 3)")
        lines.append(f"HandScript: player at {x} {y} {z}, heading {heading} degrees")
        lines.append(f"HandScript: player at 999 999 999, heading 0 degrees")
    return lines


class Verdict(unittest.TestCase):
    def test_pass(self):
        spec = r.parse_scenario("expect alpha\nreject beta\ncount 2 gamma\n")
        problems, measured = r.evaluate(spec, ["alpha here", "gamma one", "gamma two"])
        self.assertEqual((problems, measured), ([], []))

    def test_expect_missing(self):
        problems, _ = r.evaluate(r.parse_scenario("expect alpha\n"), ["beta"])
        self.assertEqual(problems, ["expected in the log, missing: alpha"])

    def test_rejected_line_present(self):
        problems, _ = r.evaluate(r.parse_scenario("reject beta\n"), ["x", "the beta line"])
        self.assertEqual(problems, ["rejected, but in the log: the beta line"])

    def test_count_wrong(self):
        problems, _ = r.evaluate(r.parse_scenario("count 2 gamma\n"), ["gamma"])
        self.assertEqual(problems, ["expected 2 times in the log, found 1: gamma"])

    def test_moved_and_still(self):
        log = player_log({"a": (0, 0, 0, 0), "b": (30, 40, 500, 0)})
        problems, measured = r.evaluate(r.parse_scenario("moved a b 50\nstill a b 60\n"), log)
        self.assertEqual(problems, [])
        self.assertEqual(measured[0], "walked from a to b: 50.0 units across the ground (at least 50.0)")
        problems, _ = r.evaluate(r.parse_scenario("moved a b 51\nstill a b 49\n"), log)
        self.assertEqual(problems, ["moved a b: 50.0 units, wanted at least 51.0",
                                    "still a b: 50.0 units, wanted at most 49.0"])

    def test_turned_across_the_wrap(self):
        log = player_log({"a": (0, 0, 0, 350), "b": (0, 0, 0, 20)})
        problems, measured = r.evaluate(r.parse_scenario("turned a b 30\n"), log)
        self.assertEqual(problems, [])
        self.assertEqual(measured, ["turned from a to b: 30.0 degrees (at least 30.0)"])
        problems, _ = r.evaluate(r.parse_scenario("turned a b 31\n"), log)
        self.assertEqual(problems, ["turned a b: 30.0 degrees, wanted at least 31.0"])

    def test_no_player_line_at_a_mark(self):
        problems, _ = r.evaluate(r.parse_scenario("moved a c 1\n"), player_log({"a": (0, 0, 0, 0)}))
        self.assertEqual(problems, ["moved a c: no player line at mark c"])


class CompositorSummary(unittest.TestCase):
    LOG = [
        "Wed Oct 14 2026 13:03:15.889940 [Info] - Total.................. 10413 presents.  361 dropped. 4932 reprojected",
        "Wed Oct 14 2026 14:12:31.123456 [Info] - Total.................. 37184 presents.   35 dropped.  631 reprojected",
        "Wed Oct 14 2026 14:12:31.123500 [Info] - Total dropped frames: 720923",
        "Wed Oct 14 2026 14:18:34.000001 [Info] - Total..................  1611 presents.  557 dropped.  629 reprojected",
    ]

    def test_the_last_summary_since_the_run_began(self):
        since = r.compositor_time("Wed Oct 14 2026 14:00:00.000000")
        self.assertEqual(r.compositor_summary(self.LOG, since), (1611, 557, 629))

    def test_none_since(self):
        since = r.compositor_time("Wed Oct 14 2026 15:00:00.000000")
        self.assertIsNone(r.compositor_summary(self.LOG, since))

    def test_the_line_as_measured(self):
        self.assertEqual(r.compositor_line((1611, 557, 629)),
                         "SteamVR: 1611 frames presented, 557 dropped, 629 reprojected (39.0 %)")


class DirectEnvironment(unittest.TestCase):
    def test_none_given(self):
        self.assertIsNone(r.env_problem(False, []))
        self.assertIsNone(r.env_problem(True, []))

    def test_only_a_direct_start_takes_them(self):
        self.assertEqual(
            r.env_problem(False, ["DXVK_HUD=fps"]),
            "--env only reaches a --direct start; a start through Steam takes Steam's launch options")

    def test_key_value_pairs(self):
        self.assertIsNone(r.env_problem(True, ["DXVK_HUD=fps", "DXVK_CONFIG=d3d9.presentInterval = 0"]))
        self.assertIsNone(r.env_problem(True, ["EMPTY="]))
        self.assertEqual(r.env_problem(True, ["DXVK_HUD"]), "--env 'DXVK_HUD': not KEY=VALUE")
        self.assertEqual(r.env_problem(True, ["=fps"]), "--env '=fps': not KEY=VALUE")


class Headless(unittest.TestCase):
    """--headless: the game inside gamescope's headless backend - nothing on the
    desktop, no input grabbed, the pictures from gamescope's own screenshots."""

    def test_the_launch_wrapped_in_gamescope(self):
        self.assertEqual(
            r.headless_command(["/slr/_v2-entry-point", "--verb=waitforexitandrun", "--", "/p/proton",
                                "waitforexitandrun", "/g/OblivionLauncher.exe"], 2560, 1440),
            ["gamescope", "--backend", "headless", "-W", "2560", "-H", "1440", "-w", "2560", "-h", "1440",
             "-r", "144", "--", "/slr/_v2-entry-point", "--verb=waitforexitandrun", "--", "/p/proton",
             "waitforexitandrun", "/g/OblivionLauncher.exe"])

    def test_no_problem_without_headless_or_without_a_background_part(self):
        self.assertIsNone(r.headless_problem(False, ["walking"]))
        self.assertIsNone(r.headless_problem(True, []))

    def test_a_background_part_needs_the_desktop(self):
        self.assertEqual(
            r.headless_problem(True, ["walking"]),
            "background-at needs the desktop (a window of another process put in front of the game); "
            "run this scenario without --headless")

    def test_the_games_size_from_its_ini(self):
        self.assertEqual(r.game_size(["[Display]", "bFull Screen=1", "iSize W=2560", "iSize H=1440"]), (2560, 1440))
        self.assertEqual(r.game_size(["iSize W = 1920 ", " iSize H=1080"]), (1920, 1080))

    def test_the_default_size_when_the_ini_lacks_it(self):
        self.assertEqual(r.game_size(["[Display]", "bFull Screen=1"]), (2560, 1440))
        self.assertEqual(r.game_size(["iSize W=abc", "iSize H=1440"]), (2560, 1440))

    def test_the_newest_screenshot_since_the_request(self):
        files = [("/tmp/gamescope_a.avif", 100.0), ("/tmp/gamescope_b.avif", 130.0),
                 ("/tmp/gamescope_c.avif", 120.0)]
        self.assertEqual(r.newest_screenshot(files, 110.0), "/tmp/gamescope_b.avif")
        self.assertEqual(r.newest_screenshot(files, 130.0), "/tmp/gamescope_b.avif")

    def test_none_when_nothing_new(self):
        files = [("/tmp/gamescope_a.avif", 100.0)]
        self.assertIsNone(r.newest_screenshot(files, 110.0))
        self.assertIsNone(r.newest_screenshot([], 0.0))


class Summary(unittest.TestCase):
    def test_pass_and_fail(self):
        text = r.summary("teleport", "PASS", "/runs/x", ["a", "b"], [], [])
        self.assertEqual(text[0], "# teleport - PASS")
        self.assertIn("Marks: a, b", text)
        self.assertEqual(text[-1], "Every expect line was found, no reject line was.")
        text = r.summary("teleport", "FAIL", "/runs/x", [], ["walked 1"], ["missing x"])
        self.assertIn("Measured:", text)
        self.assertIn("- walked 1", text)
        self.assertEqual(text[-2:], ["Problems:", "- missing x"])


if __name__ == "__main__":
    unittest.main()
