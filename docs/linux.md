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
  21:34: 40,825 presents, 3 dropped and 6,864 reprojected, about 17 % at 90 Hz. Not
  investigated yet.
- A crash when the session's first enemy health bar read its frames: the open bug of
  2026-10-14 in `docs/hud-on-hands-spec.md`. A read through a value that only looks like
  a pointer went unnoticed on Windows, where the address happened to be readable, and
  faulted under Wine.

## The harness (gate 3): not started

The game side of the harness (the script file, the synthetic head and controllers, the
marks, the compositor dumps) lives in `OBVR.dll` and travels. The runner,
`tools/hand-script-run.ps1`, is Windows: `keybd_event` for the console, window focus,
`PrintWindow` screenshots, the SteamVR start, the SteamVR screenshot folder, process
handling. A Linux runner has to respect what `HANDOFF.md` section 12 recorded: the game
ignores `xdotool`; `ydotool` works, but only with real focus (`ydotoold` runs here and
the user is in `input`); synthetic clicks do not arrive in the main menu; `proton run`
alone aborts silently, so the route goes through the Steam Linux Runtime. Whether
SteamVR on Linux writes the compositor dumps to the same place is unverified.

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
  (`docs/hud-on-hands-spec.md`); the run in the headset is open.
- The stutter under Proton (gate 2 above).
- MoreHeap under Proton: start once with the override and look for `MoreHeap.log` and a
  `native` load line.
- The Windows `Oblivion.ini` and the content mods for the Linux installation: the
  tester's decision.
- Gate 3 as above.
