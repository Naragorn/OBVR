# OBVR

VR for the original **The Elder Scrolls IV: Oblivion (2006)** — explicitly not for the
Remastered edition.

Guiding idea: *Oblivion stays Oblivion.* OBVR does not replace gameplay; it puts real
native VR on top of the existing camera and render pipeline.

```
Vanilla Oblivion Camera
        ×
relative HMD rotation
        =
final VR camera
```

## Status: 0.0.3 — head rotation from a real headset

Still no stereo. 0.0.1 answered the core question:

> Can an additional rotation be applied after Oblivion's vanilla camera calculation,
> without damaging first person, third person or animations?

Yes — confirmed in the running game, see below.

0.0.2 replaced the fixed test angle with an interchangeable source (`vr::HeadTracker`). The
rotation arrives as a quaternion, is worked out against a recenter reference and turned
from OpenXR into Oblivion convention. Which source supplies the quaternion is
configuration: a fixed angle, a simulated head movement, or a real headset.

0.0.3 adds that real headset. `OpenVRBackend` loads `openvr_api.dll` at runtime, registers
OBVR with SteamVR as a background application and reads the HMD pose. The result is 3DoF:
the head rotates, but it does not move through space — that is deliberate for this
milestone.

This has been **confirmed in the running game** with SteamVR and a real headset. The camera
follows head movement in first and third person, the character does not turn along with it,
and `Del` recenters. The log of that run is
`docs/verification/OBVR-openvr-headtracking.log`.

0.0.4 turns the 3DoF into 6DoF **for the head**: leaning forward, sideways or standing up
moves the camera with you. Locomotion stays entirely with the game - this is head movement
within arm's reach, not room-scale walking.

Two things make it usable rather than merely correct. The offset is measured against a
reference position captured on the first valid pose, so the roughly 1.2 m between the
seated floor origin and a head does not displace the camera permanently. And it is capped
at `MaxLeanUnits`, because a tracking glitch or someone standing up and walking off would
otherwise drag the camera through the nearest wall.

The head is deliberately **never** smoothed. Easing a tracked head shows the wearer where
their head was rather than where it is, and that latency is felt directly in a headset —
it is the one thing VR cannot trade away.

How far the camera moves is `HeadMovementScale`, and it defaults to 1.7 rather than to
life-size — a figure a headset settled on, not one chosen in advance. That is a deliberate compromise with a shelf life: above 1.0 the world moves
further than the head that moved it, which is the very mismatch VR comfort rests on
avoiding. It is there because OBVR still renders a single image, so parallax is the only
depth cue available and a one-to-one lean reads weaker than it will once there are two
eyes. It is kept separate from `UnitsPerMetre` on purpose — that number is the engine's
documented figure and stereo will need it for the distance between the eyes, so inflating
it to taste now would silently shrink the whole world later.

0.0.4 also takes the vertical look away from the stick, the mouse and the keyboard, and
this is where easing does belong. With a headset on, the head already decides where the
camera points; leaving the stick pointed at the same thing gives two answers to one
question, and the artificial one is what makes people ill — tilting a view that the inner
ear insists is level is the classic trigger. So in third person the vertical look moves the
camera up and down instead of tilting it, up above the character's head and down towards
the feet, and in first person it does nothing at all. Turning left and right stays with the
player, because there is no other way to face something behind you; it can be eased as
well, and that is off by default because easing the control still used for aiming puts the
camera behind where you asked it to be.

The easing speeds are per second rather than per frame, which is how UEVR computes its
camera lerp too — `t = m_lerp_camera_speed->value() * delta`. A per-frame share would make
the same setting feel twice as sluggish at 30 fps as at 60.

Up and down get separate ranges, `VerticalLookUpRange` and `VerticalLookDownRange`, because
the camera does not start halfway along its travel. It sits at head height, so downwards
there is exactly one body between it and the ground — a limit the character model sets
rather than a matter of taste — while upwards nothing bounds it at all. A single symmetric
range has to be wrong at one end, and in the game it was: sized for looking up, it stopped
around the character's hips going down.

Worth being clear about what this is not. Luke Ross's R.E.A.L. mod, which the shape of this
feature was taken from, does not do it this way at all — it *orbits* the camera around the
character ("decoupled the camera angles from the position, so that the player can look
around with the headset and separately orbit the camera around the in-game character using
the mouse/controller",
[README](https://github.com/LukeRoss00/gta5-real-mod/blob/master/README.md)), and its
published hotkey list contains no vertical camera offset at all. OBVR translates the camera
instead. Whether the newer, Patreon-only R.E.A.L. framework added such a setting **could not
be verified** — those builds are not public.

None of that needs Oblivion's input code. The hook already runs after the camera has been
computed, so whatever the stick did is sitting in the camera matrix and can be read back
out and replaced — one less address to keep correct across game versions. Everything in
`[Look]` applies only while a headset is actually delivering poses: start Oblivion without
SteamVR and the game is exactly the one you had before.

Recentering sits on the **Del** key by default. It takes the current head pose as the new
zero, so you can settle into a comfortable position and make that the forward direction.
Vanilla Oblivion does not bind Del, so it cannot collide with a game action. The key is
`RecenterKey` in the INI and takes any Windows virtual-key code, in decimal or in hex with
an `0x` prefix; `0` turns recentering off.

The simulated head is not a toy. It is the only way to check the whole chain up to the
camera matrix without an HMD, and on a Linux system without a working VR stack it is the
only way at all.

## How the hook works

Oblivion.exe 1.2.0.416 computes the player camera and writes the result into the
`CameraNode` of the scene graph. The hook sits immediately after that, at `0x0066BE6E`.

The relevant code in the binary:

```
0066BE33  mov  eax, [edx]              ; eax = CameraNode (NiAVObject*)
0066BE3D  mov  [eax+0x54], ecx         ; localTransform.pos.x
0066BE44  mov  [eax+0x58], edx         ; localTransform.pos.y
0066BE47  mov  [eax+0x5C], ecx         ; localTransform.pos.z
...
0066BE60  lea  edi, [eax+0x30]         ; localTransform.rot
0066BE63  mov  ecx, 9
0066BE68  lea  esi, [esp+0x60]
0066BE6C  rep  movsd                   ; 9 DWORDs = NiMatrix33
0066BE6E  <-- OBVR hooks in here
```

From this point the position and rotation are settled and `eax` still holds the
`CameraNode`. The write targets `[eax+0x30]` and `[eax+0x54]` also establish the
`NiAVObject` layout (`localTransform` at `0x30`, with `pos` at `+0x24`).

Shortly afterwards the game calls `NiAVObject::UpdateSelectedDownwardPass` on the same node
(`0x00707370`, vtable slot `0x64` = index 25). That is why OBVR modifies `localTransform`
and not `worldTransform`: the world transform gets recomputed from `parent * local`
regardless.

Every address in `src/game/GameAddresses.h` was disassembled from the actual binary and is
documented there with the evidence.

### Why no inline assembly

Comparable mods solve mid-function hooks like this with `__declspec(naked)` and an `__asm`
block. That ties the project to MSVC. OBVR generates the trampoline bytes at runtime
through a tiny `CodeWriter` instead. That builds with MSVC, clang-cl and clang-cross alike
— and it can be tested without a running Oblivion.

The trampoline saves all registers, calls OBVR and then executes the overwritten
instruction together with its original control flow:

```
pushad
pushfd
push dword ptr [esp+0x20]       ; the EAX saved by pushad = CameraNode
call OBVR_OnCameraUpdated
add  esp, 4
popfd
popad
cmp  word ptr [ebx+0xB6], 0     ; original
ja   0x0066BE7C                 ; original
xor  ecx, ecx                   ; original
jmp  0x0066BE84                 ; original
```

Before patching, OBVR checks that the expected eight bytes really are at the target
address. If they are not — a different game version, or another mod got there first — no
patch happens and Oblivion starts unchanged.

### Works with and without the 4GB patch

The 4GB patch (LargeAddressAware) changes two bytes in the PE header of Oblivion.exe, not
the code, so the hook site is unaffected. What does change is that `VirtualAlloc` may place
the trampoline above 2 GB, and the jump from the hook then spans more than 2 GB.

On x86-64 that would be impossible, because `rel32` is a signed ±2 GB displacement in a
64-bit address space. On x86-32 the address space is exactly 2³² and the CPU computes
`EIP = EIP_next + rel32` modulo 2³² — every target is reachable from every source, the
distance simply wraps. `CodeWriter` computes the displacements in `UInt32` throughout, so
the wraparound is well defined and matches the CPU. `TestLargeAddressAware` in
`tests/TrampolineTest.cpp` pins that down.

## The OpenVR backend

`openvr_api.dll` is loaded at runtime rather than linked. That is not a matter of taste:
OBVR has to load on machines without SteamVR, otherwise starting Oblivion would break for
every user who does not own a headset. If any step fails, the vanilla camera stays and the
reason goes into the log.

`src/vr/OpenVRTypes.h` replicates the small part of the OpenVR C interface that OBVR needs
— one function, three structs, four constants out of a 3200-line header — the same way
`src/obse/PluginInterface.h` does for xOBSE. Every value is cited with its line in Valve's
`headers/openvr_capi.h`, because three details there are easy to get wrong and each one
crashes:

- The global `VR_*` exports are `__cdecl` (`VR_CALLTYPE`), while the FnTable function
  pointers are `__stdcall` (`OPENVR_FNTABLE_CALLTYPE`). Two different calling conventions
  in one interface; confusing them pops the wrong number of bytes off a 32-bit stack.
- The interface version is `IVRSystem_026`.
- `GetDeviceToAbsoluteTrackingPose` sits at index 12. `ComputeDistortionSet` at position 4
  is missing from older listings, which leaves anyone working from one an entry short.

`VR_InitInternal` is called with `VRApplication_Background` rather than `Scene`: OBVR only
reads poses and must not take the scene away from the compositor.

## Building

### Regular: MSVC on Windows

Oblivion.exe is 32 bit, so the DLL has to be as well.

```
cmake -B build -A Win32
cmake --build build --config Release
```

With a single-config generator such as Ninja the build type has to be named at configure
time instead, otherwise the DLL comes out unoptimised and roughly seven times its size:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

### Verification on Linux, without the Windows SDK

Confirms that everything compiles and links cleanly into a 32-bit Windows DLL, without a
Windows machine and without several gigabytes of SDK. That is possible because OBVR needs
nothing beyond `kernel32` and `msvcrt`, and the import libraries are generated with
`llvm-dlltool`.

```
cmake -B build --toolchain cmake/toolchain-linux-nosdk.cmake -G Ninja
cmake --build build
```

This route builds without the C++ standard library and without exceptions. It is a
verification environment, not a comfortable one — for a release MSVC stays the way.

### Tests

Eleven test binaries, all without a running Oblivion:

- **`trampoline_test`** checks the generated hook bytes against expected values worked out
  by hand. A mistake there reliably crashes Oblivion. It also covers the 4GB-patched case
  with a trampoline above 2 GB.
- **`quaternion_test`** checks the quaternion maths and the change of basis from OpenXR to
  Oblivion. The decisive part is the cross-check against `EulerToMatrix`: that function is
  verified in the running game, so the quaternion route is tied to established evidence
  rather than only checked against itself. The two implementations are deliberately
  independent — elementary axis matrices against the quaternion formula.
- **`rotation_test`** checks `EulerToMatrix` itself. It is the reference every other
  rotation test compares against, which is exactly why it cannot be checked by comparison —
  a silent sign flip there would leave the whole suite green while agreeing with a wrong
  answer. So it is checked against its own definition: the documented `Z * Y * X`
  composition order, the sign convention of each single axis written out, and the
  properties every rotation matrix has (determinant +1, orthonormal rows).
- **`openvr_pose_test`** checks the conversion of an OpenVR pose into a quaternion,
  including all four branches of the trace case distinction and the full chain from an HMD
  pose to the Oblivion camera matrix.
- **`openvr_backend_test`** checks that OBVR falls back cleanly on a machine without
  SteamVR. That is the case most users meet first. Windows only, since it calls
  `LoadLibrary`.
- **`config_test`** checks the INI parsing, above all the virtual-key code parser: both
  notations, and every way of writing nonsense that has to leave the previous setting
  alone. A fault there does not crash anything — the key simply stops working, which is the
  hardest kind of fault to attribute. Windows only, since Config reads through
  `GetPrivateProfileString`. It also covers the older `VerticalLookRange` spelling, which is
  still honoured as the fallback for both halves of the split setting — an INI that quietly
  stops applying is worse than one that fails loudly.
- **`head_tracker_test`** checks the layer that turns a head orientation into the camera
  matrix: each source, what recentering means, and that a hot reload with unchanged
  settings does not undo a recenter. That last one is a regression guard — `Configure` used
  to reset the reference on every call, which with a real headset would have thrown the
  zero away every couple of seconds. Windows only.
- **`plugin_path_test`** checks where OBVR looks for its own files, including the buffer
  being too small — a path is one of the few things in OBVR whose length is not under its
  own control. Windows only.
- **`look_control_test`** checks what happens to the look controls once a headset takes
  over. All of it is about comfort rather than correctness, which is a bad reason to leave
  it untested: a fault there does not crash anything and does not look wrong in a
  screenshot. It is felt half an hour later by somebody who then puts the mod down and
  cannot say why. Since the ranges were split it also pins down the seam between them:
  level has to come out at zero whichever range claims it, or the camera would jump as the
  stick crossed the middle.
- **`head_offset_test`** checks the arithmetic of positional tracking, which is where the
  mistakes live: a wrong sign in the change of basis makes leaning forward pull the camera
  backwards, and a missing rotation by the reference orientation makes leaning work only if
  the seated zero in SteamVR happens to face the same way as the player - a fault that is
  invisible in a play session that starts out facing the right way.
- **`frame_logic_test`** checks the per-frame decisions of the camera hook. The callback
  itself cannot be tested — it needs a live `CameraNode` — so the decisions were lifted out
  into `camera/FrameLogic`: the recenter key edge, the point-of-view transition, and whether
  a periodic action is due this frame. The edge is the important one. Without it a held key
  would recenter on every frame, taking the current pose as the new zero sixty times a
  second, and the symptom — a camera that seems frozen — points nowhere near the cause.

```
cmake -B build-tests tests -G Ninja
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Installing

1. Unpack [xOBSE](https://github.com/llde/xOBSE/releases/latest) and copy
   `obse_1_2_416.dll`, `obse_editor_1_2.dll`, `obse_steam_loader.dll`, `obse_loader.exe`
   and the `Data` folder into the Oblivion directory (tested with 22.13).
2. Copy `OBVR.dll` into `Data/OBSE/Plugins/`.
3. Put `OBVR.ini` beside it, in `Data/OBSE/Plugins/`. Next to `Oblivion.exe` also works and
   is what older installations use.
4. For `Source=openvr`, put the **x86** build of `openvr_api.dll` in `Data/OBSE/Plugins/`
   as well, or next to `Oblivion.exe`. SteamVR ships it under `bin/win32/`; the x64 build
   that comes with most games will not load into Oblivion, which is a 32-bit process.
5. Start Oblivion through the OBSE loader.

`OBVR.log` appears next to `Oblivion.exe`.

### Mod Organizer 2

OBVR is an ordinary MO2 mod — everything of its own lives under `Data`, which is the only
folder MO2 virtualises:

```
OBVR/
└── OBSE/
    └── Plugins/
        ├── OBVR.dll
        ├── OBVR.ini
        └── openvr_api.dll
```

No `Root` folder and no [Root Builder](https://kezyma.github.io/?p=rootbuilder) needed for
OBVR itself. xOBSE still needs Root Builder, because a script extender has to put files in
the game root and MO2 cannot virtualise those — USVFS is installed after the load-time DLLs,
so the game would not see them in time. See HANDOFF.md for the details.

### Steam Proton on Linux

For Proton, xOBSE requires the loader to replace the launcher:

```
cd ~/.local/share/Steam/steamapps/common/Oblivion
cp OblivionLauncher.exe OblivionLauncher.exe.vanilla
cp obse_loader.exe OblivionLauncher.exe
```

Then start normally through Steam. Proton-CachyOS ships a protonfix of its own that points
`OblivionLauncher.exe` at `obse_loader.exe` anyway — both routes work.

To start without the Steam interface the route has to go through the Steam Linux Runtime;
a plain `proton run` aborts silently:

```
STEAM_COMPAT_DATA_PATH=~/.local/share/Steam/steamapps/compatdata/22330 \
STEAM_COMPAT_CLIENT_INSTALL_PATH=~/.local/share/Steam \
~/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/_v2-entry-point \
  --verb=waitforexitandrun -- \
  /usr/share/steam/compatibilitytools.d/proton-cachyos-slr/proton waitforexitandrun \
  ~/.local/share/Steam/steamapps/common/Oblivion/OblivionLauncher.exe
```

## Verification in the game

0.0.1 was tested against Oblivion GOTY (Steam, AppID 22330) under Proton with xOBSE 22.13.
Result:

| Check | Result |
| --- | --- |
| xOBSE loads the DLL | `plugin OBVR.dll (00000003 OBVR 00000001) loaded correctly` |
| Game version | OBSE reports `010201A0` = 1.2.0.416 |
| Byte comparison at `0x0066BE6E` | passed, hook installed |
| Trampoline | 37 bytes, as predicted by the test |
| Callback fires | yes, once per frame |
| Third person | rotation visibly applied |
| First person | rotation visibly applied |
| POV switch | detected and logged in both directions |
| Stability | no crash |

That answers the core question of 0.0.1: Oblivion's vanilla camera calculation can be
extended by an additional rotation, in both camera modes, without touching gameplay or
animations.

Excerpt from `OBVR.log`:

```
OBSE version 22, Oblivion version 010201A0
Config: HookEnabled=1 Source=fixed Fixed=(P 0.0, R 20.0, Y 0.0)
Camera: hook installed at 0066BE6E, trampoline at 024C0000 (37 bytes)
OBVR ready
Camera: first hook pass, CameraNode=1812BC40, third person
Camera: switched to first person (frame 1621)
Camera: switched to third person (frame 1745)
```

### 0.0.2 in the game

The simulated head and the hot reload were checked in the same setup.

| Check | Result |
| --- | --- |
| Simulated head movement | camera pans visibly, the character does not turn with it |
| Quaternion values in the log | match an independent calculation to three decimal places |
| Camera position | stays constant — pure rotation, no positional offset |
| Compass | keeps pointing the same way, so player orientation is untouched |
| Hot reload | source switched `simulated` → `fixed` while running, without a restart |

The logged quaternions can be recomputed directly. With `SimulatedPeriodFrames=600`,
`SimulatedYawDegrees=25`, `SimulatedPitchDegrees=12`:

```
Frame  540  Log (-0.099, -0.127, -0.013, 0.987)   Computed (-0.099, -0.127, -0.013, 0.987)
Frame  720  Log ( 0.060,  0.206, -0.013, 0.977)   Computed ( 0.060,  0.206, -0.013, 0.977)
Frame  900  Log ( 0.000, -0.000,  0.000, 1.000)   Computed (-0.000,  0.000,  0.000, 1.000)
```

Frame 900 lands at phase π, where both sine terms are zero — the identity is the expected
result there, not a dropout.

After switching to `Source=fixed` with `FixedRoll=35` the quaternion sits constantly at
`(0.000, 0.000, -0.301, 0.954)`. That is exactly a 35° rotation: sin(−17.5°) = −0.3007,
cos(17.5°) = 0.9537.

Evidence is under `docs/verification/`.

Two observations about the test environment: Oblivion pauses as soon as its window loses
focus — the frame counters stand still then. And synthetic clicks do not arrive in the main
menu, while they do in a loaded game.

### 0.0.3 in the game

Not yet verified in the game. The maths and the fallback path are covered by tests; what is
still missing is a run with a real headset.

### Axis assignment

The local camera space follows the Gamebryo convention: X to the right, Y along the view
direction, Z up. For `EulerToMatrix(x, y, z)` that means:

| Axis | INI key | Effect | Status |
| --- | --- | --- | --- |
| X | `FixedPitch` | pitch — looking up and down | confirmed in the game |
| Y | `FixedRoll` | roll — tilting the head sideways | confirmed in the game |
| Z | `FixedYaw` | yaw — looking left and right | follows necessarily from the other two, not checked on its own |

In the roll test the horizon sits at an angle; in the pitch test it stays level and the
view direction tips down. The HUD is untouched in both cases, which confirms that the
change really only affects the camera.

Still open: whether the vanity and dialogue cameras interfere. Both run through branches of
their own.

## VR backend: why OpenVR comes first

The roadmap originally had OpenXR as the only binding. Research for 0.0.2 corrected that,
and the reason is bitness.

Oblivion.exe is 32 bit, so OBVR.dll is too. OpenXR's 32-bit support is patchy to this day:

| Runtime | 32-bit OpenXR |
| --- | --- |
| SteamVR | only from beta 2.17.2 (June 2026); stable stands at 2.16 |
| Meta / Oculus | yes |
| VDXR (Virtual Desktop) | yes |
| Pimax | yes |
| Varjo | no |
| WMR | yes, but discontinued by Microsoft |

The official Win32 loader exists (in the NuGet package `OpenXR.Loader`, not in the ZIP),
but it is no use if the runtime has not registered a 32-bit DLL.

**OpenVR does not have this problem.** `openvr_api.dll` has always shipped for x86, and
Valve says so explicitly: *"OpenVR supports 32-bit applications. On the other hand, the
OpenXR implementation in SteamVR does not support them."* Architecturally OpenVR is
out-of-process anyway (`vrclient` against `vrserver.exe`) — Valve already built the bridge
between 64 and 32 bit there. Through SteamVR, a single OpenVR backend reaches more headsets
than all 32-bit OpenXR runtimes combined.

The closest precedent is [openRBRVR](https://github.com/Detegr/openRBRVR): Richard Burns
Rally from 2004, also 32 bit and D3D9. It runs entirely in-process and supports OpenVR
*and* OpenXR through a backend switch.

Which is why `TrackerSource` is deliberately an enumeration of interchangeable sources.
OpenVR came first, OpenXR is the second backend. A 64-bit helper process with shared memory
(the way [fear-vr](https://github.com/DR-89/fear-vr) does it for F.E.A.R.) would be
oversized for reading a quaternion and stays the last resort, should some runtime later
turn out to be reachable no other way.

**Under Proton** OpenXR drops out entirely: according to Proton's `Makefile.in`,
`wineopenxr` is not built for i386. Only OpenVR leads anywhere there.

## Roadmap

| Version | Content | Status |
| --- | --- | --- |
| 0.0.1 | plugin loads, logging, version check, camera hook, fixed test rotation | verified in the game |
| 0.0.2 | quaternion layer, recenter, interchangeable head source, config hot reload | verified in the game |
| 0.0.3 | OpenVR wired up, real HMD rotation on the camera — still a monitor image | implemented, not yet verified in the game |
| 0.0.4 | frame loop, left and right swapchain, test images in the headset | open |
| 0.1.0 | Oblivion's world as real dual-pass stereo, both eyes in the same game frame | open |

Quality target for the stereo rendering, in this order:

1. Real dual-pass stereo ← the goal
2. Single-pass / multiview ← a later optimisation
3. AER ← plan B
4. Depth reprojection ← last resort

The real crux is not GPU performance but whether Gamebryo can render the world twice within
the same game tick, without advancing simulation, physics, particles and animations twice.

Not in the first scope: hands, motion controllers, roomscale, 6DoF, IK, physical
interaction.

## References

- [llde/xOBSE](https://github.com/llde/xOBSE) — the plugin loader; the plugin interface in
  `src/obse/PluginInterface.h` is a minimal, binary-compatible replica of
  `obse/obse/PluginAPI.h`.
- [mcstfuerson/TES-Reloaded](https://github.com/mcstfuerson/TES-Reloaded) — reverse
  engineering reference. The hook point came from there, but it was checked against the
  binary before being adopted.
- [ValveSoftware/openvr](https://github.com/ValveSoftware/openvr) — `bin/win32/openvr_api.dll`
  and `headers/openvr_capi.h`, the source for `src/vr/OpenVRTypes.h`.
- [openRBRVR](https://github.com/Detegr/openRBRVR) — the closest relative: a 32-bit D3D9
  game with a VR mod, in-process, OpenVR and OpenXR.
