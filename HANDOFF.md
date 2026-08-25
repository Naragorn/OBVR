# OBVR — handover document

As of 2026-08-25. This document collects everything that has been worked out, verified and
decided so far. It is written so that the work can be continued on another machine
(Windows in particular) without asking questions first.

---

## 1. What OBVR is

A VR mod for the **original The Elder Scrolls IV: Oblivion from 2006** — explicitly **not**
for the Remastered edition.

Guiding idea: *Oblivion stays Oblivion.* No gameplay rework, but real native VR on top of
the existing camera and render pipeline.

```
Vanilla Oblivion Camera
        ×
relative HMD rotation
        =
final VR camera
```

### First scope (deliberately narrow)

- switching between first and third person stays
- control remains gamepad or keyboard and mouse, **no** VR controllers, **no** hands
- **no** snap turn; horizontal turning stays on the right stick
- vertical look on the gamepad should be disabled if possible
- the HMD determines head rotation (yaw, pitch, optionally roll)
- initially **no** positional head movement, no roomscale → 3DoF
- a recenter function
- gameplay and animations unchanged

### Quality target for stereo rendering (in this order)

1. real dual-pass stereo ← the goal
2. single-pass / multiview ← a later optimisation
3. AER (alternate eye rendering) ← plan B
4. depth reprojection ← last resort

Both eyes belong to the same game frame:

```
UpdateGame()        // once
SetCamera(leftEye)  → RenderWorld()
SetCamera(rightEye) → RenderWorld()
SubmitOpenVR/OpenXR()
```

The big open research question remains: *can Gamebryo render the world twice within the
same game tick, without advancing simulation, physics, particles and animations twice?*
That is not the GPU question — vanilla Oblivion is trivial for a modern GPU — but a
question about the render pipeline.

### Explicitly not in the first scope

Hands, motion controllers, roomscale, 6DoF, IK, physical interaction, weapon aiming by
controller.

---

## 2. Current state

| Version | Content | Status |
| --- | --- | --- |
| 0.0.1 | plugin loads, logging, version check, camera hook, fixed test rotation | **verified in the game** |
| 0.0.2 | quaternion layer, recenter, interchangeable head source, config hot reload | **verified in the game** |
| 0.0.3 | OpenVR wired up, real HMD rotation on the camera | **implemented and covered by tests**, not yet verified in the game ← **continue here** |
| 0.0.4 | frame loop, left and right swapchain, test images in the headset | open |
| 0.1.0 | Oblivion's world as real dual-pass stereo | open |

0.0.1 and 0.0.2 were tested against Oblivion GOTY (Steam, AppID 22330) under Proton with
xOBSE 22.13. Evidence (screenshots and logs) is under `docs/verification/`.

### What 0.0.3 still needs

The maths and the fallback path are covered by tests, but nothing has run against a real
headset yet. Missing:

- a run with SteamVR and an HMD, checking that the camera follows head movement
- a check that recentering on the Del key does what it should with a real headset
- a check of whether the vanity and dialogue cameras interfere. Both run through branches
  of their own

---

## 3. The camera hook — the technical heart

### Hook point: `0x0066BE6E` in Oblivion.exe 1.2.0.416

Oblivion computes the player camera and writes the result into the `CameraNode` of the
scene graph. The hook sits immediately after. The disassembled evidence:

```
0066BE1B  mov  ebx, [esp+0x14]              ; camera object
0066BE1F  cmp  word ptr [ebx+0xB6], 0       ; node list empty?
0066BE27  ja   0066BE2D
0066BE29  xor  eax, eax
0066BE2B  jmp  0066BE35
0066BE2D  mov  edx, [ebx+0xB0]              ; node list
0066BE33  mov  eax, [edx]                   ; eax = CameraNode (NiAVObject*)
0066BE35  mov  ecx, [esp+0x38]
0066BE39  mov  edx, [esp+0x3C]
0066BE3D  mov  [eax+0x54], ecx              ; localTransform.pos.x
0066BE40  mov  ecx, [esp+0x40]
0066BE44  mov  [eax+0x58], edx              ; localTransform.pos.y
0066BE47  mov  [eax+0x5C], ecx              ; localTransform.pos.z
0066BE4A  cmp  word ptr [ebx+0xB6], 0
0066BE52  ja   0066BE58
0066BE54  xor  eax, eax
0066BE56  jmp  0066BE60
0066BE58  mov  edx, [ebx+0xB0]
0066BE5E  mov  eax, [edx]                   ; eax = CameraNode
0066BE60  lea  edi, [eax+0x30]              ; localTransform.rot
0066BE63  mov  ecx, 9
0066BE68  lea  esi, [esp+0x60]
0066BE6C  rep  movsd                        ; 9 DWORDs = NiMatrix33
0066BE6E  <-- OBVR hooks in here
```

From this point on, position and rotation are settled and `eax` still holds the
`CameraNode`.

**Important — `localTransform`, not `worldTransform`:** shortly after the hook the game
calls `NiAVObject::UpdateSelectedDownwardPass` on the same node (`0x00707370`, dispatched
through vtable slot `0x64` = index 25). So the world transform gets recomputed from
`parent * local` regardless. A change to `worldTransform` would be overwritten in the same
frame.

### Verifying the addresses

The Steam `Oblivion.exe` has a `.bind` section and its entry point inside it (SteamStub
DRM), **but the `.text` is not encrypted**. `objdump -d --start-address=... -M intel`
returns correct code directly. Every address in `src/game/GameAddresses.h` was checked
against the binary this way, not adopted from someone else's code.

md5 of the tested exe: `cdd2f0c5eff198d4f26b7b5b54ce4930`

Note that a 4GB-patched exe has a different md5 — the patch flips two bytes in the PE
header (Characteristics `0x0103` → `0x0123`). The code is identical, and OBVR works either
way; see section 5.

### NiAVObject layout (Oblivion, 32 bit)

```
0x00  vtable
0x04  refCount
0x08  name
0x0C  controller
0x10  extraDataList
0x14  extraDataListLen (UInt16)
0x16  extraDataListCapacity (UInt16)
0x18  flags (UInt16)
0x1C  parent
0x20  worldBound (NiBound, 0x10)
0x30  localTransform (NiTransform, 0x34)   ← rot at 0x30, pos at 0x54
0x64  worldTransform (NiTransform, 0x34)
```

Established by the write targets `[eax+0x30]` and `[eax+0x54]` in the code above.

### Other verified addresses

- `0x00B333C4` — pointer to the PlayerCharacter
- `+0x588` — `isThirdPerson` in PlayerCharacter (established by `0x0066C580` ToggleCamera)
- `0x010201A0` — expected Oblivion version 1.2.0.416

### Why no inline assembly

Comparable mods (TES Reloaded) solve mid-function hooks like this with `__declspec(naked)`
and `__asm`. That ties them to MSVC. OBVR generates the trampoline bytes at runtime through
a tiny `CodeWriter` instead. Benefits: it builds with MSVC, clang-cl and clang-cross alike
— and the byte generation is **testable without a running Oblivion**.

The generated trampoline (37 bytes):

```
pushad
pushfd
push dword ptr [esp+0x20]       ; the EAX saved by pushad = CameraNode
call OBVR_OnCameraUpdated
add  esp, 4
popfd
popad
cmp  word ptr [ebx+0xB6], 0     ; original, faithfully reproduced
ja   0x0066BE7C                 ; original
xor  ecx, ecx                   ; original
jmp  0x0066BE84                 ; original
```

`pushad` puts EAX on top, `pushfd` shifts it by four bytes — hence `[esp+0x20]`.

TES Reloaded takes a shortcut here and forces one branch. OBVR rebuilds the original
control flow completely.

The patch at the target address: `E9 <rel32>` plus three `nop`, because the overwritten
original instruction is eight bytes long and no remnant may be left standing.

**Safeguard:** before patching, `mem::Verify` checks that the expected eight bytes
`66 83 BB B6 00 00 00 00` are there. Otherwise the patch is skipped and Oblivion starts
unchanged — a wrong game version must not end in a shredded code segment.

---

## 4. Axes and coordinate systems

### Oblivion / Gamebryo

X = right, Y = forward (view direction), Z = up.

For `EulerToMatrix(x, y, z)` (order Z·Y·X):

| Axis | INI key | Effect | Status |
| --- | --- | --- | --- |
| X | `FixedPitch` | pitch — looking up and down | **confirmed in the game** |
| Y | `FixedRoll` | roll — tilting the head sideways | **confirmed in the game** |
| Z | `FixedYaw` | yaw — looking left and right | follows necessarily, not checked on its own |

Evidence: with roll the horizon sits at an angle; with pitch it stays level and the view
direction tips. The HUD is untouched in both cases — the change really only affects the
camera.

### OpenXR → Oblivion

OpenXR: X = right, Y = up, −Z = forward.

Change of basis:

```
x_obl =  x_xr
y_obl = -z_xr
z_obl =  y_xr
```

Determinant +1, handedness is preserved (a mirroring would be immediately visible in the
headset). For quaternions that means: `(x, y, z, w) → (x, -z, y, w)`.

Cross-check: OpenXR yaw (about Y) becomes Oblivion yaw (about Z) ✓, OpenXR pitch (about X)
stays pitch ✓, OpenXR roll (about Z) becomes roll about −Y ✓.

**OpenVR delivers poses as a 3x4 matrix in the same convention as OpenXR** (Y up, −Z
forward). So the OpenVR backend only has to convert matrix to quaternion and then send it
through the same `FromOpenXR` function.

---

## 5. Works with and without the 4GB patch

Verified on 2026-08-25, and no code change was needed for it.

The 4GB patch (LargeAddressAware) changes exactly **two bytes in the PE header** — the
Characteristics field from `0x0103` to `0x0123`, at file offsets `0x11E` and `0x160`. The
code is untouched: the eight bytes at `0x0066BE6E` are identical in the patched and
unpatched exe, so `mem::Verify` succeeds in both. The version check reads the version
resource, which the patch does not touch either.

The one real risk lies elsewhere. With LargeAddressAware, `VirtualAlloc` may place the
trampoline above 2 GB, and the jump from the hook then spans more than 2 GB. On x86-64 that
would be impossible, because `rel32` is a signed ±2 GB displacement in a 64-bit address
space. On **x86-32 it is not**: the address space is exactly 2³² and the CPU computes
`EIP = EIP_next + rel32` modulo 2³². Every target is reachable from every source; the
distance simply wraps. `CodeWriter` computes the displacements in `UInt32` throughout, so
the overflow is well defined and matches the CPU exactly.

**Warning for later:** switching the rel32 arithmetic in `CodeWriter.cpp` to `int32_t`, or
adding a range check, breaks this — and only on machines with the 4GB patch, meaning for
some users and never for the rest. `TestLargeAddressAware` in `tests/TrampolineTest.cpp`
exists to catch exactly that.

---

## 6. Project structure

```
OBVR/
├── CMakeLists.txt                      DLL build
├── OBVR.ini                            template, belongs next to Oblivion.exe
├── README.md
├── HANDOFF.md                          this document
├── cmake/
│   ├── toolchain-linux-nosdk.cmake     cross build without the Windows SDK
│   └── imports/                        .def files + import library generation
├── docs/verification/                  screenshots and logs from the game tests
├── src/
│   ├── Plugin.cpp                      OBSEPlugin_Query / _Load
│   ├── camera/
│   │   ├── CameraHook.{h,cpp}          callback + hook installation
│   │   └── CameraTrampoline.{h,cpp}    byte generation, platform free, tested
│   ├── core/
│   │   ├── CodeWriter.{h,cpp}          mini assembler
│   │   ├── Config.{h,cpp}              INI + hot reload
│   │   ├── Log.{h,cpp}                 logging without a CRT dependency
│   │   ├── MathFns.h                   sin/cos/sqrt, freestanding capable
│   │   ├── Memory.{h,cpp}              SafeWrite / Verify / AllocExecutable
│   │   ├── Rotation.{h,cpp}            EulerToMatrix (verified in the game)
│   │   └── Types.h
│   ├── game/
│   │   ├── GameAddresses.h             every address with its disassembled evidence
│   │   ├── GameTypes.h                 NiAVObject (32-bit layout)
│   │   └── NiMath.h                    NiPoint3/NiMatrix33/NiTransform
│   ├── obse/PluginInterface.h          binary-compatible xOBSE replica
│   ├── platform/
│   │   ├── Freestanding.cpp            SDK-free build only
│   │   └── Win32Min.h                  narrow Win32 layer
│   └── vr/
│       ├── HeadTracker.{h,cpp}         interchangeable head source
│       ├── OpenVRBackend.{h,cpp}       SteamVR connection, loaded at runtime
│       ├── OpenVRTypes.h               binary-compatible OpenVR replica
│       └── Quaternion.{h,cpp}          quaternion + change of basis
└── tests/
    ├── TrampolineTest.cpp
    ├── QuaternionTest.cpp
    ├── OpenVRPoseTest.cpp
    └── OpenVRBackendTest.cpp
```

### Deliberate design decisions

- **No xOBSE headers.** They drag in the entire game object tree and tie the project to
  MSVC. `src/obse/PluginInterface.h` replicates only the two needed structs in a binary
  compatible way. Oblivion Reloaded does the same.
- **No OpenVR headers either**, for the same reason. `src/vr/OpenVRTypes.h` replicates one
  function, three structs and four constants out of a 3200-line header, each cited with its
  source line.
- **No `windows.h` in the freestanding path.** `Win32Min.h` declares the handful of needed
  imports itself. The DLL only needs `kernel32`, `msvcrt` and a single function out of
  `user32` (`GetAsyncKeyState`, for the recenter key).
- **Maths kept apart from memory layout.** `NiMath.h` contains only float structs and is
  valid on any architecture; `GameTypes.h` with its pointer-bearing structs only checks its
  offsets under `OBVR_TARGET_32BIT`.
- **Everything in the repository is English** — comments, log messages, commit messages and
  documentation. The conversation with the maintainer is not.

---

## 7. Building

### Windows / MSVC — the regular route

Oblivion.exe is 32 bit, so the DLL has to be too.

```
cmake -B build -A Win32
cmake --build build --config Release
```

Result: `build/Release/OBVR.dll`.

The static runtime is preset (`MultiThreaded`) so that no redistributable has to sit next
to Oblivion.

### Linux without the Windows SDK — verification build

Confirms that everything compiles and links cleanly into a 32-bit Windows DLL, without a
Windows machine and without an SDK download. Possible because only `kernel32` and `msvcrt`
are needed and the import libraries are generated from `.def` files with `llvm-dlltool`.

Requires `clang`, `lld`, `llvm`, `cmake`, `ninja`.

```
cmake -B build --toolchain cmake/toolchain-linux-nosdk.cmake -G Ninja
cmake --build build
```

Builds without the C++ standard library and without exceptions. A verification
environment, not a comfortable one.

### Tests (native, on any platform)

```
cmake -B build-tests tests -G Ninja
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

- `trampoline_test` — the generated hook bytes against values worked out by hand,
  including the 4GB-patched case with a trampoline above 2 GB.
- `quaternion_test` — quaternion maths and the change of basis. **The important trick:**
  the cross-check runs against `EulerToMatrix`, which is visually verified in the game. The
  two implementations are deliberately independent (elementary axis matrices against the
  quaternion formula), otherwise the comparison would be worthless.
- `openvr_pose_test` — conversion of an OpenVR pose into a quaternion, all four branches of
  the trace case distinction, and the full chain to the Oblivion camera matrix.
- `openvr_backend_test` — the fallback without SteamVR. Windows only, since it calls
  `LoadLibrary`.

---

## 8. Installing

1. Unpack [xOBSE](https://github.com/llde/xOBSE/releases/latest) (tested: 22.13) and copy
   `obse_1_2_416.dll`, `obse_editor_1_2.dll`, `obse_steam_loader.dll`, `obse_loader.exe`
   and the `Data` folder into the Oblivion directory.
2. `OBVR.dll` into `Data/OBSE/Plugins/`.
3. `OBVR.ini` next to `Oblivion.exe`.
4. For `Source=openvr`, the **x86** build of `openvr_api.dll` next to `Oblivion.exe`.
   SteamVR ships it under `bin/win32/`.
5. Start through the OBSE loader.

`OBVR.log` appears next to `Oblivion.exe`.

### Steam Proton on Linux

xOBSE requires the loader to replace the launcher:

```
cd ~/.local/share/Steam/steamapps/common/Oblivion
cp OblivionLauncher.exe OblivionLauncher.exe.vanilla
cp obse_loader.exe OblivionLauncher.exe
```

Proton-CachyOS additionally ships a protonfix that points `OblivionLauncher.exe` at
`obse_loader.exe` anyway.

Starting without the Steam interface — a plain `proton run` aborts silently, the route has
to go through the Steam Linux Runtime:

```
STEAM_COMPAT_DATA_PATH=~/.local/share/Steam/steamapps/compatdata/22330 \
STEAM_COMPAT_CLIENT_INSTALL_PATH=~/.local/share/Steam \
~/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/_v2-entry-point \
  --verb=waitforexitandrun -- \
  /usr/share/steam/compatibilitytools.d/proton-cachyos-slr/proton waitforexitandrun \
  ~/.local/share/Steam/steamapps/common/Oblivion/OblivionLauncher.exe
```

---

## 9. Configuration (OBVR.ini)

```ini
[Camera]
HookEnabled=1          ; only read at startup

[Head]
Source=fixed           ; none | fixed | simulated | openvr | openxr
RecenterKey=46         ; virtual-key code, decimal or 0x hex; 46 = Del, 0 = off
FixedPitch=0.0
FixedRoll=20.0
FixedYaw=0.0
SimulatedYawDegrees=25.0
SimulatedPitchDegrees=12.0
SimulatedPeriodFrames=600

[Debug]
LogEveryFrames=0       ; 0 = off
ReloadEveryFrames=120  ; hot reload of the [Head]/[Debug] values, 0 = off
```

**The hot reload is essential while developing.** Without it every change to an angle costs
a full restart plus loading a save. With it the camera can be tuned while the game runs —
including switching sources.

`Source=simulated` drives a slow head movement and is the only way to check the whole chain
up to the camera matrix **without a headset**.

Note on the hot reload: `HeadTracker::Configure` only resets the recenter reference when the
source actually changed. It used to reset unconditionally, which would have undone every
recenter within `ReloadEveryFrames` once a real headset was attached.

Recentering sits on `RecenterKey`, default **Del** (`VK_DELETE`, `0x2E`), which vanilla
Oblivion leaves unbound so it cannot collide with a game action. `0` disables it. The value
takes decimal or hex with an `0x` prefix — Microsoft's virtual-key table lists the codes in
hex, so that is the form people copy, and silently ignoring it would look like the key
simply not working.

Polled once per frame with `GetAsyncKeyState`, and the reason is robustness rather than
speed. A `WH_KEYBOARD_LL` hook must be called on a thread that has a message loop, which a
plugin does not own; it sits in the system-wide input path, so every keystroke in every
application waits for it; and Microsoft documents that on Windows 7 and later a hook that
exceeds `LowLevelHooksTimeout` is *silently removed without being called, with no way for
the application to know*. A stuttering game is exactly where that happens. Microsoft steers
towards raw input instead, which would need a window to register against and a message
queue to drain.

This is the only reason OBVR imports anything from `user32`.

---

## 10. VR backend: why OpenVR and not OpenXR

The original roadmap had OpenXR. For a 32-bit game that is the wrong first choice.

### OpenXR and 32 bit

The official Win32 loader exists (in the NuGet package `OpenXR.Loader`, **not** in the ZIP
release). A 32-bit process ends up, through WOW64, at
`HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ActiveRuntime`. The sticking point is not the
loader but whether the runtime registered a 32-bit DLL there:

| Runtime | 32-bit OpenXR |
| --- | --- |
| SteamVR | only from **beta** 2.17.2 (June 2026); stable stood at 2.16 |
| Meta / Oculus | yes |
| VDXR (Virtual Desktop) | yes |
| Pimax | yes |
| Varjo | no |
| WMR | yes, but discontinued by Microsoft |

**For a Bigscreen Beyond that means: OpenXR goes through SteamVR — and 32-bit support there
is still beta.**

### OpenVR

`openvr_api.dll` has always existed for x86 (`bin/win32/` in Valve's repository). Valve
confirms it explicitly (ValveSoftware/openvr issue #1687):

> *"OpenVR supports 32-bit applications. On the other hand, the OpenXR implementation in
> SteamVR does not support them, as opposed to most other vendors OpenXR runtimes."*

Architecturally OpenVR is out-of-process anyway (`vrclient` ↔ `vrserver.exe`) — Valve
already built the 64/32-bit bridge.

### Three details that are easy to get wrong

All three were checked against Valve's header, not taken second-hand, and each one crashes:

- The global `VR_*` exports use `VR_CALLTYPE`, which is **`__cdecl`** (openvr.h line 2299).
  The FnTable function pointers use `OPENVR_FNTABLE_CALLTYPE`, which is **`__stdcall`**
  (openvr_capi.h line 21). Two calling conventions in one interface.
- The interface version is **`IVRSystem_026`**.
- `GetDeviceToAbsoluteTrackingPose` sits at **index 12**. `ComputeDistortionSet` at
  position 4 is missing from older listings, leaving anyone working from one an entry short.

### Precedent

[openRBRVR](https://github.com/Detegr/openRBRVR) — Richard Burns Rally from 2004, also 32
bit and D3D9, essentially the same starting position. It runs **entirely in-process** and
supports OpenVR *and* OpenXR through a backend switch. For rendering it uses a DXVK fork
with VR support (D3D9 → Vulkan), which solves the texture handover problem along the way.

A 64-bit helper process with shared memory (the way
[fear-vr](https://github.com/DR-89/fear-vr) does it for F.E.A.R.) would be heavily oversized
for reading a quaternion and stays the last resort.

### Under Proton

According to Proton's `Makefile.in`, `wineopenxr` is **not built for i386** (the reason
given there: not supported by SteamVR). Under Proton, OpenXR drops out completely.

---

## 11. VR hardware situation on the Linux test system

Tested on 2026-08-25 on CachyOS (kernel 7.2.0), GNOME/Wayland, NVIDIA.

### What works

The **Bigscreen Beyond is fully detected**:

```
USB:  35bd:0101 Bigscreen Beyond
      35bd:0105 Bigscreen Beyond Audio Strap
      28de:2102 Valve VR Radio  (2x)
      28de:2300 Valve Tundra Tracker
HID:  hidraw11 = Bigscreen Beyond   (writable, group wheel)
DRM:  card1-DP-2  status=connected  enabled=disabled
EDID: Display Product Name 'Beyond'
      Primary Use Case: Head-mounted Virtual Reality (VR) display
      Modes: 5088x2544, 3840x1920
```

And **SteamVR detects the HMD**:

```
lighthouse: HMD Model: Bigscreen Beyond
Active HMD set to lighthouse.LHR-58B456BE
Using existing HMD lighthouse.LHR-58B456BE
```

The run on 12 August still said `VRInitError_Init_HmdNotFound` — so lighthouse tracking has
since started working.

### What does not work

- `lighthouse: Unable to query MC Image size` — the Beyond's mura correction data is not
  read (on Windows the Bigscreen driver supplies it). Cosmetic, but a sign of incomplete
  driver support.
- `lighthouse: Enumerating displays... SDL says there are 2 video displays` — SteamVR only
  sees the two monitors, not the Beyond panel.
- **Starting SteamVR crashed the GNOME desktop.** A new login was required.
  GNOME/Wayland + NVIDIA + SteamVR direct mode is a known fragile combination.
- Steam keeps reporting: `Refusing to init SteamVR build 23791826 because it crashed.`

### Assessment

For **0.0.3 (reading head rotation only)** strictly speaking only the tracking is needed,
not the display output — and the tracking works. But as long as merely starting SteamVR
takes the desktop with it, this is not a viable development environment.

**Recommendation: Windows dual boot for everything from 0.0.3 onwards.**

Reasons:
1. SteamVR on Linux crashes the desktop here.
2. The Beyond needs its official driver on Windows anyway (display activation, mura
   correction) — on Linux there are only community workarounds for that.
3. OpenVR 32 bit is proven and well established on Windows.
4. Debugging a hook plugin with MSVC/Visual Studio is considerably more pleasant.

Linux remains useful for everything that does **not** need a headset: writing code, unit
tests, the cross build for verification, and camera tests with `Source=simulated`.

---

## 12. Pitfalls of the test environment (Linux/Proton)

Each of these cost one failed attempt:

- **Oblivion pauses when it loses focus.** The frame counter stands still as soon as the
  window is not active. It looks as though a hot reload did not take.
- **Synthetic input:** `xdotool` (XTEST) is ignored completely by the game. `ydotool`
  (uinput) works — but key presses only arrive when the Oblivion window is **really**
  focused (`xdotool windowactivate --sync`, then verify with `getactivewindow`). Mouse
  movement works without focus, which easily leads to the false conclusion that input is
  arriving.
- **Synthetic clicks do not arrive in the main menu at all**, while they work fine in a
  loaded game. The jump into the game has to be made by hand.
- **`proton run` on its own aborts silently** — the route has to go through
  `SteamLinuxRuntime_4/_v2-entry-point` (see section 8).
- **Steam's launch pipeline** occasionally hangs at `ProcessingShaderCache` waiting for a
  GUI answer.

---

## 13. Next step: verify 0.0.3, then 0.0.4

### Verifying 0.0.3

The code is in place. What is missing is a run with a real headset:

1. Put the x86 `openvr_api.dll` next to `Oblivion.exe`, start SteamVR, set `Source=openvr`.
2. Check that the camera follows head movement, in first and third person, and that the
   character does not turn with it.
3. Check `OBVR.log` for the `OpenVR: connected through FnTable:IVRSystem_026` line.
4. Check the fallback: start once without SteamVR and confirm Oblivion still runs with the
   vanilla camera.
5. Press Del and confirm that the current head pose becomes the new zero.

### Then 0.0.4

Stereo rendering. The architectural decision there is about the *rendering* strategy, not
the VR API:

- **Root fix (model: openRBRVR):** D3D9 → Vulkan through a DXVK fork with VR support. Stays
  in-process and 32 bit, delivers Vulkan textures that OpenVR accepts directly, and solves
  Oblivion's single-threaded D3D9 problems along the way.
- **Pragmatic:** keep native D3D9, bridge to D3D11 via a D3D9Ex helper device and a shared
  handle. Also possible in-process; the price is a CPU readback, because classic D3D9 has
  no shared surfaces.

Note on OpenVR: `IVRCompositor::Submit` only accepts `TextureType_DirectX` (ID3D11),
OpenGL, Vulkan or DirectX12. `TextureType_DXGISharedHandle` is explicitly for overlays
only. So a bridge out of D3D9 is required either way.

---

## 14. References

- [llde/xOBSE](https://github.com/llde/xOBSE) — the plugin loader. `src/obse/PluginInterface.h`
  is a minimal binary-compatible replica of `obse/obse/PluginAPI.h`.
- [mcstfuerson/TES-Reloaded](https://github.com/mcstfuerson/TES-Reloaded) — reverse
  engineering reference. The hook point came from there but was checked against the binary
  before adoption. Particularly relevant: `TESReloaded/Core/CameraMode.cpp` and
  `TESReloaded/Framework/GameNi.h`.
- [openRBRVR](https://github.com/Detegr/openRBRVR) — the closest relative: a 32-bit D3D9
  game with a VR mod, in-process, OpenVR + OpenXR.
- [ValveSoftware/openvr](https://github.com/ValveSoftware/openvr) — `bin/win32/openvr_api.dll`,
  `headers/openvr_capi.h`. The source for `src/vr/OpenVRTypes.h`.
- [DR-89/fear-vr](https://github.com/DR-89/fear-vr) — two-process architecture with shared
  memory, should it ever be needed.
