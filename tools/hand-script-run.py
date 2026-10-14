#!/usr/bin/env python3
"""Runs a Full VR scenario in the game under Proton, with nobody in the headset.

The Linux counterpart of tools/hand-script-run.ps1 (docs/hand-script-harness.md).
The scenario is a hand script (src/test/HandScript.h). This runner
  1. writes OBVR-test.ini beside OBVR.dll - [Debug] HandScript, HandScriptConsole=1
     and HandScriptLoad=<save>, plus the scenario's own "ini" lines - so the
     player's OBVR.ini is never touched,
  2. starts the game through Steam (steam://rungameid/22330, with the launch
     options Steam keeps for the game). OBVR loads the save from the main menu
     and hands the scenario's console lines to the game itself
     (src/test/HandScriptConsole.h): nothing is typed into the game,
  3. follows OBVR.log and takes a picture of the game's window at every mark,
  4. ends the game once the script has finished, and
  5. checks the scenario's expect, reject, count, moved, still and turned lines.
OBVR.log and OBVR.log.prev as they were before the run are put back afterwards;
the run's own log and pictures go to artifacts/hand-script/<scenario>/<time>/.

Needs: Steam and SteamVR running (the headset may sleep), Oblivion not running.

    python3 tools/hand-script-run.py tools/hand-scripts/walk-direction.txt
"""

from __future__ import annotations

import argparse
import os
import shutil
import signal
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import List, Optional, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hand_script_runner as logic  # noqa: E402

STEAM = Path.home() / ".local/share/Steam"
APP_ID = "22330"
GAME_DIR = STEAM / "steamapps/common/Oblivion"
SAVES_DIR = (STEAM / f"steamapps/compatdata/{APP_ID}/pfx/drive_c/users/steamuser/Documents/My Games/"
             "Oblivion/Saves")
STEAMVR_DIR = STEAM / "steamapps/common/SteamVR"
# The direct start, past Steam and its launch options (HANDOFF.md section 8):
# the Steam Linux Runtime's entry point with Proton inside it.
SLR_ENTRY = STEAM / "steamapps/common/SteamLinuxRuntime_4/_v2-entry-point"
PROTON = Path("/usr/share/steam/compatibilitytools.d/proton-cachyos-slr/proton")
OLD_SAVE_TIME = datetime(2000, 1, 1).timestamp()

# A window of a process of its own in front of the game, for "background-at":
# what a browser or a chat in front looks like to the game. Small, top left.
THIEF = """
import tkinter as tk
root = tk.Tk()
root.title("OBVR harness - the game is behind this window")
root.geometry("420x120+0+0")
root.attributes("-topmost", True)
tk.Label(root, text="OBVR harness: the game is behind this window").pack(expand=True)
root.after(300, lambda: (root.lift(), root.focus_force()))
root.mainloop()
"""


def say(text: str) -> None:
    print(text, flush=True)


def processes() -> List[Tuple[int, str, str]]:
    """(pid, argv[0], comm) of every process that can be read."""
    out = []
    for entry in os.scandir("/proc"):
        if not entry.name.isdigit():
            continue
        try:
            with open(f"/proc/{entry.name}/cmdline", "rb") as f:
                argv0 = f.read().split(b"\0", 1)[0].decode("utf-8", "replace")
            with open(f"/proc/{entry.name}/comm", "r", encoding="utf-8", errors="replace") as f:
                comm = f.read().strip()
        except OSError:
            continue
        out.append((int(entry.name), argv0, comm))
    return out


def full_cmdline(pid: int) -> str:
    try:
        with open(f"/proc/{pid}/cmdline", "rb") as f:
            return f.read().replace(b"\0", b" ").decode("utf-8", "replace")
    except OSError:
        return ""


def game_pid() -> Optional[int]:
    return logic.find_game_pid(processes())


def steam_launch_alive() -> bool:
    """Steam's launch of the game still running (its reaper), even after the game."""
    return any("reaper" in argv0 and f"AppId={APP_ID}" in full_cmdline(pid)
               for pid, argv0, _ in processes())


def steamvr_running() -> bool:
    return any(comm == "vrserver" or argv0.endswith("/vrserver") for _, argv0, comm in processes())


def read_log(path: Path) -> List[str]:
    try:
        return path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []


def run_quiet(args: List[str], timeout: float = 10.0) -> Tuple[int, str]:
    try:
        done = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
        return done.returncode, done.stdout.strip()
    except (OSError, subprocess.TimeoutExpired) as error:
        return -1, str(error)


def window_of(pid: int) -> Optional[str]:
    """The game's X11 window (Wine's, through XWayland), by its process."""
    code, out = run_quiet(["xdotool", "search", "--onlyvisible", "--pid", str(pid)])
    if code == 0 and out:
        return out.splitlines()[-1]
    code, out = run_quiet(["xdotool", "search", "--onlyvisible", "--name", "^Oblivion$"])
    if code == 0 and out:
        return out.splitlines()[-1]
    return None


def active_window() -> Optional[str]:
    code, out = run_quiet(["xdotool", "getactivewindow"])
    return out if code == 0 and out else None


def activate(window: str) -> bool:
    run_quiet(["xdotool", "windowactivate", "--sync", window], timeout=3.0)
    return active_window() == window


class Run:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.root = Path(__file__).resolve().parent.parent
        self.script = Path(args.script).resolve()
        self.name = self.script.stem
        artifacts = Path(args.artifact_root) if args.artifact_root else self.root / "artifacts/hand-script"
        self.run_dir = artifacts / self.name / datetime.now().strftime("%Y%m%d-%H%M%S")
        self.game_dir = Path(args.game_dir)
        self.plugins = self.game_dir / "Data/OBSE/Plugins"
        self.test_ini = self.plugins / "OBVR-test.ini"
        self.script_target = self.plugins / logic.SCRIPT_TARGET
        self.log = self.game_dir / "OBVR.log"
        self.prev = self.game_dir / "OBVR.log.prev"
        self.problems: List[str] = []
        self.measured: List[str] = []
        self.marks: List[str] = []
        self.notes: List[str] = []
        self.shot_index = 0
        self.window: Optional[str] = None
        self.background = False
        self.thief: Optional[subprocess.Popen] = None
        self.thief_window: Optional[str] = None
        self.started_at = time.time()
        self.launched_at: Optional[float] = None
        self.direct: Optional[subprocess.Popen] = None
        self.capture_said = False

    def launch_alive(self) -> bool:
        """The launch still running: our own direct start, or Steam's."""
        if self.direct is not None:
            return self.direct.poll() is None
        return steam_launch_alive()

    # --- pictures -------------------------------------------------------
    def shot(self, name: str) -> None:
        self.shot_index += 1
        pid = game_pid()
        if pid is None:
            return
        if self.window is None:
            self.window = window_of(pid)
        if self.window is None:
            if not self.capture_said:
                self.capture_said = True
                self.notes.append("no X11 window of the game found - no pictures of it")
            return
        target = self.run_dir / f"{self.shot_index:02d}-{name}-game.png"
        code, out = run_quiet(["magick", "import", "-silent", "-window", self.window,
                               "-resize", f"{self.args.shot_width}x>", str(target)], timeout=15.0)
        if code != 0 and not self.capture_said:
            self.capture_said = True
            self.notes.append(f"the game window could not be captured: {out[:200]}")

    def focus_game(self) -> None:
        if self.background or self.window is None:
            return
        if active_window() != self.window:
            activate(self.window)

    # --- the background part --------------------------------------------
    def start_thief(self, mark: str) -> None:
        self.thief = subprocess.Popen([sys.executable, "-c", THIEF], stdout=subprocess.DEVNULL,
                                      stderr=subprocess.DEVNULL)
        deadline = time.time() + 15
        while time.time() < deadline and self.thief_window is None:
            time.sleep(0.3)
            code, out = run_quiet(["xdotool", "search", "--onlyvisible", "--pid", str(self.thief.pid)])
            if code == 0 and out:
                self.thief_window = out.splitlines()[-1]
        in_front = self.thief_window is not None and activate(self.thief_window)
        self.background = True
        say(f"Background from mark {mark}: another window in front {in_front}")
        if not in_front:
            self.problems.append(f"could not put another window in front of the game at mark {mark}")

    def check_still_behind(self, mark: str) -> None:
        if self.background and self.window is not None and active_window() == self.window:
            self.problems.append(
                f"the game was in front again at mark {mark} - the background part is inconclusive")

    # --- SteamVR's dumps and OBVR's own pictures --------------------------
    def bring_dumps(self) -> None:
        """SteamVR's dumps ("dump" in the script), wherever SteamVR put them: new
        PNG files under its own folder, Steam's config and logs, or /tmp."""
        candidates = [STEAMVR_DIR, STEAM / "config", STEAM / "logs", Path("/tmp")]
        found = []
        for base in candidates:
            if not base.exists():
                continue
            depth_limited = base == Path("/tmp")
            iterator = base.glob("*.png") if depth_limited else base.rglob("*.png")
            for png in iterator:
                try:
                    if png.stat().st_mtime >= self.started_at:
                        found.append(png)
                except OSError:
                    continue
        asked = sum(1 for l in read_log(self.log) if "SteamVR was asked to dump the frame" in l)
        if asked and not found:
            self.notes.append(f"{asked} SteamVR dump(s) asked for, none written - SteamVR on Linux wrote no "
                              "dump files on 2026-10-14 either (docs/linux.md)")
        for png in sorted(found, key=lambda p: p.stat().st_mtime):
            kind = "eyes" if png.stem.endswith("-raw") else "headset"
            stamp = datetime.fromtimestamp(png.stat().st_mtime).strftime("%H%M%S")
            target = self.run_dir / f"dump-{stamp}-{kind}.png"
            command = ["magick", str(png), "-resize", f"{self.args.shot_width}x>"]
            if kind == "headset":
                command += ["-gravity", "east", "-crop", "50%x100%+0+0", "+repage", "-auto-level"]
                if self.args.dump_rotate:
                    command += ["-rotate", str(self.args.dump_rotate)]
            run_quiet(command + [str(target)], timeout=60.0)
            self.notes.append(f"SteamVR dump {png} brought over as {target.name}")
            if STEAMVR_DIR in png.parents:
                png.unlink(missing_ok=True)

    def bring_obvr_pictures(self) -> None:
        for bmp in sorted(self.game_dir.glob("OBVR-*.bmp")):
            if bmp.stat().st_mtime < self.started_at:
                continue
            run_quiet(["magick", str(bmp), str(self.run_dir / f"{bmp.stem}.png")], timeout=30.0)
            bmp.unlink(missing_ok=True)
            say(f"  picture {bmp.stem}.png")

    # --- the run ----------------------------------------------------------
    def wait_for(self, what: str, timeout: float) -> bool:
        deadline = time.time() + timeout
        said_load = False
        while time.time() < deadline:
            time.sleep(1.0)
            lines = read_log(self.log)
            warning = [l for l in lines if "HandScript: WARNING" in l]
            if warning:
                raise RuntimeError("the script was refused: " + " / ".join(warning))
            if not said_load:
                asked = [l for l in lines if "asked the main menu's console" in l]
                if asked:
                    said_load = True
                    say(asked[-1])
                    self.shot("main-menu")
            if any(what in l for l in lines):
                return True
            if what != "OBVR ready" and game_pid() is None and not self.launch_alive():
                raise RuntimeError(f"the game ended before \"{what}\"")
        return False

    def execute(self) -> None:
        spec = logic.parse_scenario(self.script.read_text(encoding="utf-8", errors="replace"))
        self.test_ini.write_text(logic.build_test_ini(spec, self.args.save), encoding="utf-8")
        shutil.copyfile(self.test_ini, self.run_dir / "OBVR-test.ini")
        shutil.copyfile(self.script, self.script_target)
        shutil.copyfile(self.script, self.run_dir / "script.txt")
        if self.args.save and self.args.save != "continue" and not (SAVES_DIR / f"{self.args.save}.ess").exists():
            raise RuntimeError(f"no save {self.args.save}.ess in {SAVES_DIR}")
        self.log.unlink(missing_ok=True)
        self.started_at = time.time()

        self.launched_at = time.time()
        if self.args.direct:
            self.start_direct()
        else:
            say("Starting the game through Steam...")
            subprocess.Popen(["steam", f"steam://rungameid/{APP_ID}"], stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL, start_new_session=True)
        if not self.wait_for("OBVR ready", self.args.start_timeout):
            raise RuntimeError(f"no 'OBVR ready' line within {self.args.start_timeout} s")
        pid = game_pid()
        if pid is not None:
            self.window = window_of(pid)
        self.focus_game()
        if not self.wait_for("HandScript: started", self.args.start_timeout):
            raise RuntimeError(f"the script did not start within {self.args.start_timeout} s (no world?)")
        say("Script started.")

        seen = 0
        finished = False
        deadline = time.time() + self.args.run_timeout
        while time.time() < deadline:
            time.sleep(0.2)
            self.focus_game()
            marks, done, seen = logic.new_marks(read_log(self.log), seen)
            for mark in marks:
                self.marks.append(mark)
                say(f"Mark {mark}")
                self.shot(mark)
                self.check_still_behind(mark)
                if mark in spec.background_at and not self.background:
                    self.start_thief(mark)
            if done:
                finished = True
                break
            if game_pid() is None:
                self.problems.append("the game ended during the script")
                break
        if not finished and not self.problems:
            self.problems.append(f"the script did not finish within {self.args.run_timeout} s")
        self.shot("end")
        self.bring_dumps()
        self.bring_obvr_pictures()
        problems, measured = logic.evaluate(spec, read_log(self.log))
        self.problems += problems
        self.measured += measured

    def start_direct(self) -> None:
        """The game through the Steam Linux Runtime and Proton, past Steam's launch
        options: the environment is the runner's own, plus --env. MoreHeap's
        version.dll is taken native as Steam's options take it."""
        env = dict(os.environ)
        env.pop("PROTON_LOG", None)
        env.update({
            "STEAM_COMPAT_DATA_PATH": str(STEAM / f"steamapps/compatdata/{APP_ID}"),
            "STEAM_COMPAT_CLIENT_INSTALL_PATH": str(STEAM),
            "SteamAppId": APP_ID,
            "SteamGameId": APP_ID,
            "WINEDLLOVERRIDES": "version=n,b",
        })
        for pair in self.args.env:
            key, _, value = pair.partition("=")
            env[key] = value
        command = [str(SLR_ENTRY), "--verb=waitforexitandrun", "--", str(PROTON), "waitforexitandrun",
                   str(self.game_dir / "OblivionLauncher.exe")]
        (self.run_dir / "launch.txt").write_text(
            "direct start\n" + " ".join(command) + "\n" + "\n".join(
                f"{k}={env[k]}" for k in sorted(env) if k.startswith(("STEAM_COMPAT", "Steam", "WINE", "DXVK",
                                                                       "PROTON", "VKD3D"))) + "\n",
            encoding="utf-8")
        say("Starting the game directly, past Steam's launch options: " + " ".join(self.args.env or ["(no extra env)"]))
        output = open(self.run_dir / "proton.log", "wb")
        # From the game folder, as Steam starts it: the OBSE loader looks for
        # Oblivion.exe beside itself in the working directory ("Couldn't find
        # Oblivion.exe", 2026-10-14), and DXVK reads dxvk.conf from there.
        self.direct = subprocess.Popen(command, env=env, cwd=str(self.game_dir), stdout=output,
                                       stderr=subprocess.STDOUT, start_new_session=True)

    def bring_measurements(self) -> None:
        """SteamVR's frame summary for the run (written when the game leaves the
        compositor) and OBVR's profiler captures, moved in and analysed."""
        time.sleep(2.0)
        totals = logic.compositor_summary(read_log(STEAM / "logs/vrcompositor.txt"),
                                          datetime.fromtimestamp(self.started_at))
        if totals is not None:
            self.measured.append(logic.compositor_line(totals))
        for capture in sorted(self.game_dir.glob("OBVR-performance-*")):
            if not capture.is_dir() or capture.stat().st_mtime < self.started_at:
                continue
            target = self.run_dir / capture.name
            shutil.move(str(capture), str(target))
            report = self.run_dir / f"{capture.name}-report"
            code, out = run_quiet([sys.executable, str(self.root / "tools/analyze-performance.py"), str(target),
                                   "--output", str(report)], timeout=180.0)
            self.notes.append(f"profiler capture {capture.name}: " +
                              (f"{report.name}/report.md" if code == 0 else f"analysis failed: {out[:200]}"))

    def close_game(self) -> None:
        # A run interrupted right after the launch: the game may still be on
        # its way up, and would run on with nobody to close it - and Steam
        # takes a few seconds to begin the launch at all (2026-10-14: the sweep
        # stopped seconds after a launch, the cleanup found neither the game
        # nor Steam's launch, and the game came up after it).
        if self.launched_at is not None:
            deadline = time.time() + 90
            while game_pid() is None and time.time() < deadline:
                if not self.launch_alive() and time.time() - self.launched_at > 30:
                    break
                time.sleep(1.0)
        pid = game_pid()
        if pid is not None:
            if self.window is not None:
                run_quiet(["xdotool", "windowquit", self.window])
            for signal_, wait in ((None, 10.0), (signal.SIGTERM, 8.0), (signal.SIGKILL, 5.0)):
                if signal_ is not None:
                    try:
                        os.kill(pid, signal_)
                    except ProcessLookupError:
                        break
                deadline = time.time() + wait
                while time.time() < deadline and game_pid() == pid:
                    time.sleep(0.5)
                if game_pid() != pid:
                    break
        # The launch ends after the game; the next run cannot start before.
        deadline = time.time() + 60
        while time.time() < deadline and self.launch_alive():
            time.sleep(1.0)

    def clean_up(self) -> None:
        if self.thief is not None and self.thief.poll() is None:
            self.thief.terminate()
        if not self.args.keep_game_open:
            self.close_game()
            self.bring_measurements()
        # The harness save stays the oldest there is, so the player's own
        # Continue never lands in it.
        for ext in ("ess", "obse"):
            path = SAVES_DIR / f"OBVRHarness.{ext}"
            if path.exists():
                os.utime(path, (OLD_SAVE_TIME, OLD_SAVE_TIME))
        self.test_ini.unlink(missing_ok=True)
        self.script_target.unlink(missing_ok=True)
        if not self.args.keep_game_open:
            if self.log.exists():
                shutil.copyfile(self.log, self.run_dir / "OBVR.log")
            saved_log = self.run_dir / "player-OBVR.log"
            saved_prev = self.run_dir / "player-OBVR.log.prev"
            if saved_log.exists():
                shutil.copyfile(saved_log, self.log)
            if saved_prev.exists():
                shutil.copyfile(saved_prev, self.prev)

    def main(self) -> int:
        if game_pid() is not None:
            raise SystemExit("Oblivion is already running; the runner starts its own session.")
        if steam_launch_alive():
            raise SystemExit("Steam still runs a launch of Oblivion; wait for it to end.")
        if not steamvr_running():
            raise SystemExit("SteamVR is not running; start it first (the headset may sleep).")
        self.run_dir.mkdir(parents=True, exist_ok=True)
        for source, name in ((self.log, "player-OBVR.log"), (self.prev, "player-OBVR.log.prev")):
            if source.exists():
                shutil.copyfile(source, self.run_dir / name)
        verdict = "FAIL"
        try:
            self.execute()
            if not self.problems:
                verdict = "PASS"
        except KeyboardInterrupt:
            self.problems.append("runner: interrupted")
        except Exception as error:  # noqa: BLE001 - every failure belongs in the summary
            self.problems.append(f"runner: {error}")
        finally:
            self.clean_up()
        lines = logic.summary(self.name, verdict, str(self.run_dir), self.marks, self.measured, self.problems)
        if self.notes:
            lines += ["", "Notes:"] + [f"- {n}" for n in self.notes]
        (self.run_dir / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
        say("\n".join(lines))
        return 0 if verdict == "PASS" else 1


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("script", help="the scenario, e.g. tools/hand-scripts/walk-direction.txt")
    p.add_argument("--game-dir", default=str(GAME_DIR))
    p.add_argument("--artifact-root", default="")
    p.add_argument("--start-timeout", type=float, default=240.0)
    p.add_argument("--run-timeout", type=float, default=300.0)
    p.add_argument("--shot-width", type=int, default=1600)
    # The composited dump is seen from where the real headset lies; on the
    # desk on 2026-09-27 it lay rolled a quarter turn.
    p.add_argument("--dump-rotate", type=int, choices=(0, 90, 180, 270), default=270)
    # The save the run starts from, loaded by OBVR from the main menu;
    # "continue" or empty: none named (the game stays in the main menu).
    p.add_argument("--save", default="OBVRHarness")
    p.add_argument("--keep-game-open", action="store_true")
    # Past Steam and its launch options, with this environment on top
    # (repeatable KEY=VALUE, e.g. DXVK_HUD=fps,frametimes,gpuload,compiler).
    p.add_argument("--direct", action="store_true")
    p.add_argument("--env", action="append", default=[], metavar="KEY=VALUE")
    args = p.parse_args(argv)
    problem = logic.env_problem(args.direct, args.env)
    if problem is not None:
        p.error(problem)
    return args


if __name__ == "__main__":
    def _interrupt(_signum, _frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, _interrupt)
    sys.exit(Run(parse_args()).main())
