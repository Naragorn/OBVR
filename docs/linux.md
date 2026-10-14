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

### The DLL: the SDK-free cross build no longer builds

`cmake/toolchain-linux-nosdk.cmake` compiles OBVR with clang for
`i686-pc-windows-msvc` without the Windows SDK, without the C runtime headers and
without a C++ standard library (`-nostdinc++`), on the premise, true in August 2026,
that OBVR needs nothing beyond kernel32 and msvcrt. The code has since taken the
standard library on board: `<atomic>`, `<new>` and `<cstdint>` arrived with `3b883d9`
(2026-09-23), `<vector>` and `<string>` with `008e70a` (2026-09-27), `<limits>` with
`fb3ef31` (2026-10-01), and `<cstddef>`, `<cstdio>`, `<cstring>` and `<cmath>` are
included directly in dozens of files rather than through the `OBVR_NO_WINSDK` switch
that `core/MathFns.h` shows. On 2026-10-14 the toolchain configured and generated the
import libraries, and then 106 of the 145 translation units failed on a missing header
(67 `<cstddef>`, 12 `<cstdio>`, 12 `<atomic>`, 5 `<cmath>`, 3 `<cstring>`, 2
`<vector>`, 2 `<new>`, 1 `<cstdlib>`).

There is no minimal fix: a freestanding build has no `std::atomic`, `std::vector` or
`std::string`, and taking them back out of the DLL would be feature work for the sake of
a verification build. The ways forward, none taken yet:

1. **clang-cl with the MSVC headers and libraries**, fetched by
   [xwin](https://github.com/Jake-Shadle/xwin) (Apache-2.0/MIT; it downloads the
   Microsoft CRT and Windows SDK and asks the user to accept Microsoft's licence). The
   same ABI and runtime as the Windows build, so the result would be the real DLL rather
   than a verification artefact; no root needed; a download in the hundreds of
   megabytes.
2. **MinGW-w64** (`clang --target=i686-w64-mingw32` or the mingw GCC). A different C and
   C++ runtime linked statically, the `_MSC_VER` branches (`core/AtomicFlag.h`,
   `game/HandAdjust.cpp`, `game/VRMenuBridge.cpp`, `game/VRMenuIntentStub.cpp`) taking
   their so far untested `#else` side, `<intrin.h>` from mingw rather than MSVC; root
   for the packages.
3. **Drop the cross build** and take the DLL from Windows or from CI
   (`.github/workflows/build.yml` keeps `build/Release/OBVR.dll` for every push).

Until one is chosen, the DLL that runs under Proton is the one Windows built. On
2026-10-14 that was `build/OBVR.dll` of 1,339,392 bytes, byte-identical in size to the
one deployed in the Windows game folder.

## The game under Proton (gate 2): not started

State of the Linux installation on 2026-10-14, before anything was changed:

- Oblivion 1.2.0.416 from Steam; xOBSE 22.13 installed on 2026-08-25 with the loader
  swap from `HANDOFF.md` section 8 (`OblivionLauncher.exe` is `obse_loader.exe`, the
  original kept as `OblivionLauncher.exe.vanilla`).
- `Data/OBSE/Plugins` holds one `OBVR.dll` of 17 KB from 2026-08-25 and nothing else; an
  `OBVR.ini` of the same age sits in the game folder, where the plugin stopped looking
  for it long ago. No `d3d9.dll`, which is right: Proton brings its own DXVK.
- The saves in the prefix (`compatdata/22330/pfx/drive_c/users/steamuser/Documents/My
  Games/Oblivion/Saves`) include `OBVRHarness.ess`, the save the harness loads, but not
  the `.obse` co-save that lies beside it on Windows.

To do, in this order: copy the current `OBVR.dll`, the live `OBVR.ini` from the Windows
game folder (not the repository's), `OBVR_Input`, `OBVR_Sounds` and the x86
`openvr_api.dll` into `Data/OBSE/Plugins`; bring the `.obse` co-saves; start through
Steam with SteamVR running; read `OBVR.log` for the `Render: back buffer image=` line
and the OpenVR initialisation lines. The question nobody has tested: OBVR hands DXVK's
Vulkan images to the compositor (`src/render/DxvkInterop.h`); under Proton, Valve's own
DXVK and `openvr_api_dxvk.dll` sit in between, and Proton's `vrclient` for 32-bit
processes is what carries OpenVR there. If no picture reaches the headset, read Proton's
`vrclient` and DXVK sources before changing anything.

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
- Decide the DLL build route (above).
- Gate 1, the headset: SteamVR was running on this machine during the session of
  2026-10-14 (`vrserver`, `vrcompositor`, `vrmonitor`); whether the headset shows a
  picture was not checked from here. `HANDOFF.md` section 11 describes the 2026-08-25
  state, in which starting SteamVR took the desktop down.
- Gates 2 and 3 as above.
