# OBVR

Native VR for the original **The Elder Scrolls IV: Oblivion (2006)**. Not for the
Remastered edition.

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

## What it does today

Everything below has been confirmed in a headset unless marked otherwise.

- **Dual-pass stereo.** The world is rendered twice per game frame, once per eye, from
  cameras one interpupillary distance apart, into eye-sized targets at the headset's own
  resolution. Skinned bodies stay intact across the second render (a bone-palette lock
  works around an engine mixup that only a second render triggers).
- **6DoF head tracking.** The head rotates and moves the camera; leaning works. The
  character never turns with the head. Locomotion stays with mouse, keyboard or gamepad.
- **Head-based aiming.** Bows, spells and melee go where the head looks, in first and third
  person, while the walking direction stays with the movement controls. In first person the
  weapon visibly points where the shot goes.
- **One crosshair, at the depth the aim ray hits**, instead of a flat reticle that reads as
  two.
- **HUD, menus and dialogue in the headset.** The 2D layer is lifted out of the frame and
  shown on an overlay in front of the world. In-game menus keep the world behind them in
  live stereo, with Oblivion's own sepia pause look; the main menu, loading screens and
  videos take a cinema screen. The dialogue zoom is disabled. Menus are also composited
  back onto the monitor window so the game stays controllable from the desk (this one is
  new and not yet seen with a headset running).
- **OBVR's own settings menu in the headset**, and a hot-reloaded `OBVR.ini` for every
  setting.
- **Recenter** on a key (`Del` by default).
- Works with and without the 4GB patch, and under Mod Organizer 2 without Root Builder.

Not in scope today: motion controllers, hands, room-scale locomotion, physical
interaction. Head-tracked aiming is the whole of the input model. A hand-tracked mode is
being started on its own branch.

## Requirements

- Oblivion **1.2.0.416** (the Steam and GOG builds), 32-bit. OBVR checks the version and
  stays inactive on any other.
- [xOBSE](https://github.com/llde/xOBSE/releases/latest) 22.13 or newer.
- **SteamVR**, and a headset it drives. OBVR talks OpenVR; there is no OpenXR path, and
  the reason is bitness: a 32-bit process needs a 32-bit runtime, and SteamVR's OpenVR
  has always shipped one. See "Why OpenVR" below.
- **DXVK** as the game's `d3d9.dll`. This is not optional. OpenVR's `Submit` has no entry
  for a Direct3D 9 texture, and DXVK is what turns Oblivion's frame into a Vulkan image the
  compositor accepts. Without it OBVR says so in the log and shows a test pattern. The
  dependency-free alternative, a D3D9Ex device shared into D3D11, was tried on the binary
  and the game crashes on it, on Microsoft's runtime and DXVK alike; see `HANDOFF.md`.
- Windows. Linux under Proton is the intended second platform, not a supported one yet.

## Installing

1. Install xOBSE: `obse_1_2_416.dll`, `obse_editor_1_2.dll`, `obse_steam_loader.dll`,
   `obse_loader.exe` and its `Data` folder into the Oblivion directory.
2. Put DXVK's 32-bit `d3d9.dll` next to `Oblivion.exe`.
3. Put `OBVR.dll`, `OBVR.ini` and the **x86** `openvr_api.dll` (SteamVR ships it under
   `bin/win32/`) into `Data/OBSE/Plugins/`.
4. Start SteamVR, then start the game through the OBSE loader (Steam users: the Steam
   loader DLL does this for the normal Play button).

`OBVR.log` is written next to `Oblivion.exe`; the previous run's log survives as
`OBVR.log.prev`. Both are the first thing to attach to a bug report.

### Mod Organizer 2

MO2 virtualises `Data` and nothing else. xOBSE's loader files live in the game root and
need [Root Builder](https://kezyma.github.io/?p=rootbuilder); OBVR does not, because it
anchors its own files on `OBVR.dll`:

| File | Where OBVR looks | Under MO2 |
| --- | --- | --- |
| `OBVR.ini` | next to `OBVR.dll` first, then the game root | virtualised, ships with the mod |
| `openvr_api.dll` | next to `OBVR.dll` first, then the default search | virtualised, ships with the mod |
| `OBVR-crosshair.cache` | next to `OBVR.dll` | virtualised |
| `OBVR.log` | game root, always | written for real |

So the mod folder is an ordinary MO2 mod with `OBSE/Plugins/` inside and no `Root` folder.

## Configuration

Every setting lives in `OBVR.ini`, each with its reasoning written above it, and the file
is hot reloaded while the game runs. The ones most people touch:

| Key | What it does |
| --- | --- |
| `[Head] Source` | `openvr` for a headset; `simulated` or `fixed` to check the camera chain without one |
| `[Head] RecenterKey` | virtual-key code of the recenter key, `Del` by default |
| `[Render] Stereo` | `dual` for real stereo |
| `[Render] Menus` | `world` keeps menus in front of the live world, `cinema` puts them on a flat screen |
| `[Render] LiveMenuBackground` | render the paused world freshly per eye behind menus |
| `[Render] MirrorMenusToMonitor` | composite in-game menus back onto the monitor window |
| `[Render] Crosshair*` | the crosshair, its depth, size and when it shows |
| `[Look] *` | what happens to the look controls once a headset drives the camera, and the head-based aiming options |

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
throughout.

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
dead end and why it was a dead end.

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

### Tests

The tests are a separate CMake project that runs natively on the development machine.
Every decision the hooks make is lifted into pure functions over plain values so it can be
exercised without a running game: trampoline bytes, rotation maths, the frame decisions,
the INI parser, the bone-lock pairing, the compositor submit policy, the watchdog, and so
on. Forty-one test binaries at the time of writing.

```
cmake -B build-tests tests -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

`docs/verification/` holds logs and screenshots from the game runs that confirmed each
milestone; `tools/` holds the scripts that start the game and take measurements without a
person in the headset.

## Working on it

Two rules shape every change here, and pull requests are read against them:

- **Two sources for every address.** A reverse-engineered address is used only when the
  disassembly of the actual binary and an independent source (xOBSE, NorthernUI, another
  plugin, or a second code path in the binary) agree on it.
- **Cause, not workaround.** A change that makes a symptom go away without an explanation
  of why it appeared has not fixed anything. The commit history is written in that spirit
  and is the first place to look for why something is the way it is.

## License

Not chosen yet. Until a license file is added, all rights are reserved by the author.

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
