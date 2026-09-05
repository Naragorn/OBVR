# OBVR - TES4 Oblivion (2006) in VR

Native VR for the original **The Elder Scrolls IV: Oblivion**. Not for the Remastered
edition.

Guiding idea: *Oblivion stays Oblivion.* OBVR does not replace gameplay. It puts real
stereo VR on top of the game's own camera and render pipeline, as an xOBSE plugin, and
leaves the game exactly as it was on a machine without a headset.

```
Vanilla Oblivion camera
        ×
tracked head pose
        =
the VR camera, rendered once per eye
```

**Status: early public test build (0.1.1).** One developer, one headset, one machine so
far. The seated experience below works and is what this release is for; the standing
experience with motion controllers is under construction and switched off. Please test,
and please report what you see - see [Reporting a problem](#reporting-a-problem).

## What works

Everything here has been confirmed in a headset.

- **Dual-pass stereo.** The world is rendered twice per game frame, once per eye, from
  cameras one interpupillary distance apart, into eye-sized targets at the headset's own
  resolution. Skinned bodies stay intact across the second render. **Alternate eye
  rendering** (`Stereo=aer`) is there as well: the world is drawn once per frame from
  alternating eyes and the other eye keeps its last picture, which costs nothing extra
  but shows a one-frame disparity on fast motion. Dual is the default; aer is the
  fallback if dual misbehaves on your setup.
- **6DoF head tracking.** The head rotates and moves the camera; leaning works. The
  character never turns with the head. Locomotion stays with mouse, keyboard or gamepad.
- **Head-based aiming.** Bows, spells and melee go where the head looks, in first and
  third person, while the walking direction stays with the movement controls. In first
  person the weapon visibly points where the shot goes.
- **One crosshair, at the depth the aim ray hits**, instead of a flat reticle that reads
  as two. Shown in third person and with a weapon drawn as well.
- **HUD, menus and dialogue in the headset.** The 2D layer is lifted out of the frame and
  shown on an overlay in front of the world. In-game menus keep the world behind them in
  stereo with Oblivion's own sepia pause look; the main menu, loading screens and videos
  take a cinema screen. The dialogue zoom is disabled.
- **OBVR's own settings menu in the headset** (`Insert`), and a hot-reloaded `OBVR.ini`
  for every setting.
- **Recenter** on a key (`Del` by default).
- **Smooth turning** as a comfort option for the mouse and stick turn.
- Works with and without the 4GB patch, and under Mod Organizer 2 without Root Builder.
- Refuses to patch anything if another plugin got there first, and names it in the log.

## Built, not yet confirmed in a headset

These are in the build and covered by tests, but nobody has looked at them through a
headset yet. Reports on them are especially useful.

- **The first-start walkthrough**: a few pages in the headset that choose the way to play
  and set the comfort basics. Same machinery as the settings menu.
- **The live world behind the pause menus** (`Render.LiveMenuBackground`) and the option
  to keep it unpaused (`Render.UnpausedMenus`).
- **Menus mirrored onto the monitor window** while the overlay carries them
  (`Render.MirrorMenusToMonitor`), so the game stays controllable from the desk.
- **The hang watchdog**: a thread that writes where a frame stood when frames stop.
- **The freeze fix.** Freezes shortly after a switch to third person were traced to
  OpenVR's pose wait racing DXVK's submissions on the same queue; the fix is in, its
  confirmation is open. If the game freezes, the log's last lines are the evidence.

## Under construction - switched off

The **standing experience**: motion controllers in both hands, the weapon in the right
one, swings that strike what they pass through, a shield raised to block, spells from the
left hand, menus on the wrists with a laser and finger presses, controllers steering every
menu from the main menu on. All of it is built (`docs/hand-tracked-mode.md` is the ladder,
rung by rung), **none of it has been seen working in a headset, and it is not working as
intended**. It is off by default, the walkthrough shows it but refuses it, and
`[Hands] ControllerMenus` is off with it. `[Hands] Enabled=1` in `OBVR.ini` or "Hand
tracking" in the settings menu switch it on at your own risk; a report from doing so is
welcome, marked as such.

Not built at all: snap turning, teleport, room-scale locomotion, physical interaction with
objects by hand, real finger tracking (`IVRInput`), Linux under Proton.

## Known issues

- The recenter key does nothing during the intro films: the render pose cannot be read
  for the first seconds, so the picture rides the head until the main menu.
- Text entry (a character's name) still needs the keyboard.
- Oblivion Reloaded and its derivatives are incompatible; see
  [Compatibility](#compatibility).

## Requirements

- Oblivion **1.2.0.416**, 32-bit. Every edition that carries this executable version is
  supported: the original release patched to 1.2.0.416 and the **Game of the Year**
  edition alike. By store: **Steam (tested)**, **GOG (untested)**, retail disc with the
  final patch (untested). OBVR checks the version and stays inactive on any other. Not
  the Remastered edition.
- [xOBSE](https://github.com/llde/xOBSE/releases/latest) 22.13 or newer.
- **SteamVR**, and a headset it drives. OBVR talks OpenVR; there is no OpenXR path, and
  the reason is bitness: a 32-bit process needs a 32-bit runtime, and SteamVR's OpenVR
  has always shipped one. See [Why OpenVR](#why-openvr).
- **DXVK** as the game's `d3d9.dll`. This is not optional. OpenVR's `Submit` has no entry
  for a Direct3D 9 texture, and DXVK is what turns Oblivion's frame into a Vulkan image the
  compositor accepts. Without it OBVR says so in the log and shows a test pattern. The
  dependency-free alternative, a D3D9Ex device shared into D3D11, was tried on the binary
  and the game crashes on it, on Microsoft's runtime and DXVK alike; see `HANDOFF.md`.
- Windows 10 or 11. Linux under Proton is the intended second platform, not a supported
  one yet.

Tested on: one headset (a Dream Air) with Valve Index controllers through SteamVR, an
RTX 4090, Windows 11. Anything else is untested, which is exactly what reports are for.

## Installing

1. Install [xOBSE](https://github.com/llde/xOBSE/releases/latest): `obse_1_2_416.dll`,
   `obse_editor_1_2.dll`, `obse_steam_loader.dll`, `obse_loader.exe` and its `Data`
   folder into the Oblivion directory.
2. Install [DXVK](https://github.com/doitsujin/dxvk/releases/latest): from the archive's
   `x32` folder, put `d3d9.dll` next to `Oblivion.exe`. Only that one file.
3. Download `OBVR-<version>.zip` from this repository's **Releases** page and extract it
   into Oblivion's `Data` folder. It contains `OBSE/Plugins/OBVR.dll`,
   `OBSE/Plugins/OBVR.ini` and the license text, nothing else.
4. Copy the **32-bit** `openvr_api.dll` from SteamVR - it is at
   `Steam\steamapps\common\SteamVR\bin\win32\openvr_api.dll` - into `Data/OBSE/Plugins/`
   next to `OBVR.dll`. The 64-bit one from `bin/win64` will not load into Oblivion.
5. Start SteamVR, then start the game through the OBSE loader (Steam users: the Steam
   loader DLL does this for the normal Play button).

The first start opens the walkthrough in the headset: choose the seated experience, set
the comfort basics, done. `OBVR.log` is written next to `Oblivion.exe`; the previous
run's log survives as `OBVR.log.prev`. Both are the first thing to attach to a report.

To check that everything is in place, `OBVR.log` opens with the OBVR version and the
xOBSE and Oblivion versions found, and says further down whether DXVK answered ("DXVK")
and whether SteamVR was reached ("OpenVR").

### Mod Organizer 2

MO2 virtualises `Data` and nothing else. xOBSE's loader files live in the game root and
need [Root Builder](https://kezyma.github.io/?p=rootbuilder); OBVR does not, because it
anchors its own files on `OBVR.dll`:

| File | Where OBVR looks | Under MO2 |
| --- | --- | --- |
| `OBVR.ini` | next to `OBVR.dll` first, then the game root | virtualised, ships with the mod |
| `openvr_api.dll` | next to `OBVR.dll` first, then the default search | virtualised, put it in the mod |
| `OBVR-crosshair.cache` | next to `OBVR.dll` | virtualised |
| `OBVR.log` | game root, always | written for real |

So the release archive is an ordinary MO2 mod: install it from the archive as it is, with
`OBSE/Plugins/` inside and no `Root` folder, and drop `openvr_api.dll` into the same
`OBSE/Plugins/` folder of that mod.

### Uninstalling

Delete `OBVR.dll`, `OBVR.ini`, `OBVR-crosshair.cache` and `openvr_api.dll` from
`Data/OBSE/Plugins/`. OBVR writes nothing else and touches no save. `[Camera] HookEnabled=0`
in `OBVR.ini` is the quick way to rule OBVR out without removing it.

## Reporting a problem

Open an issue on this repository's **Issues** tab; the bug report form asks for what
follows. The short version:

- **`OBVR.log` and `OBVR.log.prev`** from the Oblivion directory. The log is the evidence;
  a report without it can rarely be acted on. It contains the install path and nothing
  else personal.
- What you did and what you saw: both eyes, one eye, the monitor, the log's last line.
- Headset, controllers, SteamVR version, GPU and driver, Windows version.
- DXVK version, 4GB patch yes or no, Mod Organizer 2 yes or no, and the other xOBSE
  plugins and mods loaded. Compatibility with other renderer and camera mods is the most
  likely cause of a problem and the least known.
- Whether the standing experience was switched on. It is under construction; reports from
  it are welcome but read differently.

A freeze rather than a crash: note whether the monitor window still updated, and whether
SteamVR's own view still tracked. The watchdog's lines in the log ("Watchdog: ...") say
where the game stood.

## Configuration

Every setting lives in `OBVR.ini`, each with its reasoning written above it, and the file
is hot reloaded while the game runs. The settings menu in the headset (`Insert`) has the
ones most people touch:

| Key | What it does |
| --- | --- |
| `[Head] Source` | `openvr` for a headset; `simulated` or `fixed` to check the camera chain without one |
| `[Head] RecenterKey` | virtual-key code of the recenter key, `Del` by default |
| `[Render] Stereo` | `dual` for real stereo; `aer` for alternate eye rendering; `none` for a flat picture |
| `[Render] EyeSeparationScale` | how far apart the two viewpoints are, as a multiple of your real eye distance; `1.0` is true to life, above it is the depth boost (see below) |
| `[Render] Menus` | `world` keeps menus in front of the world, `cinema` puts them on a flat screen |
| `[Render] LiveMenuBackground` | render the paused world freshly per eye behind menus |
| `[Render] MirrorMenusToMonitor` | composite in-game menus back onto the monitor window |
| `[Render] Crosshair*` | the crosshair, its depth, size and when it shows |
| `[Look] *` | what happens to the look controls once a headset drives the camera, smooth turning, and the head-based aiming options |
| `[Hands] Enabled` | the standing experience, under construction, off |
| `[Onboarding] ShowAtStart` | the first-start walkthrough |

**Depth boost.** `EyeSeparationScale` is OBVR's version of the "3D Depth Boost" that
PrimaShock's mod of the OpenXR Toolkit made popular in the flight and racing sim crowd
(https://primashock.com/openxr-toolkit-mod-by-primashock-vr/). It moves the two
viewpoints further apart than your real eyes are, so everything gets more parallax: more
felt depth and separation up close, and a world that reads proportionally smaller. Those
come as one package. Where the toolkit mod reprojects finished images, OBVR moves the
camera before each eye is rendered, so there is no warping; each eye is genuinely drawn
from the wider viewpoint. `1.0` is the default and geometrically true to your headset's
own eye distance. Values close to 1 are the useful range; past about 1.5 most people
trade depth for eye strain. It is hot reloaded, so it can be tuned from inside the
headset.

## Compatibility

**Oblivion Reloaded, Oblivion Reloaded Combined, E3: incompatible.** Both take over the
same functions. Oblivion Reloaded plants a detour at the entry of the scene render
(`0x0040C830`) in every version, and the 8.x-derived builds (E3, ORC) also at the camera
update (`0x0066BE6E`) and the dialogue camera (`0x0066C6F0`). OBVR needs all three. It
verifies every site before patching, refuses one that is already patched, and the log
names the DLL that got there first. Whichever loads first keeps the site; the other loses
the feature, and with the scene render that means no stereo. There is no configuration
that makes the two coexist.

**ENB**: not tested. ENB replaces `d3d9.dll`, which is where DXVK has to sit.

**Enhanced Camera**: not tested. It patches the first-person camera OBVR also reads.

Other xOBSE plugins that do not touch the renderer or the player camera are expected to
work; the engine fixes commonly installed alongside have been in the test load order
throughout. Reports either way are welcome.

## How it works, briefly

- The camera hook sits at `0x0066BE6E`, immediately after Oblivion has written the
  player camera into the scene graph, and rewrites the camera's local transform from the
  head pose. The trampoline bytes are generated at runtime, so there is no inline assembly
  and the generator is unit-tested.
- The scene render entry (`0x0040C830`) is detoured so the world is drawn twice, the camera
  stepped one eye over between the draws. The frame clock is zeroed for the second draw so
  nothing animates twice.
- The 2D pass (`0x0057F170`) is redirected into a texture of OBVR's own, which becomes an
  OpenVR overlay. Direct3D device methods are hooked through the device's method table for
  the back buffer copy, the bone lock and the render-target bookkeeping.
- Aiming is set at the source: the player's attack update and the projectile factory are
  wrapped so they read the gaze instead of the body's heading, and nothing else does.
- Every reverse-engineered address in `src/game/GameAddresses.h` carries the evidence it
  was read from, and is verified against the binary before it is patched.

`HANDOFF.md` is the long version: the coordinate systems, the axis conventions, every
dead end and why it was a dead end. `docs/hand-tracked-mode.md` is the same for the
standing experience. `docs/vr-modding/` is the knowledge base written for coding agents:
the architecture, the stereo model, the engine facts, the failed approaches and the open
questions, classified by how well each is established, plus a playbook for bringing
another flat game into VR.

## Why OpenVR

Oblivion is a 32-bit process, so OBVR is a 32-bit DLL, and it needs a 32-bit VR runtime.
OpenVR's `openvr_api.dll` has always shipped for x86 and is out-of-process by design;
SteamVR's OpenXR does not serve 32-bit applications, and under Proton `wineopenxr` is not
built for i386 at all. Through SteamVR one OpenVR backend reaches more headsets than every
32-bit OpenXR runtime combined. `TrackerSource` is an enumeration of interchangeable
sources, so a second backend can be added without touching the camera code.

## Building

Oblivion is 32-bit, so the DLL must be too. With Visual Studio 2022 Build Tools:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

or with the Visual Studio generator:

```
cmake -B build -A Win32
cmake --build build --config Release
```

Name the build type either way: an unoptimised DLL loads, but it is not what was tested.
The DLL depends on nothing beyond `kernel32`, `user32` and `msvcrt`; a cross build without
the Windows SDK exists as a verification route (`cmake/toolchain-linux-nosdk.cmake`).
The version comes from the `project()` line in `CMakeLists.txt` and is the first thing
the log says.

### Tests

The tests are a separate CMake project that runs natively on the development machine.
Every decision the hooks make is lifted into pure functions over plain values so it can be
exercised without a running game: trampoline bytes, rotation maths, the frame decisions,
the INI parser, the bone-lock pairing, the compositor submit policy, the watchdog, the
menus, the walkthrough, the hand mode, and so on. Fifty test binaries at the time of
writing.

```
cmake -B build-tests tests -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Build them with the 32-bit toolchain too (`vcvars32`, or `-A Win32` with the Visual
Studio generator): the layout checks on the mirrored Windows and Direct3D structures
hold for the pointer size the DLL is built for.

`docs/verification/` holds logs and screenshots from the game runs that confirmed each
milestone; `tools/` holds the scripts that start the game and take measurements without a
person in the headset.

### Releases

`tools/package-release.ps1` builds the release archive: it reads the version from
`CMakeLists.txt`, takes `build/OBVR.dll` and `OBVR.ini`, and writes
`dist/OBVR-<version>.zip` laid out for both a manual install and Mod Organizer 2, with the
linker map beside it for reading a crash address back to a function. The GitHub Actions
workflow in `.github/workflows/build.yml` builds the DLL and runs the tests on every push
and keeps the DLL as an artifact.

## Working on it

Two rules shape every change here, and pull requests are read against them:

- **Two sources for every address.** A reverse-engineered address is used only when the
  disassembly of the actual binary and an independent source (xOBSE, NorthernUI, another
  plugin, or a second code path in the binary) agree on it.
- **Cause, not workaround.** A change that makes a symptom go away without an explanation
  of why it appeared has not fixed anything. The commit history is written in that spirit
  and is the first place to look for why something is the way it is.

Everything in the repository - code, comments, commits, documents - is in English.

## Supporting the work

OBVR is free and stays free. If it puts you in Cyrodiil and you want to buy a coffee for
the evenings it took, there is a Patreon: https://www.patreon.com/naragorn

Patrons get new builds of this and future VR mods before they go public, with the
testing notes that come with them, and from the higher tiers a vote on which game gets
the VR treatment next. Early access is a head start, not a lock: OBVR is GPL-3.0, and
every build reaches this repository.

Tips in crypto are welcome too:

| Coin | Address |
| --- | --- |
| BTC | `bc1q78akvct56ctrgcz0ahzthm3sjayu96tl8fdhc4` |
| ETH (Ethereum network) | `0x94c3Ec9707a1ac383F40Bb55Bff63d37175AC666` |
| SOL | `5GYdnxLnKByKuptpHtqkcmCx8LTjaMm5BtkoFN4jvCEU` |

## License

OBVR is free software under the **GNU General Public License, version 3** (`LICENSE`).
Copyright (C) 2026 Naragorn.

In plain words: use it, change it, share it, build on it, show it, fund your own work on it.
What the license rules out is taking it away from the commons: whoever passes OBVR on,
changed or not, has to pass the source on with it under this same license, keep the
copyright notice, and mark what they changed. A closed or paid-for build of OBVR, with or
without additions, is therefore not possible. The license text is the authority; this
paragraph is only its summary.

`src/obse/PluginInterface.h` reproduces the layout of two structs from xOBSE's
`PluginAPI.h` for binary compatibility, the way every OBSE plugin does; xOBSE publishes
that header without a license of its own.

## References

- [llde/xOBSE](https://github.com/llde/xOBSE) - the plugin loader. `src/obse/PluginInterface.h`
  is a minimal, binary-compatible replica of its `PluginAPI.h`.
- [ValveSoftware/openvr](https://github.com/ValveSoftware/openvr) - `openvr_api.dll` and the
  headers `src/vr/OpenVRTypes.h` was written against.
- [doitsujin/dxvk](https://github.com/doitsujin/dxvk) - the Direct3D 9 to Vulkan layer and
  the interop interfaces OBVR uses to hand frames to the compositor.
- [DavidJCobb/NorthernUI](https://github.com/DavidJCobb/NorthernUI) and
  [llde/TESReloaded](https://github.com/llde/TESReloaded) - reverse-engineering references;
  every address taken from them was checked against the binary first.
- [openRBRVR](https://github.com/Detegr/openRBRVR) - the closest relative: a 32-bit D3D9
  game with an in-process VR mod.
