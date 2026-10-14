# OBVR on Linux

What the Linux side of the development machine can do as of 2026-10-14, and what it
cannot do yet. Written in the first Linux session after the work had run on Windows
since 2026-08-25. `HANDOFF.md` sections 7, 8, 11 and 12 hold the older Linux notes;
where they and this file disagree, this file is the later measurement. Everything below
was measured on this machine on the date given unless it says otherwise.

## The machine

CachyOS (kernel 7.2.9), 16 cores, NVIDIA, dual boot with the Windows installation that
built and tested OBVR until 2026-10-14.

- The repository is the Windows checkout (`D:\Modding\OBVR`), mounted read-write with
  `ntfs3`. Both systems work on the same files; `core.filemode` is off, so the
  executable bits NTFS reports do not show up as changes. The Windows build directories
  `build/` and `build-tests/` hold MSVC caches and cannot be reused from Linux; the
  Linux builds live in `build-linux*`, which `.gitignore` already covers.
- Steam under `~/.local/share/Steam`; Oblivion (app 22330) under
  `steamapps/common/Oblivion`, its Proton prefix under `steamapps/compatdata/22330`.
  Proton builds present: `proton-cachyos-slr` (system package, under
  `/usr/share/steam/compatibilitytools.d`) and `GE-Proton11-5` (user). Steam Linux
  Runtimes present: `SteamLinuxRuntime`, `_4`, `_sniper`, `_soldier`.
- Toolchain: clang, lld and llvm 23.1.1 (`lld-link` and `llvm-dlltool` included), GCC
  16.2.1 as the default `c++`, CMake 4.4.4, Ninja 1.13.2 (installed 2026-10-14; the
  first builds of the day ran on the Makefile generator, with the same result), GNU make
  4.4.1. 32-bit userland for native 32-bit builds: `lib32-glibc`, `lib32-gcc-libs`.
- Python 3.14.8, Node 25.8.1, OpenJDK 27 as the system Java.

## Building

### The tests: clang, 32-bit

```
cmake -S tests -B build-linux-tests -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=-m32
cmake --build build-linux-tests
ctest --test-dir build-linux-tests --output-on-failure
```

Result on 2026-10-14: 125 tests, 125 passed. The other 14 of the 139 are Windows only
(below). clang reported one warning in code that runs here,
`tests/WaterVRTestPlanTest.cpp:82` (misleading indentation), and two in
`tests/VRMenuBridgeTest.cpp` (an unused variable and function), which is Windows only
now; none of the three was touched.

Two constraints, both enforced by `tests/CMakeLists.txt` since 2026-10-14 with a message
that names the command to use instead:

- **32-bit.** The tests check the layouts of the mirrored Windows, Direct3D and OpenVR
  structures (`D3DPRESENT_PARAMETERS is thirteen 32-bit fields and one pointer`,
  `VRControllerState_t is 64 bytes` and so on). Those hold for the pointer size the DLL
  is built for, which is why CI builds the tests `-A Win32`. A 64-bit build used to fail
  in the middle of compiling; now it stops at configure time.
- **clang, not GCC.** The mirrored interfaces are declared with the `__stdcall`,
  `__cdecl`, `__thiscall` and `__fastcall` keywords. GCC knows these conventions only in
  their `__attribute__` spelling (1,700 `expected ')' before '*' token` errors); clang
  knows the keywords on every x86 target and, with `-m32`, implements them, so a
  function pointer type means the same thing it means on Windows.

Two things had to change for the 32-bit Linux build to pass. Both were Windows facts
left to the host ABI:

- `VRControllerState`, `DigitalData` and `AnalogData` hold 64-bit members. Windows
  aligns them to eight bytes; the i386 System V ABI aligns them to four, which shrank
  the structs to 60, 20 and 40 bytes instead of 64, 24 and 48 and failed the size
  checks. The members now carry `alignas(8)` (`src/vr/OpenVRTypes.h`,
  `src/vr/ControllerActions.h`), which pins the Windows layout on any host and changes
  nothing where eight is already the natural alignment, as it is for MSVC on x86. The
  Windows CI run is the check for that last claim.
- Five tests compiled sources that go through Win32: `Log.cpp` and `PluginPath.cpp`
  write and look up files, `LayoutProbe.cpp` writes its dump with `CreateFileA`,
  `VRMenuBridge.cpp` reads `GetAsyncKeyState`, `Memory.cpp` calls `VirtualProtect`,
  and `QuickKeyPages.h` includes the OBSE plugin interface, which is declared on
  `platform/Win32Min.h`. They sit behind `if(WIN32)` now, like the eight before them:
  `game_device`, `menu_type`, `layout_probe`, `quick_key_pages`, `vr_menu_bridge`.

The 14 tests that run only on Windows: `config`, `crosshair_cache`,
`dialog_camera_hook`, `game_device`, `head_tracker`, `ini_settings`, `layout_probe`,
`menu_type`, `openvr_backend`, `plugin_path`, `present_hook`, `quick_key_pages`,
`ui_screen_size`, `vr_menu_bridge`. CI runs all 139 on Windows.

### The DLL: clang-cl against Microsoft's headers and libraries (since 2026-10-14)

`cmake/toolchain-linux-clang-cl.cmake` builds the real `OBVR.dll` on Linux: clang-cl and
lld-link against Microsoft's C runtime, C++ standard library and Windows SDK, which
[xwin](https://github.com/Jake-Shadle/xwin) (0.10.0, from `cargo install xwin --locked`)
fetches from Microsoft and lays out as a winsysroot under `~/tools/xwin`: the C runtime
of Visual Studio 17.14 (14.44, the toolset the Windows build links with; its `OBVR.dll`
names linker 14.44) and the Windows SDK 10.0.26100, for x86 only. The tester accepted
Microsoft's licence for them on 2026-10-14; the files are not redistributable and stay
out of the repository. Setup and build, also at the top of the toolchain file:

```
xwin --accept-license --arch x86 --cache-dir ~/tools/xwin-cache splat --use-winsysroot-style --preserve-ms-arch-notation --output ~/tools/xwin
cmake -S . -B build-linux-clangcl -G Ninja -DCMAKE_BUILD_TYPE=Release --toolchain cmake/toolchain-linux-clang-cl.cmake
cmake --build build-linux-clangcl
```

What the toolchain sets, and why:

- Nothing for the build definition itself: CMake sets `MSVC` for a compiler that takes
  cl's command line, so the MSVC branch of `CMakeLists.txt` applies as on Windows
  (`/W4 /permissive-`, the static runtime, the linker map), and `OBVR_NO_WINSDK` stays
  off, so the code takes the same branches as the Windows build.
- `-fms-compatibility-version=19.44`: without a cl.exe to ask, clang-cl announces 19.33.
- `CMAKE_TRY_COMPILE_CONFIGURATION Release`: xwin leaves Microsoft's debug runtime out,
  and CMake's compiler check failed on `msvcrtd.lib` until it built in Release.
- `/manifest:no`: CMake would otherwise run mt over the DLL. The Windows build carries
  link.exe's default manifest (trustInfo at asInvoker, no dependencies); this build
  carries none.

The first build, 2026-10-14: 153 steps in about four seconds, no error, 11 warnings -
unused variables, functions and fields and one pointer cast MSVC allows, code MSVC's
`/W4` does not flag; left as they are. Against the Windows build of the same sources:
the same exports (`OBSEPlugin_Load`, `OBSEPlugin_Query`), the same imported DLLs
(`KERNEL32`, `USER32`, `WINMM`, no runtime DLL), a TLS directory in both; 1,516,032
bytes against 1,339,392, and no `.rsrc` (the manifest). The one MSVC inline-assembly
function, `LeafCall` in `render/LeafFacingHook.cpp`, comes out as the instructions its
source spells.

Releases stay with MSVC: `.github/workflows/build.yml` builds the DLL on every push to
`main` and keeps it as an artifact. Clang optimises differently, so the DLL tested here
is not byte-identical with a release; before a release the CI artifact gets one run in
the headset.

The older `cmake/toolchain-linux-nosdk.cmake` builds without a C++ standard library and
has not built since the DLL took `<atomic>`, `<vector>` and `<string>` on board
(2026-09-23 and 2026-09-27): on 2026-10-14, 106 of 145 translation units failed on a
missing header. The route above replaces it.

## Switching between Windows and Linux

Since 2026-10-14 both systems work on the same state, with the Windows files as the
originals and Linux pointing at them through symbolic links on the mounted Windows
partition (Linux follows the links; Wine sees ordinary files):

- **The live `OBVR.ini`.** The Linux game folder's `Data/OBSE/Plugins/OBVR.ini` is a link to
  the Windows game folder's. A setting changed in the menu lands in the one file either
  way: Wine's `WritePrivateProfileString` rewrites the file in place with
  `CreateFileW(CREATE_ALWAYS)`, which follows the link and keeps it (Wine's
  `dlls/kernel32/profile.c`, `PROFILE_FlushFile`).
- **`dxvk.conf`.** The Linux game folder's is a link to the Windows game folder's; Proton's
  DXVK reads it there ("Found config file: dxvk.conf" and both settings in the Proton
  log). It does not take the gplasync options: Proton's DXVK is not that build.
- **The saves.** The Proton prefix's `Documents/My Games/Oblivion/Saves` is a link to the
  Windows user's `Saves` folder, so the xOBSE co-saves (`.obse`) travel too; Steam Cloud
  carries only the `.ess` files. `Oblivion.ini` stays per system - the Windows one has
  four content mods and other settings.
- **The source.** One checkout on the shared partition; the build directories are per
  system (`build/` with MSVC, `build-linux-clangcl/` with clang-cl).

Building and deploying on Linux:

```
tools/build-linux.sh              # build OBVR.dll with clang-cl, deploy it and the map
tools/build-linux.sh --test       # the unit tests first
tools/build-linux.sh --assets     # the menus, the input manifest and the sounds too
tools/build-linux.sh --linux-only # the Linux game folder alone
```

Since 2026-10-14 it deploys to both game folders, the Linux one and the Windows
installation's on its partition (`OBVR_WINDOWS_GAME_DIR`, by default
`/run/media/$USER/Programs/Steam/steamapps/common/Oblivion`), so the two never run
different builds (the tester: "deployen immer both nach win und linux"). The clang-cl
DLL links the C runtime statically and imports only `KERNEL32`, `USER32` and `WINMM`, so
it loads on Windows as it does under Proton. It deploys nothing when the build fails, a
game folder is missing, or Oblivion runs, and says so.

The links need the Windows partition mounted; without it the game finds neither its
settings nor its saves. Until 2026-10-14 udisks mounted it on demand. Since then
`/etc/fstab` mounts both Windows partitions where udisks did, by UUID, `ntfs3` with
`windows_names` (no file names Windows cannot open), `nofail` (a missing disk does not
stop the boot) and `x-systemd.automount` (mounted at the first access); `findmnt
--verify` reports no errors or warnings, and systemd generated the automount units. The
first boot with them is still to come. Windows must not hibernate while Linux writes to
its partition; there is no `hiberfil.sys` on it, so fast startup is off.

## The game under Proton (gate 2): runs, first run 2026-10-14

Gate 1 came first: on 2026-10-14 the tester confirmed that SteamVR runs games on the
Bigscreen Beyond. The compositor drives it at 75 Hz with 3562x3562 recommended per eye
(the Windows side: 90 Hz, 4030x3383).

What the Linux installation got on 2026-10-14, every copy checked by SHA-256 against its
source:

- `Data/OBSE/Plugins`: `OBVR.dll` from `build/` (the Windows build of 09:08), the live
  `OBVR.ini` from the Windows game folder (not the repository's), `OBVR_Input`,
  `OBVR_Sounds`, `openvr_api.dll` with its licence, and every other OBSE plugin of the
  Windows installation with its INI files, MenuQue's subfolder included. Left behind:
  `shadeMe.dll.nah` (off on Windows too), logs, caches, the linker map and an INI
  backup. Every INI is byte-identical to its Windows twin.
- `Data/Menus`: OBVR's five XML files, which the Linux installation lacked.
- The game folder: `dxvk.conf` and MoreHeap (`Version.dll`, `MoreHeap.ini` with 512 MB
  for the game and the editor) from Windows. Still no `d3d9.dll`.
- The prefix: the `.obse` co-save beside every save; Steam Cloud had carried only the
  `.ess` files.
- Not copied: the Windows `Oblivion.ini` (the prefix's differs in 98 lines, among them
  `iMultiSample` 0 against 8 and HDR on against off) and the Windows installation's four
  content mods.

What Proton (`proton-cachyos-slr`, `cachyos-11.0-20261005-slr`, Wine 11.0) does, read
from its `proton` script and from the run's logs:

- It forces large-address awareness (`forcelgadd` is on unless switched off), so the
  unpatched `Oblivion.exe` here gets what the patched header gives the Windows copy.
- It loads Wine's own `version.dll` instead of MoreHeap's. Wine 11.0 takes a builtin over
  a native DLL beside the executable unless the builtin is flagged to prefer native, and
  Proton's `version.dll` is not; Wine's newer heuristic that prefers a native DLL by its
  version resource came after 11.0. The run's `+loaddll` line names `VERSION.dll` in the
  game folder and says `builtin`, and no `MoreHeap.log` was written. The documented way
  is the launch option `WINEDLLOVERRIDES="version=n,b" %command%`; Steam's console log
  shows the first run started without it, so it is untested.
- Its DXVK (v3.1.1-47) found `dxvk.conf` ("Found config file: dxvk.conf") and uses
  graphics pipeline libraries. It is not the gplasync build the Windows side runs.
- Its 32-bit `vrclient` carries OpenVR: OBVR connected through `IVRSystem_026` and
  `IVRCompositor_029`, took the back buffer's Vulkan image as on Windows (`Render: back
  buffer image=... 3560x3560 format=44`), and SteamVR loaded the action manifest from the
  path OBVR handed it, translated to `file:///home/nadi/.../OBVR_Input/actions.json`,
  together with the Knuckles bindings.

The first run, 13:00 to 13:03: a picture in the headset, and every feature the tester
tried worked. Two findings:

- Stutter. The compositor's summary of the session: 10,413 presents, 361 dropped and
  4,932 reprojected, about 47 % at 75 Hz. The Windows session that ended on 2026-10-13 at
  21:34: 40,825 presents, 3 dropped and 6,864 reprojected, about 17 % at 90 Hz. The
  cause, found the same afternoon: the monitor window's vsync, "The stutter" below.
- A crash when the session's first enemy health bar read its frames: the open bug of
  2026-10-14 in `docs/hud-on-hands-spec.md`. A read through a value that only looks like
  a pointer went unnoticed on Windows, where the address happened to be readable, and
  faulted under Wine.

## The harness (gate 3): runs, since 2026-10-14

`tools/hand-script-run.py` runs the hand script scenarios under Proton
(`docs/hand-script-harness.md`, "On Linux, under Proton"). It sidesteps every input
pitfall `HANDOFF.md` section 12 recorded by typing nothing into the game: OBVR loads the
harness save from the main menu itself (`[Debug] HandScriptLoad`) and hands the
scenario's console lines to the game through xOBSE's console interface (`[Debug]
HandScriptConsole`, `src/test/HandScriptConsole.h`). The runner starts the game through
Steam, follows `OBVR.log`, takes the game window's picture from its X11 window with
ImageMagick, and checks the log exactly as the Windows runner does; its pure part
(`tools/hand_script_runner.py`) is covered by `hand_script_runner` in the tests.

The first runs, 2026-10-14: `load-only`, `walk-direction` (head turn and stick),
`hand-shapes` (two `console` lines from inside), `hit-blur-on` (a `console-at` line) and
`stereo-view` (three `dump` lines) passed; the game window's pictures show the world with
the HUD. SteamVR on Linux wrote no files for `dump` - three requests in `stereo-view`,
no picture anywhere under the home folder or `/tmp`, nothing in the compositor's log;
why, I could not verify. `enemy-health` passed its bar checks with the new height, but
in two of three runs only one of the two bandits it places fought, so its "bar 2" lines
were missing; on Windows it passed with both. `activate-takes` failed: the hand never
reached the dropped dagger. The dagger lay where it lay in the Windows PASS runs of
2026-09-28 and 2026-09-30 (252 -1585 -252), but the player stood about 85 units further
back (the eyes at y -1751 against -1666) - the harness save was written again on
2026-10-01 16:10 (its header's save time), after the last of those runs, so the scenario
predates the save it runs on; not a Linux matter. The console lines now wait four seconds
after the start, as the Windows runner's typing did; the dagger lay in the same place
either way.

A sweep of all scenarios, started the same afternoon, was stopped by the tester after five
("ein paa grün das reicht mir als nachweis"): `armour-sheath-probe` passed;
`activate-takes` failed as above; `arrow-stab` missed "Hands: the arrow in the hand met";
`attack-frequency` ended with the game gone after a message box and the pause menu came
up, which the stop may have been; `attack-tempo` was the run interrupted. Not looked
into. The interrupted run's cleanup found the game not yet started (Steam begins a
launch a few seconds after the request) and the game came up after it with the player's
configuration; the runner now waits up to 30 seconds for a launch it asked for before it
gives up on it.

## The stutter: the monitor window's vsync (found 2026-10-14)

Other VR games under Proton on the same machine that day reprojected little (Blade &
Sorcery 7.5 %, Subnautica 3 to 6 %), with the compositor at realtime priority and
asynchronous reprojection on, so the machine and SteamVR were not the suspects. The
harness measured the game instead: `tools/hand-scripts/perf-walk.txt`, two minutes in
the harness save's hall (looking around, walking there and back, standing), with OBVR's
profiler on. The runner puts SteamVR's frame summary for the run under "Measured" and
analyses the profiler's capture in the run folder.

| Run | Start | Reprojected | Dropped | `interop_lock` mean / p95 | `begin_frame` median |
|---|---|---:|---:|---:|---:|
| A | Steam, `PROTON_LOG=1` | 36.5 % of 9,478 | 531 | 2.50 / 13.24 ms | 13.04 ms |
| B | direct, no `PROTON_LOG` | 37.3 % of 6,673 | 410 | 2.32 / 13.02 ms | 12.75 ms |
| C | direct, `DXVK_CONFIG=d3d9.presentInterval = 0` | 1.3 % of 9,905 | 70 | 0.026 / 0.053 ms | 9.59 ms |

- **The cause: the game's own window waits for the monitor.** `Oblivion.ini` has
  `iPresentInterval=1`, the game's vsync and its default, so DXVK presents the monitor
  window in FIFO mode at the monitors' 59.91 Hz. DXVK presents from its submission thread
  while it holds the queue's mutex, and `LockSubmissionQueue` first waits for that thread
  to finish what is queued, then takes the same mutex (DXVK's master branch, read
  2026-10-14, Proton ships v3.1.1: `submitCmdLists` and `lockDeviceQueue` in
  `src/dxvk/dxvk_queue.cpp`, `lockSubmission` in `src/dxvk/dxvk_device.h`,
  `src/d3d9/d3d9_interop.cpp`). OBVR takes that lock about five times a frame to hand its
  Vulkan images to SteamVR. A lock waited 2.3 to 2.5 ms on average and 13 ms at p95: about 11 to 12 ms of
  every 13.3 ms frame at 75 Hz. With the monitor's vsync off (run C) a lock takes 0.03 ms,
  the frame waits on SteamVR's pose call instead (`poses_wait` median 9.5 ms, 0.15 ms
  before), and reprojection fell from 37 % to 1.3 %.
- **Not gplasync.** Proton's DXVK is not the gplasync build, and run C ran the same DXVK
  as runs A and B.
- **Not Proton's logging.** `PROTON_LOG=1` made no measurable difference (runs A and B).
- **Windows** has `iPresentInterval=1` as well and runs DXVK, the gplasync build, with the
  same lock. Whether part of its 17 % at 90 Hz comes from the same wait, I could not
  verify; a run there with and without the setting would tell.
- **The fix, built 2026-10-14.** While OBVR drives the headset, its `CreateDevice` hook
  (`src/render/ResolutionHook.cpp`) gives the game's device the immediate present
  interval (`render/PresentInterval.h`), so the monitor window no longer waits for the
  monitor and SteamVR alone paces the frame. It changes the parameters, not the INI
  setting, which the engine writes back. `[Render] MonitorVsync=1` keeps the game's own
  interval. Without a headset nothing changes. Like the eye size it is set at device
  creation only; a device reset by the game would take both back, and DXVK logged no
  reset besides the creation in the runs of 2026-10-14. Harness, through Steam with the
  same launch options as runs A and B:

  | Run | Setting | Reprojected | Dropped | `interop_lock` mean / p95 |
  |---|---|---:|---:|---:|
  | D | default, `MonitorVsync=0` | 0.6 % of 9,956 | 12 | 0.007 / 0.018 ms |
  | E | `MonitorVsync=1` | 37.5 % of 9,312 | 670 | 2.33 / 13.22 ms |

  The change reaches Windows as well, where it is unmeasured.
- **OBVR's GPU timings** read 0 for every interval under Proton; not looked into.
- Run B ended nine seconds after its last mark, through the pause menu and its quit
  message, the pattern `attack-frequency` showed in the sweep; what sent it, I could not
  verify. Its numbers up to that point are in the table.

## Analysis tooling

- **REA** (`rea-agents@6.4.0`) is registered as a user-scope MCP server with the Ghidra
  provider: Ghidra 12.1.4 and a Temurin JDK 21.0.12.1 under `~/tools`, the SHA-256 of
  both archives checked against the publishers' values; Ghidra's
  `application.properties` asks for Java 21 and sets no maximum. `rea doctor --provider
  ghidra` reports every Ghidra check healthy and the whole as unhealthy, because Node
  25.8.1 is not one of the lines it supports (22.19+, 24.11+, 26+). In practice `rea
  inspect` of `Oblivion.exe` ran through on 2026-10-14 in 144 s and found 21,466
  procedures, 14,185 strings and 7 segments, the `.bind` Steam wrapper among them,
  which matches the Windows run of 2026-10-13 (21,467 and 14,185).
- **pyffi** for reading NIF and BSA files: a venv under `~/obvr-venv` with Python 3.14.
  The PyPI release 2.2.3 does not import on Python 3.12 or newer (`time.clock`,
  `distutils`); the pre-release `pyffi==2.2.4.dev4` together with `setuptools` (which
  supplies `distutils`) does.
- `ydotool` with `ydotoold`, and `xdotool`, are installed.

## Open items

- A Node LTS for REA's doctor, or live with the warning.
- The enemy bar crash: fixed and built with clang-cl on 2026-10-14
  (`docs/hud-on-hands-spec.md`). The tester's run that afternoon had bars over four
  fighters, the frames read and no crash; the bars' height lowered to 0.25 m since.
- The stutter under Proton: fixed ("The stutter" above); the headset and a Windows
  measurement are open. OBVR's GPU timings under Proton.
- MoreHeap under Proton: start once with the override and look for `MoreHeap.log` and a
  `native` load line.
- The Windows `Oblivion.ini` and the content mods for the Linux installation: the
  tester's decision.
- Gate 3: a sweep of every scenario under the Linux runner, when the tester wants it;
  the scenarios older than the harness save of 2026-10-01; `dump` pictures on Linux;
  whether `tools/hand-script-run.ps1` should hand its console lines to OBVR too (it would
  lose the two open console bugs of `docs/hand-script-harness.md`), to be tried on Windows.
