"""Checks the pure part of the Linux hand script runner (tools/hand_script_runner.py):
reading a scenario's lines for the runner, the OBVR-test.ini it writes, finding the
game among the processes, following the marks in the log, and every flow of the
verdict - expect, reject, count, and the moved, still, turned and gazed checks; the
headless start (gamescope's command, its refusals, its screenshots); and which saves
are the harness's own."""

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
ini-at grabbed Render GrassDistance=8000
ini-at grabbed Hands Enabled=0
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
        self.assertEqual(s.ini_at, {"grabbed": [("Render", "GrassDistance=8000"), ("Hands", "Enabled=0")]})
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

    def test_an_ini_at_line_needs_a_mark_a_section_and_a_setting(self):
        for line in ("ini-at far Render", "ini-at far Render GrassDistance", "ini-at Render GrassDistance=1",
                     "ini-at far Render Grass Distance=1"):
            self.assertEqual(r.parse_scenario(line + "\n").ini_at, {}, line)


class TestIni(unittest.TestCase):
    def test_debug_section_carries_the_script_and_the_inside_work(self):
        text = r.build_test_ini(r.parse_scenario(SCENARIO), "OBVRHarness", "run-77")
        lines = text.splitlines()
        self.assertTrue(lines[0].startswith("; Written by tools/hand-script-run.py"))
        debug = lines.index("[Debug]")
        hands = lines.index("[Hands]")
        self.assertEqual(
            lines[debug + 1:debug + 6],
            ["HandScript=OBVR-HandScript.txt", "HandScriptConsole=1", "HandScriptLoad=OBVRHarness",
             "HarnessToken=run-77", "HandScriptTrace=1"],
        )
        self.assertEqual(lines[hands + 1], "Enabled=1")

    def test_the_start_guide_is_off_unless_the_scenario_turns_it_on(self):
        off = r.build_test_ini(r.parse_scenario("wait 1\n"), "OBVRHarness", "run-77").splitlines()
        self.assertEqual(off[off.index("[Onboarding]") + 1], "StartGuide=0")
        on = r.build_test_ini(r.parse_scenario("ini Onboarding StartGuide=1\nwait 1\n"), "OBVRHarness", "run-77")
        self.assertIn("StartGuide=1", on)
        self.assertNotIn("StartGuide=0", on)

    def test_the_self_check_panel_is_off_unless_the_scenario_turns_it_on(self):
        off = r.build_test_ini(r.parse_scenario("wait 1\n"), "OBVRHarness", "run-77").splitlines()
        self.assertEqual(off[off.index("[SelfCheck]") + 1], "ShowPanel=0")
        on = r.build_test_ini(r.parse_scenario("ini SelfCheck ShowPanel=1\nwait 1\n"), "OBVRHarness", "run-77")
        self.assertIn("ShowPanel=1", on)
        self.assertNotIn("ShowPanel=0", on)

    def test_the_holster_places_are_the_shipped_ones_unless_the_scenario_sets_them(self):
        # The player's own places (the live OBVR.ini) are not the scripts':
        # draw-leg-flicker reached for the hip the shipped places put by the
        # script's hands, and the live left hip 0.22 m behind the eyes left it
        # 24 cm off once the places stood upright over the neck (e11c200).
        off = r.build_test_ini(r.parse_scenario("ini Hands Enabled=1\nwait 1\n"), "OBVRHarness", "run-77")
        lines = off.splitlines()
        hands = lines[lines.index("[Hands]") + 1:]
        for key, value in r.SHIPPED_HOLSTER_PLACES:
            self.assertIn(f"{key}={value}", hands)
        self.assertIn("Enabled=1", hands)
        own = r.build_test_ini(r.parse_scenario("ini Hands HolsterOneHandUp=-0.5\nwait 1\n"), "OBVRHarness",
                               "run-77")
        self.assertIn("HolsterOneHandUp=-0.5", own)
        self.assertNotIn("HolsterOneHandUp=-0.62", own)
        self.assertNotIn("HolsterBowX=-0.18", own)

    def test_a_setting_changed_in_the_written_ini(self):
        text = "; one run\n[Debug]\nHarnessToken=run-77\n[Render]\nGrassDistance=2000\nGrassDensity=200\n"
        self.assertEqual(
            r.apply_ini_change(text, "Render", "GrassDistance=8000"),
            "; one run\n[Debug]\nHarnessToken=run-77\n[Render]\nGrassDistance=8000\nGrassDensity=200\n",
            "the key's line in its section replaced")
        self.assertEqual(
            r.apply_ini_change(text, "Render", "GrassAllAround=0"),
            "; one run\n[Debug]\nHarnessToken=run-77\n[Render]\nGrassDistance=2000\nGrassDensity=200\n"
            "GrassAllAround=0\n",
            "a key the section does not have yet: added at its end")
        self.assertEqual(
            r.apply_ini_change(text, "Debug", "EyeDump=1"),
            "; one run\n[Debug]\nHarnessToken=run-77\nEyeDump=1\n[Render]\nGrassDistance=2000\nGrassDensity=200\n",
            "added at the end of a section that is not the last")
        self.assertEqual(
            r.apply_ini_change(text, "Hands", "Enabled=0"), text + "[Hands]\nEnabled=0\n",
            "a section the file does not have yet: added with the key")
        self.assertEqual(
            r.apply_ini_change("[Render]\nGrassDistance=1\n[Hands]\nGrassDistance=2\n", "Hands", "GrassDistance=3"),
            "[Render]\nGrassDistance=1\n[Hands]\nGrassDistance=3\n",
            "the same key in another section is left alone")
        self.assertEqual(
            r.apply_ini_change("[render]\ngrassdistance=1\n", "Render", "GrassDistance=3"),
            "[render]\nGrassDistance=3\n", "section and key found without case, as the INI is read")

    def test_continue_or_no_save_loads_nothing(self):
        for save in ("", "continue"):
            text = r.build_test_ini(r.parse_scenario("wait 1\n"), save, "run-77")
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

    def test_gazed_holds_the_eyes(self):
        def eyes(points):
            lines = []
            for mark, heading in points.items():
                lines.append(f"HandScript: mark {mark} (t=1.00 s, line 3)")
                lines.append(f"Camera: at the mark - the eyes at 1.0 2.0 3.0 look along 0.000 1.000 0.000 "
                             f"(pitch 0.0 degrees, positive up, heading {heading}); the player's rotX 0.0 rotZ 0.0")
                lines.append("Camera: at the mark - the eyes at 0 0 0 look along 0 1 0 (pitch 0.0 degrees, "
                             "positive up, heading 77.0); the player's rotX 0.0 rotZ 0.0")
            return lines
        log = eyes({"a": 178.0, "b": -179.0})
        problems, measured = r.evaluate(r.parse_scenario("gazed a b 3\n"), log)
        self.assertEqual(problems, [])
        self.assertEqual(measured, ["the eyes from a to b: 3.0 degrees (at most 3.0)"])
        problems, _ = r.evaluate(r.parse_scenario("gazed a b 2.5\n"), log)
        self.assertEqual(problems, ["gazed a b: the eyes turned 3.0 degrees, wanted at most 2.5"])
        problems, _ = r.evaluate(r.parse_scenario("gazed a c 1\n"), log)
        self.assertEqual(problems, ["gazed a c: no eyes line at mark c"])

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


class RunToken(unittest.TestCase):
    """The run's token: the runner's game carries it in OBVR_HARNESS and the
    overlay names it, and OBVR reads the overlay only when both agree - a game
    started through Steam never does (core/HarnessToken.h)."""

    def test_the_token_is_the_stamp_and_the_pid(self):
        self.assertEqual(r.run_token("20261023-193000", 4242), "20261023-193000-4242")

    def test_the_runs_folder_is_its_start_and_for_a_parallel_run_its_slot(self):
        # Two parallel runs of one scenario started in the same second wrote into one folder (2026-11-17).
        self.assertEqual(r.run_folder("20261117-104038", None), "20261117-104038")
        self.assertEqual(r.run_folder("20261117-104038", 3), "20261117-104038-slot3")

    def test_the_overlay_names_the_token_even_without_a_save(self):
        text = r.build_test_ini(r.parse_scenario("wait 1\n"), "continue", "20261023-193000-4242")
        self.assertIn("HarnessToken=20261023-193000-4242", text.splitlines())

    def test_a_start_through_steam_cannot_carry_it(self):
        self.assertIsNone(r.token_problem(True))
        self.assertEqual(
            r.token_problem(False),
            "the test overlay needs the run's token in the game's environment (OBVR_HARNESS), "
            "which only a --direct or --headless start carries; a start through Steam cannot",
        )


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


class HarnessSaves(unittest.TestCase):
    def test_every_harness_save_and_its_co_save(self):
        names = ["OBVRHarness.ess", "OBVRHarness.obse", "OBVRHarnessMOO.ess", "OBVRHarnessMOO.obse",
                 "OBVRHarnessTall.ess", "OBVRHarness.bak", "OBVRHarness.obse.bak", "Save 12 - Nadi.ess",
                 "quicksave.ess", "obvrharness.ess"]
        self.assertEqual(r.harness_saves(names),
                         ["OBVRHarness.ess", "OBVRHarness.obse", "OBVRHarnessMOO.ess", "OBVRHarnessMOO.obse",
                          "OBVRHarnessTall.ess"])

    def test_none(self):
        self.assertEqual(r.harness_saves([]), [])
        self.assertEqual(r.harness_saves(["Save 1.ess", "autosave.obse"]), [])


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


class Ending(unittest.TestCase):
    """The game's own ending (an "ends" scenario: SteamVR's quit, vr/VrEvents.h)."""

    def test_ends_is_read(self):
        self.assertTrue(r.parse_scenario("ends\nwait 1\n").ends)
        self.assertFalse(r.parse_scenario("wait 1\n").ends)
        self.assertFalse(r.parse_scenario("ends now\n").ends)

    def test_every_flow(self):
        self.assertIsNone(r.ending_problem(ends=True, game_ended=True))
        self.assertEqual(r.ending_problem(ends=True, game_ended=False), "the game did not end by itself")
        self.assertEqual(r.ending_problem(ends=False, game_ended=True), "the game ended during the script")
        self.assertIsNone(r.ending_problem(ends=False, game_ended=False))


class StandInRuntime(unittest.TestCase):
    """A run without SteamVR (--no-steamvr): OBVR is pointed at the stand-in
    runtime, tools/mock-openvr's openvr_mock.dll (vr/OpenVRLibrary.h)."""

    LOADED = ("OpenVR: [Debug] OpenVRLibrary=openvr_mock.dll loaded in place of openvr_api.dll - "
              "a stand-in runtime, no SteamVR and no headset behind it")

    def test_the_overlay_names_the_stand_in_only_when_asked(self):
        scenario = r.parse_scenario("wait 1\n")
        plain = r.build_test_ini(scenario, "OBVRHarness", "run-77").splitlines()
        self.assertFalse(any(line.startswith("OpenVRLibrary=") for line in plain))
        stand_in = r.build_test_ini(scenario, "OBVRHarness", "run-77", stand_in=True).splitlines()
        debug = stand_in.index("[Debug]")
        self.assertEqual(
            stand_in[debug + 1:debug + 6],
            ["HandScript=OBVR-HandScript.txt", "HandScriptConsole=1", "HandScriptLoad=OBVRHarness",
             "HarnessToken=run-77", "OpenVRLibrary=openvr_mock.dll"])

    def test_steamvr_must_run_unless_the_run_goes_without_it(self):
        self.assertIsNone(r.steamvr_problem(no_steamvr=False, steamvr_runs=True))
        self.assertEqual(
            r.steamvr_problem(no_steamvr=False, steamvr_runs=False),
            "SteamVR is not running; start it first (the headset may sleep), or run with --no-steamvr "
            "(the stand-in runtime, docs/hand-script-harness.md)")
        self.assertIsNone(r.steamvr_problem(no_steamvr=True, steamvr_runs=False))
        # SteamVR running for someone else is left alone: the stand-in never talks to it.
        self.assertIsNone(r.steamvr_problem(no_steamvr=True, steamvr_runs=True))

    def test_the_stand_in_must_be_built(self):
        self.assertIsNone(r.stand_in_problem(no_steamvr=False, built=False, path="/b/openvr_mock.dll"))
        self.assertIsNone(r.stand_in_problem(no_steamvr=True, built=True, path="/b/openvr_mock.dll"))
        self.assertEqual(
            r.stand_in_problem(no_steamvr=True, built=False, path="/b/openvr_mock.dll"),
            "--no-steamvr needs the stand-in runtime, and /b/openvr_mock.dll is not there; "
            "tools/build-linux.sh builds it")

    def test_the_log_must_say_the_stand_in_was_loaded(self):
        ok = ["OBVR 0.3.0", self.LOADED, "OBVR ready"]
        self.assertIsNone(r.stand_in_loaded_problem(True, ok))
        self.assertIsNone(r.stand_in_loaded_problem(False, ["OBVR ready"]))
        self.assertEqual(
            r.stand_in_loaded_problem(True, ["OBVR ready"]),
            "--no-steamvr, but OBVR did not load the stand-in runtime (no \"OpenVRLibrary=openvr_mock.dll "
            "loaded\" line): an OBVR.dll from before [Debug] OpenVRLibrary?")
        missing = ["OpenVR: [Debug] OpenVRLibrary=openvr_mock.dll not found beside OBVR.dll - camera stays "
                   "unchanged (openvr_api.dll is not tried in its place)"]
        self.assertIsNotNone(r.stand_in_loaded_problem(True, missing))
        # A run with SteamVR that loaded the stand-in all the same did not test what it claims.
        self.assertEqual(
            r.stand_in_loaded_problem(False, ok),
            "OBVR loaded the stand-in runtime in a run that was to use SteamVR")

    def test_the_note_says_what_the_run_did_not_have(self):
        self.assertIn("openvr_mock.dll", r.STAND_IN_NOTE)
        self.assertIn("no SteamVR", r.STAND_IN_NOTE)


class StandInHeadset(unittest.TestCase):
    """Which headset the stand-in runtime stands in for (--headset, --hz, --eye,
    --ipd; tools/mock-openvr/MockRuntime.h): the Dream Air unless told otherwise."""

    def test_nothing_asked_is_fine_with_or_without_steamvr(self):
        self.assertIsNone(r.headset_problem(False, None, None, None, None))
        self.assertIsNone(r.headset_problem(True, None, None, None, None))

    def test_a_headset_is_chosen_only_for_the_stand_in(self):
        for args in (("beyond", None, None, None), (None, 90.0, None, None), (None, None, "3840x3552", None),
                     (None, None, None, 63.0)):
            self.assertEqual(
                r.headset_problem(False, *args),
                "--headset, --hz, --eye and --ipd describe the stand-in runtime's headset; they need --no-steamvr "
                "(with SteamVR the headset is the real one)", args)

    def test_every_value_as_it_may_be(self):
        self.assertIsNone(r.headset_problem(True, "dreamair", 90.0, "3840x3552", 63.5))
        self.assertIsNone(r.headset_problem(True, "beyond", 30.0, "256x16384", 50.0))
        self.assertIsNone(r.headset_problem(True, None, 240.0, None, 80.0))

    def test_values_that_are_refused(self):
        self.assertEqual(r.headset_problem(True, "index", None, None, None),
                         "--headset index: the stand-in knows beyond, dreamair")
        for hz in (29.9, 240.5, -72.0):
            self.assertEqual(r.headset_problem(True, None, hz, None, None),
                             f"--hz {hz:g}: a display rate from 30 to 240", hz)
        for eye in ("3840", "3840x", "x3552", "3840 x 3552", "255x3552", "3840x16385", "3840x35.5", "axb"):
            self.assertEqual(r.headset_problem(True, None, None, eye, None),
                             f"--eye {eye}: an eye's size as <width>x<height>, each side from 256 to 16384", eye)
        for ipd in (49.9, 80.1):
            self.assertEqual(r.headset_problem(True, None, None, None, ipd),
                             f"--ipd {ipd:g}: the eyes' distance in millimetres, from 50 to 80", ipd)

    def test_the_games_environment_names_the_headset(self):
        self.assertEqual(r.headset_env(None, None, None, None), {"OBVR_MOCK_HEADSET": "dreamair"})
        self.assertEqual(r.headset_env("beyond", None, None, None), {"OBVR_MOCK_HEADSET": "beyond"})
        self.assertEqual(
            r.headset_env("dreamair", 90.0, "3840X3552", 63.5),
            {"OBVR_MOCK_HEADSET": "dreamair", "OBVR_MOCK_HZ": "90", "OBVR_MOCK_EYE": "3840x3552",
             "OBVR_MOCK_IPD": "63.5"})

    def test_what_the_headset_is_expected_to_answer(self):
        self.assertEqual(r.expected_headset(None, None, None, None), (4036, 3376, 72.0, 63.0))
        self.assertEqual(r.expected_headset("dreamair", 90.0, None, None), (4036, 3376, 90.0, 63.0))
        self.assertEqual(r.expected_headset("beyond", None, None, None), (2568, 2568, 75.0, 62.0))
        self.assertEqual(r.expected_headset("beyond", 90.0, "3840x3552", 65.0), (3840, 3552, 90.0, 65.0))

    LOG = ["OpenVR: the headset's display runs at 72.00 Hz, a period of 13.89 ms",
           "Render: IPD 63.0 mm",
           "Render: two 4036x3376 eye textures ready, cross at u=0.536 and u=0.464"]

    def test_the_log_agrees_with_the_headset_asked_for(self):
        self.assertIsNone(r.headset_log_problem((4036, 3376, 72.0, 63.0), self.LOG))

    def test_the_log_names_another_headset(self):
        self.assertEqual(
            r.headset_log_problem((2568, 2568, 75.0, 62.0), self.LOG),
            "the stand-in's headset is not the one asked for - missing in the log: \"two 2568x2568 eye textures "
            "ready\", \"display runs at 75.00 Hz\", \"IPD 62.0 mm\"")
        self.assertEqual(
            r.headset_log_problem((4036, 3376, 90.0, 63.0), self.LOG),
            "the stand-in's headset is not the one asked for - missing in the log: \"display runs at 90.00 Hz\"")

    def test_a_log_without_the_lines(self):
        self.assertIsNotNone(r.headset_log_problem((4036, 3376, 72.0, 63.0), ["OBVR ready"]))

    def test_the_note_names_the_headset(self):
        self.assertEqual(r.headset_note(None, (4036, 3376, 72.0, 63.0)),
                         "stand-in headset: dreamair - 4036x3376 an eye, 72 Hz, IPD 63 mm")
        self.assertEqual(r.headset_note("beyond", (2568, 2568, 75.0, 62.5)),
                         "stand-in headset: beyond - 2568x2568 an eye, 75 Hz, IPD 62.5 mm")


class ExclusiveRuns(unittest.TestCase):
    """A run that measures has the machine to itself (Nadi, 2026-11-17): no other
    harness run and no other game beside it. Asked for with --exclusive, and taken
    by itself by a scenario that turns the profiler on."""

    def test_a_scenario_that_turns_the_profiler_on_measures(self):
        self.assertTrue(r.measures(r.parse_scenario("ini Performance Enabled=1\nwait 1\n")))
        self.assertTrue(r.measures(r.parse_scenario("ini-at ready Performance Enabled=1\nmark ready\n")))

    def test_one_that_does_not_does_not(self):
        self.assertFalse(r.measures(r.parse_scenario("wait 1\n")))
        self.assertFalse(r.measures(r.parse_scenario("ini Performance Enabled=0\nwait 1\n")))
        self.assertFalse(r.measures(r.parse_scenario("ini Performance CaptureSeconds=30\nwait 1\n")))
        self.assertFalse(r.measures(r.parse_scenario("ini Hands Enabled=1\nwait 1\n")))
        self.assertFalse(r.measures(r.parse_scenario("ini-at ready Performance Enabled=0\nmark ready\n")))

    def test_why_a_run_is_exclusive(self):
        measuring = r.parse_scenario("ini Performance Enabled=1\n")
        plain = r.parse_scenario("wait 1\n")
        self.assertIsNone(r.exclusive_reason(False, plain))
        self.assertEqual(r.exclusive_reason(True, plain), "asked for with --exclusive")
        self.assertEqual(r.exclusive_reason(False, measuring),
                         "the scenario turns the profiler on ([Performance] Enabled=1)")
        self.assertEqual(r.exclusive_reason(True, measuring), "asked for with --exclusive")

    def test_what_a_run_waits_for(self):
        self.assertEqual(r.waiting_for(exclusive=False, gate_free=False, lock_free=False, other_games=0),
                         "an exclusive run is under way or waiting")
        self.assertEqual(r.waiting_for(exclusive=False, gate_free=True, lock_free=False, other_games=0),
                         "an exclusive run is under way")
        self.assertIsNone(r.waiting_for(exclusive=False, gate_free=True, lock_free=True, other_games=3))
        self.assertEqual(r.waiting_for(exclusive=True, gate_free=False, lock_free=False, other_games=0),
                         "another exclusive run is under way or waiting")
        self.assertEqual(r.waiting_for(exclusive=True, gate_free=True, lock_free=False, other_games=2),
                         "other harness runs are under way")
        self.assertEqual(r.waiting_for(exclusive=True, gate_free=True, lock_free=True, other_games=2),
                         "2 games are running (the player's, or a run started some other way)")
        self.assertEqual(r.waiting_for(exclusive=True, gate_free=True, lock_free=True, other_games=1),
                         "1 game is running (the player's, or a run started some other way)")
        self.assertIsNone(r.waiting_for(exclusive=True, gate_free=True, lock_free=True, other_games=0))

    def test_a_wait_that_ran_out(self):
        self.assertEqual(
            r.wait_ran_out(True, "other harness runs are under way", 1800.0),
            "this exclusive run did not get the machine to itself within 1800 s: other harness runs are under way")
        self.assertEqual(
            r.wait_ran_out(False, "an exclusive run is under way", 60.0),
            "this run could not start within 60 s: an exclusive run is under way")

    def test_the_note(self):
        self.assertEqual(
            r.exclusive_note("asked for with --exclusive", 0.0, 0),
            "exclusive (asked for with --exclusive): it started with no other harness run and no other game "
            "running, and held the harness's other runs off until its end")
        self.assertEqual(
            r.exclusive_note("the scenario turns the profiler on ([Performance] Enabled=1)", 125.4, 2),
            "exclusive (the scenario turns the profiler on ([Performance] Enabled=1)): it started with no other "
            "harness run and no other game running, and held the harness's other runs off until its end; it waited "
            "125 s for that. 2 other gamescope sessions were running at its start (not this harness's - their load "
            "is in the numbers)")
        self.assertEqual(
            r.exclusive_note("asked for with --exclusive", 0.4, 1),
            "exclusive (asked for with --exclusive): it started with no other harness run and no other game "
            "running, and held the harness's other runs off until its end. 1 other gamescope session was running "
            "at its start (not this harness's - its load is in the numbers)")

    def test_a_game_started_beside_an_exclusive_run(self):
        # A game the runner cannot hold off: the player's own start, or a runner from before the locks.
        self.assertIsNone(r.intruder_problem(0, "walking"))
        self.assertEqual(
            r.intruder_problem(1, "walking"),
            "this exclusive run was not alone: 1 other game was running beside it after the mark walking "
            "(not started through this runner's locks) - its numbers carry that load")
        self.assertEqual(
            r.intruder_problem(2, None),
            "this exclusive run was not alone: 2 other games were running beside it before its first mark "
            "(not started through this runner's locks) - its numbers carry that load")



class Recording(unittest.TestCase):
    """The recording mode's part of the runner: the scenario switched on and
    dressed, the films found among the files, the strip's picks, the rate and
    the commands that make the film."""

    def test_switched_on_after_the_comments(self):
        text = "# head\n\nwait 1\nmark a\n"
        out = r.with_recording(text, 4, 2, 640, False).splitlines()
        self.assertEqual(out[:2], ["# head", ""])
        self.assertEqual(out[2:6], ["ini Debug Record=1", "ini Debug RecordSeconds=4",
                                    "ini Debug RecordEvery=2", "ini Debug RecordWidth=640"])
        self.assertEqual(out[6], "wait 1")
        s = r.parse_scenario("\n".join(out))
        self.assertIn("Record=1", s.ini["Debug"])

    def test_dressed(self):
        out = r.with_recording("wait 1\n", 0, 1, 640, True).splitlines()
        self.assertEqual(out[:2], ["console player.additem 000651D2 1", "console player.equipitem 000651D2"])
        self.assertNotIn("ini Debug Record=1", out)

    def test_nothing_asked_leaves_it(self):
        self.assertEqual(r.with_recording("wait 1\n", 0, 1, 640, False), "wait 1\n")

    def test_every_zero_is_every_frame(self):
        self.assertIn("ini Debug RecordEvery=1", r.with_recording("wait 1", 2, 0, 320, False).splitlines())

    def test_only_comments(self):
        out = r.with_recording("# a\n", 1, 1, 320, False).splitlines()
        self.assertEqual(out[0], "# a")
        self.assertEqual(out[1], "ini Debug Record=1")

    def test_films_found_and_ordered(self):
        names = ["OBVR-Rec-pull-0002.bmp", "OBVR-Rec-pull-0000.bmp", "OBVR-Rec-up-0000.bmp",
                 "OBVR-Rec-pull-0001.bmp", "OBVR-Eye-Right-pull.bmp", "OBVR-Rec-x.bmp",
                 "OBVR-Rec-a-b-0003.bmp"]
        films = r.recording_films(names)
        self.assertEqual(films["pull"], ["OBVR-Rec-pull-0000.bmp", "OBVR-Rec-pull-0001.bmp",
                                         "OBVR-Rec-pull-0002.bmp"])
        self.assertEqual(films["up"], ["OBVR-Rec-up-0000.bmp"])
        self.assertEqual(films["a-b"], ["OBVR-Rec-a-b-0003.bmp"])
        self.assertEqual(len(films), 3)

    def test_strip_picks(self):
        self.assertEqual(r.strip_picks(0), [])
        self.assertEqual(r.strip_picks(3), [0, 1, 2])
        picks = r.strip_picks(100, 10)
        self.assertEqual(len(picks), 10)
        self.assertEqual((picks[0], picks[-1]), (0, 99))

    def test_rate(self):
        self.assertEqual(r.film_fps(72, 2), 36)
        self.assertEqual(r.film_fps(0, 1), 72)
        self.assertEqual(r.film_fps(90, 0), 90)
        self.assertEqual(r.film_fps(2, 10), 1)

    def test_commands(self):
        cmds = r.film_commands("pull", "/g", "/run", 36, ["OBVR-Rec-pull-0000.bmp"])
        self.assertEqual(len(cmds), 3)
        self.assertIn("/g/OBVR-Rec-pull-%04d.bmp", cmds[0])
        self.assertEqual(cmds[0][-1], "/run/rec-pull.mp4")
        self.assertEqual(cmds[1][-1], "/run/rec-pull.gif")
        self.assertEqual(cmds[2][1], "/g/OBVR-Rec-pull-0000.bmp")
        self.assertEqual(cmds[2][-1], "/run/rec-pull-strip.png")
        self.assertEqual(len(r.film_commands("pull", "/g", "/run", 36, [])), 2)

if __name__ == "__main__":
    unittest.main()
