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
| 0.0.3 | OpenVR wired up, real HMD rotation on the camera | **verified in the game** |
| 0.0.4 | 6DoF for the head, vertical look taken off the stick | **verified in the game**, two settings retuned from what it showed |
| 0.0.5 | frame loop, left and right swapchain, test images in the headset | **verified in the headset**, every question the pattern was built to ask has been answered |
| 0.1.0 | Oblivion's world as real dual-pass stereo | **the picture arrives, mono**: the game's own frame reaches both eyes through DXVK, aligned; depth between the eyes is what remains ← **continue here** |

0.0.1 and 0.0.2 were tested against Oblivion GOTY (Steam, AppID 22330) under Proton with
xOBSE 22.13. 0.0.3 was tested on Windows 11 with SteamVR and a real headset, against a
4GB-patched `Oblivion.exe` of the same version. Evidence (screenshots and logs) is under
`docs/verification/`.

### What the 0.0.5 run showed, and what it did not

**OBVR put a picture in a headset.** `docs/verification/OBVR-openvr-first-frames.log` is the
record — 4320 frames of it. What the wearer saw is testimony; what follows is the log.

Confirmed: the scene registration held, a Direct3D 11 device and two textures were created,
`Submit` accepted them, and the frame loop ran without `SubmitPolicy` shutting it down. The
red square appeared to the left eye and the green one to the right — **the eyes are not
swapped**, and since each marker sits on its own side, **the picture is not mirrored**. The
ramp ran dark at the top to bright at the bottom, so **it is the right way up**.

That settles the question the milestone was built to ask, and it settles it independently of
Direct3D 9: whatever comes next, the way OBVR talks to the compositor is right.

**The border at the extreme edge is hard to see, and I overstated that.** The first report
was that the top corners were visible and the bottom was uncertain - the frame sits so far
out that a face gasket and the lens edge make it awkward to look at. I turned that
uncertainty into a finding and wrote here that the optics do not reach the edges of a
render target. A later run with the wearer looking deliberately found all four sides, so
that claim was wrong: the outer border works, it just needs looking for.

The inset cyan frame added in response is still worth having, but for a weaker reason than
the one recorded: it is *easier* to see, not the only one visible, and being further in it
also says something about rescaling that the outer one does not. Both earn their place. The
lesson that does survive is about evidence rather than optics - "I could not see it" and
"it is not visible" are different claims, and only one of them was made.

What the log settles on its own:

- **the compositor version was guessed right.** `OpenVR: connected as a scene application
  through FnTable:IVRSystem_026 and FnTable:IVRCompositor_029`. `IVRCompositor_029` was
  derived from the header declaring `IVRSystem_026` twelve lines earlier, and the runtime
  accepts it.
- **the headset asks for 3560 × 3560 per eye.** That is 48.3 MB a texture, 96.6 MB for the
  pair — comfortably inside the 256 MB per-eye refusal in `PatternBufferBytes`, which is
  therefore neither too tight nor decorative.
- **a hardware device, not WARP.** No fallback line. Worth knowing before 0.1.0, because a
  software rasteriser would have changed what is worth attempting there.
- **`SubmitPolicy` never fired.** No `stopped rendering` line in 4320 frames; the compositor
  took every one.
- **the lean limit has never once cut anything.** In every line `raw=` equals the magnitude
  of `lean=`, and the largest across the session is 12.1 units — 10 cm of head movement
  against a limit of 80. This is what the `raw=` figure was added for, and it earned itself
  on first use: it removes one of the three candidates under "leaning forward does not feel
  like leaning sideways" by measurement rather than by argument.

**Frame times are 17.8 to 18.5 ms across the whole run**, with no outlier — about 55 fps and
no stutter, and nowhere near the 100 ms that a 10 Hz throttle would show.

**And they are Oblivion's own, not the compositor's.** `docs/verification/OBVR-framerate-baseline.log`
is the same session with `Render.Enabled=0`, and it runs at 17.8 to 18.4 ms — the same
distribution. So submitting costs nothing measurable, and `WaitGetPoses` never gated the
game: at 55 fps Oblivion is already slower than the headset wants, so the call returns
immediately and the compositor reprojects. Worth recording that the earlier guess went the
other way — the consistency of the figures looked like pacing, and it was not.

Also visible: the tester's zero sits about 5 cm forward of where they actually sit. The Y
component of `lean=` is persistently positive while X straddles zero, which is a reference
captured while leaning back rather than a fault. A recenter fixes it and returns the lean
range symmetrically.

**The eye geometry is confirmed, by the eyes.** With the centring cross moved from the
middle of the image to the measured optical axis — `u` 0.583 for the left eye, 0.424 for the
right — the two crosses **fuse into one** when the wearer looks straight ahead. That is the
check the cross was put there for, and it is a stronger answer than the numbers alone: it
says the frustum is being read correctly, the eyes are the right way round, and the offsets
are usable, all from one look.

So every question the pattern was built to ask now has an answer: the whole texture arrives
(all four edges of the outer border, plus the inset frame), it is the right way up, the eyes
are not swapped, it is not mirrored, and the optical axes are where the headset says. 0.0.5
is done.

What remains unknown is deliberately so: the sign convention of `GetProjectionRaw`'s top and
bottom cannot be settled from the numbers, and does not need to be — 0.1.0 takes a ready made
matrix from `GetProjectionMatrix` at index 1 instead, and `GetProjectionRaw` is documented as
being for "something fancy like infinite Z", which OBVR is not doing.

### What the 0.0.4 run showed, and what it did not

Weaker evidence than 0.0.3, and it should be read that way: **no log was kept**. What
exists is the tester's report, so what follows is testimony rather than a record. It is
recorded here because it drove two changes to the defaults, and a reader deserves to know
on what basis.

Confirmed by that report: leaning moves the camera the right way; the vertical look does
move the third person camera up and down rather than tilting it, in the direction intended;
in first person the vertical look does nothing; the camera no longer tilts; and easing the
turn works but is unwelcome, so `SmoothTurning` stays off.

Two things it found wrong, both now changed:

- **the lean was far too small** at one to one. Hence `HeadMovementScale`, which went
  1.0 → 2.0 → 1.7 → **2.0** across three sessions in the headset. That path is the useful
  part: a number that wanders and comes back is a preference, not a measurement, and
  preferences do not survive a change to the thing they were formed against. The honest
  reading is that it compensates for a missing depth cue rather than fixing a bug — OBVR
  still renders one image, so parallax is doing work stereo will take over, and 1.0 is worth
  trying again once there are two eyes.
- **one vertical range could not fit both directions.** At 60 units the camera reached
  roughly the character's backside going down, while the same 60 was right going up. The
  camera starts at head height rather than halfway along its travel, so the two directions
  are asymmetric by geometry. Split into `VerticalLookUpRange` and `VerticalLookDownRange`.

Still not established: whether the vanity and dialogue cameras interfere.

**The lean limit is settled, and it was not the culprit.** The 0.0.5 log has `raw=` equal to
the magnitude of `lean=` on every line, with a session maximum of 12.1 units against a limit
of 80 — so `MaxLeanUnits` has never cut anything at all. That was the reason the unclamped
figure was added to the log, and it answered on first use.

#### Open: leaning forward does not feel like leaning sideways

Reported from the headset, deliberately not acted on yet. Recorded here rather than fixed
because the cause is not known and three candidates would each call for a different fix.

**Not the arithmetic.** `head_offset_test`'s `TestOffsetAxes` puts one metre along each of
the four directions through `OffsetFromPose` with the same conversion and checks the result
per axis, so a metre forward and a metre sideways demonstrably come out the same size.
Whatever this is, it is not the change of basis.

**The best explanation so far, and it came out of the log rather than out of reasoning.**
`docs/verification/OBVR-firstperson-lean.log` is a first person session with deliberate
leaning in both directions, and the vertical component tells a clear story. Leaning forward:

```
lean=(5.5, 33.5, -14.4)   lean=(8.0, 26.7, -8.1)
lean=(0.4, 22.7,  -8.2)   lean=(1.0, 18.6, -7.1)      Z/Y ~ -0.35
```

Leaning sideways:

```
lean=(30.6, -8.9, -3.0)   lean=(-29.2, -0.9,  0.7)
lean=(29.1, -1.0, -2.4)   lean=( 30.7, -1.8, -3.3)    Z/X ~ -0.06
```

**Leaning forward lowers the camera about six times as much as leaning sideways does.** That
is not a fault, it is a body: leaning forward pivots at the hips and the head travels an arc
that is forward *and* down, while leaning sideways is closer to a shift than a tilt.

So a forward lean is, to something like a third, a downward movement — and since motion along
the view axis produces almost no parallax anyway, what is left to perceive is mostly a change
in height where a change in distance was expected. That would account for the two feeling
unlike each other without anything being wrong.

**Resolved at a scale of 3.0**, and how it resolved is the confirmation. Raising the gain
from 2 to 3 amplifies both directions equally - the coupling ratio above is unchanged - and
the tester reports the two now feel alike. So the asymmetry was never unequal treatment in
the code: forward motion sat below the threshold of perception while sideways sat above it,
and more gain lifted both above it.

Which sharpens the prediction rather than ending it. Stereo supplies the depth cue that is
missing, so after 0.1.0 markedly less gain should be needed - and if leaning still feels
symmetric at a scale of 1, the explanation is confirmed end to end. If it does not, the
vertical coupling deserves a damping setting of its own after all.

The candidates it was weighed against, in the order they are worth checking:

1. **Geometry, and expected.** Motion along the view axis produces almost no parallax:
   distant things barely shift, they only scale. Sideways motion shifts everything
   laterally at rates that differ with distance, which is the strong cue. Forward and back
   therefore genuinely carry less information than left and right, and with one eye there
   is nothing else to carry it. If this is the cause it should soften on its own in 0.0.5.
2. **Third person specifically.** The offset is carried into world space by the camera's
   own rotation, and in third person the camera faces the character's back. Leaning forward
   therefore moves the camera *towards* the character, so the dominant change on screen is
   the character growing rather than the world shifting - while leaning sideways slides
   around them. Worth checking whether the effect is weaker or absent in first person; that
   single observation separates this candidate from the one above.
3. **The levelled rotation.** With `BlockVerticalLook` on, the rotation carrying the offset
   has had its pitch removed, so "forward" for a lean is horizontal while the camera's
   actual view axis in third person is not. Deliberate, but it does mean the lean and the
   view no longer share an axis.

How to tell them apart without guessing: with `LogEveryFrames` on, lean the same distance
forward and then sideways and compare the `lean=` triples. Equal magnitudes point at 1 or
3; unequal ones would mean something upstream of the arithmetic, which would be a genuine
finding since the arithmetic itself is covered.

### What the 0.0.3 run proved

`docs/verification/OBVR-openvr-headtracking.log` is the record of it. The camera followed
head movement in first and third person, the character did not turn along with it, and
`Del` recentered. Four things the log settles on its own:

- **the SteamVR connection**: `OpenVR: connected through FnTable:IVRSystem_026`. The
  interface version and the calling conventions replicated in `src/vr/OpenVRTypes.h` are
  the right ones.
- **the Mod Organizer 2 anchor**: `Config: ...\Data\OBSE\Plugins\OBVR.ini`. OBVR found its
  own INI next to the plugin DLL rather than in the game root, which is the whole point of
  `platform/PluginPath`.
- **the recenter edge**: four presses, four log lines, none of them repeating. A held key
  fires exactly once, which is what `KeyEdge` is for.
- **the point-of-view transition**: six switches, every one of them reported once and in
  the right direction.

Two things it does **not** prove, and neither should be claimed:

- **the 4GB path.** The exe carries the LargeAddressAware flag, but the trampoline landed
  at `0x03B60000`, well below 2 GB. `VirtualAlloc` hands out low addresses first, so a
  patched exe alone does not force the case. Only `TestLargeAddressAware` covers it.
- **the vanity and dialogue cameras.** Both run through branches of their own and were not
  deliberately exercised.

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
│   │   ├── CameraTrampoline.{h,cpp}    byte generation, platform free, tested
│   │   ├── FrameLogic.{h,cpp}          per-frame decisions + frame clock, tested
│   │   └── LookControl.{h,cpp}         what the stick may still do, tested
│   ├── core/
│   │   ├── CodeWriter.{h,cpp}          mini assembler
│   │   ├── Config.{h,cpp}              INI + hot reload
│   │   ├── Log.{h,cpp}                 logging without a CRT dependency
│   │   ├── MathFns.h                   sin/cos/sqrt, freestanding capable
│   │   ├── Memory.{h,cpp}              SafeWrite / Verify / AllocExecutable
│   │   ├── Rotation.{h,cpp}            EulerToMatrix (verified in the game)
│   │   ├── Smoothing.{h,cpp}           easing, for the camera OBVR moves itself
│   │   └── Types.h
│   ├── game/
│   │   ├── GameAddresses.h             every address with its disassembled evidence
│   │   ├── GameTypes.h                 NiAVObject (32-bit layout)
│   │   └── NiMath.h                    NiPoint3/NiMatrix33/NiTransform
│   ├── obse/PluginInterface.h          binary-compatible xOBSE replica
│   ├── platform/
│   │   ├── Freestanding.cpp            SDK-free build only
│   │   ├── PluginPath.{h,cpp}          plugin and game anchor, for Mod Organizer 2
│   │   └── Win32Min.h                  narrow Win32 layer
│   └── vr/
│       ├── HeadOffset.{h,cpp}          head position to camera offset, tested
│       ├── HeadTracker.{h,cpp}         interchangeable head source
│       ├── OpenVRBackend.{h,cpp}       SteamVR connection, loaded at runtime
│       ├── OpenVRTypes.h               binary-compatible OpenVR replica
│       └── Quaternion.{h,cpp}          quaternion + change of basis
└── tests/
    ├── TrampolineTest.cpp
    ├── QuaternionTest.cpp
    ├── RotationTest.cpp
    ├── FrameLogicTest.cpp
    ├── HeadOffsetTest.cpp
    ├── LookControlTest.cpp
    ├── OpenVRPoseTest.cpp
    ├── OpenVRBackendTest.cpp
    ├── ConfigTest.cpp
    ├── HeadTrackerTest.cpp
    └── PluginPathTest.cpp
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
- `config_test` — INI parsing, above all the virtual-key code parser: decimal and hex, and
  the rejection paths that have to keep the previous setting rather than silently disabling
  the key. Windows only, since Config reads through `GetPrivateProfileString`. Each case
  writes its own file name, because Windows caches the contents of the most recently used
  INI.
- `rotation_test` — `EulerToMatrix` against its own definition rather than against another
  implementation. It is the reference the rest of the suite compares to, so a silent sign
  flip there would leave every other test green while agreeing with a wrong answer.
- `head_tracker_test` — each source, recenter semantics, and the regression guard that a
  hot reload with unchanged settings must not reset the recenter reference. Windows only.
- `plugin_path_test` — the two anchors and the buffer-too-small contract. Windows only.
- `look_control_test` - what happens to the look controls once a headset takes over: that
  the camera stops tilting, that turning still works, that the vertical look becomes height
  in third person and nothing at all in first, and the easing of both. It caught a real
  fault while being written, recorded below. Pure arithmetic, so it runs on Linux too.
- `head_offset_test` - the arithmetic of positional tracking: which way a lean moves the
  camera, that only the difference against the reference counts, that the lean is read in
  the frame the user recentered in, the lean limit, and the smoothing. Pure arithmetic, so
  it runs on Linux too.
- `frame_logic_test` — the per-frame decisions of the camera hook, lifted out of the
  callback into `camera/FrameLogic`: the recenter key edge, the point-of-view transition,
  and whether a periodic action is due. The edge is the one that matters. Without it a held
  key would recenter on every frame, taking the current pose as the new zero sixty times a
  second — and the symptom would be a camera that appears frozen, which points nowhere near
  the cause. Pure logic, so this one runs on Linux too.

**Deliberately not covered**, so nobody goes looking for it:

- `core/Memory` — `Verify` and `SafeWrite` take an address as a `UInt32` and cast it
  straight to a pointer. That is right for the 32-bit process OBVR is loaded into and
  truncates in a native 64-bit test build, so these are only exercisable from a 32-bit
  build. The bytes they transport are covered by `trampoline_test`.
- `camera/CameraHook`'s callback — it needs a live `CameraNode` and Oblivion's player
  pointer. What is left in it is the reading of game memory, the logging and the one line
  that multiplies the rotation onto the camera; every decision it used to make has moved to
  `camera/FrameLogic` and is covered by `frame_logic_test`.
- `core/Log` — writing to a file and to the debugger. Nothing to get wrong that a test
  would catch before a reader would.

### One the tests caught

Worth recording, because the reasoning was wrong rather than the typing. Easing between two
headings held as a cosine and a sine handles the seam at 360 degrees for free, and the
degenerate case is two headings pointing opposite ways. The first implementation looked for
it *after* the easing, on the grounds that the midpoint of the straight line between them is
the origin.

That is only true for a step of exactly half. At a step of 0.2, easing `(1, 0)` towards
`(-1, 0)` gives `(0.6, 0)`, which renormalises straight back to `(1, 0)`: the camera would
have sat still while the game turned, then flipped once the step grew past half. Opposite
headings are now recognised before the easing, from the dot product. A camera that turns
right round in one frame was cut rather than panned, so the target is taken as it stands.

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

### Mod Organizer 2

MO2 virtualises the game's `Data` folder and nothing else. Its own issue tracker is blunt
about why the root cannot be virtualised: USVFS is installed after the load-time DLLs, so
from the game's point of view those files would not exist yet when it needs them. That is
why the separate [Root Builder](https://kezyma.github.io/?p=rootbuilder) plugin exists — it
copies or links root files into place for real, and names script extenders (SKSE, OBSE,
FOSE) as its main use case.

**xOBSE needs Root Builder.** `obse_loader.exe`, `obse_1_2_416.dll` and
`obse_steam_loader.dll` sit in the game root and cannot be delivered any other way. That is
xOBSE's constraint rather than OBVR's, and an MO2 user will already have it set up.

**OBVR itself does not.** It anchors its own files on `OBVR.dll` instead of `Oblivion.exe`:

| File | Where OBVR looks | Under MO2 |
| --- | --- | --- |
| `OBVR.ini` | next to `OBVR.dll` first, then the game root | virtualised, ships with the mod |
| `openvr_api.dll` | next to `OBVR.dll` first, then the default search | virtualised, ships with the mod |
| `OBVR.log` | game root, always | not virtualised, written for real |

Which makes the mod folder an ordinary MO2 mod, with no `Root` folder at all:

```
OBVR/
└── OBSE/
    └── Plugins/
        ├── OBVR.dll
        ├── OBVR.ini
        └── openvr_api.dll        (x86, from SteamVR's bin/win32)
```

Two reasons for preferring the plugin directory, beyond dropping the Root Builder
dependency:

- The INI follows the MO2 profile, so different profiles can carry different settings.
- Root Builder's own documentation lists `.ini files` among the usual exclusions. An
  `OBVR.ini` placed in a `Root` folder might quietly never be deployed, and OBVR would run
  on defaults without anyone noticing.

The log stays in the game root deliberately. Written into a virtualised path it would land
in MO2's Overwrite folder — correct behaviour, but awkward for the one file users get asked
to attach to a bug report.

Shipping everything through Root Builder instead still works: the layout is a `Root` folder
alongside the Data contents. Note that Root Builder ignores a mod **entirely** if it finds a
folder named `Data` inside `Root`.

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

## 13. Next step: 0.0.5, pictures in the headset

0.0.1 to 0.0.4 are verified in the game. What remains is the half OBVR has not touched at
all: it reads from VR but has never written to it. 0.0.5 closes that loop — a frame loop, a
texture per eye, and a test image visible in the headset. Not Oblivion's world yet; that is
0.1.0. The point of the intermediate step is that it separates two things which would
otherwise fail together, and indistinguishably: talking to the compositor correctly, and
getting Oblivion's pixels out of Direct3D 9.

### What the compositor requires, and where it bites

Four findings, each read from a source rather than recalled. They are set down here because
three of them contradict decisions OBVR has already made.

**OBVR has to stop being a background application.** `src/vr/OpenVRTypes.h` picks
`VRApplication_Background` and says why: as a scene application it would claim the
compositor and take the scene away from whatever SteamVR is showing. That reasoning was
right for reading poses and is exactly what has to change now, because claiming the scene
*is* the feature. This is not a matter of taste. `Submit` checks the application type and
returns `VRCompositorError_IsNotSceneApplication` to anything that is not
`VRApplication_Scene`; `WaitGetPoses` does the same, and additionally returns
`VRCompositorError_DoNotHaveFocus` and throttles itself to 10 Hz when another application
holds focus. So the fallback path matters more than before, not less: a machine without a
headset must still get the game it had.

**`IVRCompositor` is a second FnTable, with its own indices.** From `openvr_capi.h`, in
order from the top of `VR_IVRCompositor_FnTable`:

| Index | Function |
| --- | --- |
| 0 | `SetTrackingSpace` |
| 1 | `GetTrackingSpace` |
| 2 | `WaitGetPoses` |
| 3 | `GetLastPoses` |
| 4 | `GetLastPoseForTrackedDeviceIndex` |
| 5 | `GetSubmitTexture` |
| 6 | `Submit` |

The same calling-convention trap applies as for `IVRSystem`: the global `VR_*` functions
are `__cdecl`, the pointers inside the table `__stdcall`. See the warning at the top of
`OpenVRTypes.h`, which is not repeated per interface.

**The compositor owns the frame loop, not OBVR.** `WaitGetPoses` blocks until the right
moment to start rendering and hands back the poses to render with; it is the clock. That
sits awkwardly beside the camera hook, which runs on Oblivion's clock. Which of the two
drives the other is the open design question of 0.0.5, and it should be answered before any
code is written rather than discovered afterwards.

**`Submit` takes no Direct3D 9 texture, and there is no flag that makes it.** `ETextureType`
has no D3D9 entry at all: `TextureType_DirectX` is an `ID3D11Texture`, then OpenGL, Vulkan,
IOSurface, DirectX12, DXGISharedHandle, Metal, Reserved, SharedTextureHandle.
`TextureType_DXGISharedHandle` is documented as overlay-only. `GetOutputDevice` lists D3D9
as "Not Supported". The one D3D9-shaped thing in the API, `GetD3D9AdapterIndex`, is for
taking the headset over as a fullscreen exclusive display — the old extended mode, not
compositor submission.

### The D3D9 problem, sharpened

0.0.5 sidesteps this by rendering its test image with a D3D11 device of OBVR's own. 0.1.0
cannot, and the constraint is tighter than "a bridge is needed":

**Plain Direct3D 9 cannot share surfaces at all.** Microsoft's *Surface sharing between
Windows graphics APIs* is unambiguous: "Direct3D 10.0, Direct3D 9c, and older Direct3D
runtimes do not support shared surfaces. System memory copies will continue to be used for
interoperability." `IDirect3DDevice9::CreateTexture` documents `pSharedHandle` as
"Reserved. Set this parameter to NULL", shareable only on the Vista-era (9Ex) runtime. An
attempt on a plain device fails with `Device is not capable of sharing resource`.

D3D9**Ex** can share, unsynchronised, and `ID3D11Device::OpenSharedResource` documents the
D3D9→D3D11 route with its restrictions: 2D only, one mip level, default usage, write only,
no MSAA, bind flags `SHADER_RESOURCE | RENDER_TARGET`, and only `R8G8B8A8_UNORM`,
`R10G10B10A2_UNORM` or `R16G16B16A16_FLOAT`.

Oblivion is from 2006 and creates a plain D3D9 device. So the two candidate routes are:

- **Force the game onto D3D9Ex**, through a `d3d9.dll` wrapper that hands back an
  `IDirect3D9Ex`, then share into D3D11 and submit. Keeps OBVR dependency-free, which has
  been a deliberate property so far. Unverified: whether Oblivion tolerates a 9Ex device —
  the runtimes differ in `D3DPOOL_MANAGED` handling and in device-lost behaviour, and that
  has to be tried rather than argued.
- **Let DXVK render in Vulkan** and submit Vulkan textures. Two independent precedents do
  this for D3D9 games: [openRBRVR](https://github.com/Detegr/openRBRVR) and
  [l4d2vr](https://github.com/sd805/l4d2vr).

**DXVK is the chosen route.** Two things decided it, and one of them corrects an earlier
assumption in this document.

**No fork is needed.** l4d2vr ships a modified DXVK, which made the route look like it cost
a large maintained fork. It does not: stock upstream DXVK already exposes public D3D9
interop, in `src/d3d9/d3d9_interfaces.h`.

| Interface | Queried from | Yields |
| --- | --- | --- |
| `ID3D9VkInteropInterface` / `…1` | `IDirect3D9` | `VkInstance`, `VkPhysicalDevice`, enabled instance extensions |
| `ID3D9VkInteropDevice` | `IDirect3DDevice9` | `VkInstance`, `VkPhysicalDevice`, `VkDevice`, the render queue, image layout transitions |
| `ID3D9VkInteropTexture` | `IDirect3DTexture9` / `IDirect3DSurface9` | `VkImage` plus its `VkImageCreateInfo` and `VkImageLayout` |

So OBVR stays a plugin that asks a `QueryInterface` question, rather than a project that
maintains a graphics driver. That is a different order of cost entirely.

**Under Proton it is already there.** Oblivion's D3D9 goes through DXVK to Vulkan whether
OBVR wants it or not, so a route that fights that is one that works on Windows and not on
Linux.

**But Windows comes first.** The stated order, as of 2026-08-25, is: get VR working on
Windows, then Linux. That does not change the destination, and it does weaken the argument
just made — under Proton DXVK costs nothing, on Windows it is an extra file from a project
that does not support Windows. The remaining case for DXVK still stands on its own: two
precedents, no fork needed, and it is advice this game gets anyway for reasons that have
nothing to do with VR. But the risk has moved from the second platform to the first, and
that is precisely what the D3D9Ex alternative below is being kept for.

#### What "depends on DXVK" actually means

Not an installation. DXVK on Windows is a single `d3d9.dll` beside `Oblivion.exe`, taken
from the **x32** directory of a release because Oblivion is a 32-bit process. Under Proton
it ships with Proton and needs nothing. Independently of VR it is already common advice for
this game — it lifts the 32-bit video memory ceiling and moves work off the single core
Oblivion is stuck on — and DXVK's own maintainers test against Oblivion 2006 directly
(issue #4862, 2025).

Three real costs, none of them fatal but all of them the user's problem rather than OBVR's:

- **DXVK does not officially support Windows.** Its own wiki: "While using DXVK on Windows
  may generally work […] we do not support it officially. Many issues with running DXVK on
  Windows are outside of our control." Linux is the supported platform, which for this
  project is the right way round, but it has to be said plainly to anyone on Windows.
- **`d3d9.dll` is contested ground.** ENB and several other Oblivion mods want to be that
  same file. Whatever OBVR does here has to coexist with a chain that already exists, or
  say clearly that it cannot.
- **The game root is not virtualised by Mod Organizer 2.** OBVR's own files avoid this by
  anchoring on the plugin DLL under `Data`, but `d3d9.dll` cannot: it has to sit next to the
  executable, so MO2 users need Root Builder or a manual copy for that one file. The
  property "OBVR needs no Root Builder" survives only for OBVR's own files.

**Both things that were unverified here are now settled, and both in favour of the route.**

**Every field OpenVR needs is exposed.** `VRVulkanTextureData_t` wants an instance, a
physical device, a device, a queue, a queue family index, and the image with its format and
size. `ID3D9VkInteropDevice::GetVulkanHandles` gives the first three;
`GetSubmissionQueue(VkQueue*, uint32_t* pQueueIndex, uint32_t* pQueueFamilyIndex)` gives the
next two - the family index was the specific doubt, and it is right there in the signature;
`ID3D9VkInteropTexture::GetVulkanImageInfo` gives the rest. The same interface also carries
`TransitionTextureLayout`, `FlushRenderingCommands` and
`LockSubmissionQueue`/`ReleaseSubmissionQueue`, which is precisely the set needed to hand an
image over safely. DXVK built this for this.

**DXVK asks OpenVR for its extensions by itself.** `VrInstance` in `src/dxvk/dxvk_openvr.cpp`
calls `GetVulkanInstanceExtensionsRequired` and `GetVulkanDeviceExtensionsRequired` while
building the Vulkan instance and enabling adapter extensions. On by default on Windows.
**Do not set `DXVK_NO_VR=1`** - that is the one switch that turns this off, and it would fail
late and confusingly.

The choice of route does not block 0.0.5 in any case: proving the compositor connection,
the frame timing and the projection maths is worth the same either way.

#### Kept open: D3D9Ex, and why both could stand side by side

DXVK is the route to build first, not the route to build only. The D3D9Ex alternative is
recorded here in full because it may yet be needed, and because the reason to keep it is
not sentimental.

**The route.** A `d3d9.dll` wrapper hands the game an `IDirect3D9Ex` instead of an
`IDirect3D9`, which buys the shared surfaces plain D3D9 does not have. From there
`ID3D11Device::OpenSharedResource` opens the surface on a D3D11 device and `Submit` takes
it as `TextureType_DirectX`. Restrictions as documented above: 2D, one mip level, default
usage, write only, no MSAA, `SHADER_RESOURCE | RENDER_TARGET`, and one of `R8G8B8A8_UNORM`,
`R10G10B10A2_UNORM`, `R16G16B16A16_FLOAT`.

**What it buys.** OBVR stays dependency-free, which has been a deliberate property from the
start and is why the mod loads on a machine with nothing installed but the game. It also
needs no file the user has to fetch, and nothing in the game root beyond what a wrapper
would put there anyway.

**What is unverified, and cannot be argued either way.** Whether Oblivion tolerates a 9Ex
device at all. The two runtimes differ in `D3DPOOL_MANAGED` handling — 9Ex removes the
managed pool — and in device-lost behaviour, where 9Ex mostly does not lose devices and a
game written to expect that it does may take a path that never runs. A 2006 engine has no
reason to be careful about either. This has to be tried on the actual binary; no amount of
reading settles it.

**Why both is a real option rather than a hedge.** They fail in different places. DXVK is
unsupported on Windows by its own maintainers and contests `d3d9.dll` with ENB; D3D9Ex
depends on a 2006 engine tolerating a runtime it was never written for. Neither risk covers
the other. And the seam between them is narrow: both end at `Submit` with a texture and
bounds, differing only in what kind of texture and where it came from. If the eye textures
are produced behind one interface with two implementations, choosing between them is a
setting rather than a rewrite — the same shape `TrackerSource` already has for head poses,
which is why that pattern is worth reusing rather than reinventing.

### Alternate eye rendering was tried, and the compositor said no

Built, switched on, and it does not work - `docs/verification/OBVR-aer-refused.log` is the
record. SteamVR fell back to its Home scene and OBVR showed nothing.

**The log is decisive by what it does not contain.** No `stopped rendering`, no failed
`Submit`, no error of any kind: every call reported success. The only thing different from
the working run was that one eye was submitted per frame instead of two.

So the question this document flagged as unestablished has an answer, and it is the
unfavourable one: **the compositor counts a frame as delivered only when both eyes have been
submitted.** A single eye returns success and is not a frame, and after ten such non-frames
the scene fades out exactly as the documentation says it does for an application that has
stopped submitting.

Worth noting how that failure presented. Nothing failed. Had it not been written down in
advance as the thing that would decide whether AER works at all, the search would have
started from "why is Submit silently broken" rather than from "Submit is fine, the frame is
incomplete".

Confirmed in passing: `Head: eye separation 62.0 mm, half of it 2.17 Oblivion units`. The
conversion works, and the camera offset it feeds is correct - it simply had nowhere to go.

**What AER needs instead.** Both eyes every frame, with the eye waiting its turn showing a
kept copy of its own last picture. That means OBVR owning two images rather than borrowing
one, and copying into them:

- `IDirect3DDevice9::CreateRenderTarget` at vtable index **28**, twice, matching the back
  buffer
- `IDirect3DDevice9::StretchRect` at vtable index **34**, once a frame, from the back buffer
  into the current eye's target
- each target's `VkImage` through `ID3D9VkInteropTexture`, read once and cached
- both submitted every frame

Both indices counted from Wine's `include/d3d9.h`, interface facts rather than game ones.
Whether a render target made this way carries the `TRANSFER_SRC` and `SAMPLED` usage bits
the compositor requires is **not established** - the back buffer has them, but that is not
the same claim. `IsSubmittableImage` already answers it at runtime, so it costs one run to
find out rather than an argument.

### Where 0.1.0 stands, and the two things left

Oblivion's frame reaches both eyes, aligned, at no measurable cost in frame time.
`docs/verification/OBVR-gameframe-aligned.log` records it, with the computed split
`game frame bounds left u=0.034..0.834 right u=0.161..0.961` - both inside the texture,
both the same width, each eye given the side that puts the picture on its own axis.

Two things remain, and they are independent of each other.

**The frame is one frame old.** The camera hook runs while the camera is being computed,
which is before the frame is drawn - so the back buffer holds the previous frame. With a
static test pattern that made no difference and it was recorded as harmless; with the game's
own picture it is a real latency, on top of everything else in the chain. The fix needs no
new address: `IDirect3DDevice9::Present` is at vtable index 17, an interface fact rather
than a game one.

**There is no depth between the eyes.** One image shown twice has none, whatever the bounds
do. Two routes:

- **Dual-pass**, the stated goal: render the world twice per game tick, once per eye. This
  is where the old open question bites - can Gamebryo be made to draw twice without
  advancing simulation, physics, particles and animations twice? Unanswered, and it is the
  question the whole milestone was named for.
- **Alternate eye rendering**, listed here as plan B and worth more than that label
  suggests. Offset the camera to one eye per frame and alternate, which is what Luke Ross's
  mods do - "stereoized the world rendering by shifting the in-game camera into the eye
  positions on alternate frames". It needs no double rendering at all, and OBVR already
  moves the camera every frame in exactly the place such an offset would go. What it does
  need is somewhere to keep the other eye's previous image, and whether OpenVR tolerates one
  eye being submitted per frame is **not established** - the documentation says only that
  ten frames without any Submit fades the scene out.

The cheap route is worth trying first for the same reason the mono step came before stereo:
it gives depth now, and it proves the eye offsets are usable before anything is rebuilt
around them.

### 0.1.0, step one: reaching Oblivion's device

Done, and logged rather than acted on. `addr::kRendererPointer` is `0x00B3F928`, a pointer
to `NiDX9Renderer`, and the device sits at `+0x280` inside it. Two independent sources and a
check against the binary, which is the standard `GameAddresses.h` holds addresses to:

- the address from OBGEv2's `Nodes/NiDX9Renderer.cpp`, in a namespace named `v1_2_416` -
  the same build OBVR targets
- the offset from xOBSE's `obse/obse/NiRenderer.h`, which lays out `NiDX9Renderer` with
  `IDirect3DDevice9 * device; // 280` and compile-time asserts its size and two offsets
- the encoding of `mov eax, [0x00B3F928]` - `A1 28 F9 B3 00` - appears **41 times** in
  `Oblivion.exe`. A five byte sequence does not occur that often by chance.

**Confirmed in the game, twice over.** `docs/verification/OBVR-d3d9-device.log` has
`Render: Oblivion's D3D9 device 07F86CE0 is native Direct3D 9` - the predicted answer, since
there was no `d3d9.dll` in that game root. Two runs reported *different* pointer values,
both non-null and both answering QueryInterface, which is how a heap-allocated COM object
behaves; a wrong address would have given garbage or a constant.

Then the x32 `d3d9.dll` from a DXVK release went next to `Oblivion.exe`, and
`docs/verification/OBVR-dxvk-confirmed.log` has
`Render: Oblivion's D3D9 device 0BB34CC0 is DXVK`. **That is the whole route confirmed before
a line of code was written for it**: the address is right, the IID was transcribed right -
now proven in the game rather than only against its own text - and stock upstream DXVK
answers `ID3D9VkInteropDevice` with no fork involved. The run is clean, with no warning of
any kind, and Oblivion, DXVK and OBVR coexist.

Frame times were 18.4 to 18.7 ms against 18.0 to 18.5 without DXVK. Five samples is too few
to claim anything, and nothing is claimed.

`render::GameDevice` reads it and asks it one question: does it answer to
`ID3D9VkInteropDevice` (`2eaa4b89-0107-4bdb-87f7-0f541c493ce0`, from DXVK's
`d3d9_interfaces.h`)? That single QueryInterface decides which of the two routes in this
section is actually open on a given machine, and it is written to the log on the first hook
pass as `Render: Oblivion's D3D9 device XXXXXXXX is ...`.

The identifiers have a test to themselves, because of how they fail. A mistyped IID does not
crash: QueryInterface answers "no such interface", OBVR concludes DXVK is absent, and the
project commits to the harder route for no reason and with nothing anywhere saying why. The
test renders the bytes back into the canonical text form and compares against the string in
DXVK's header, which is the one thing a transcription error cannot survive.

### The end of the frame, without another address

The camera hook runs while the camera is being computed, which is before the frame is
drawn. 0.1.0 needs the other end - the moment the game has finished drawing - and that does
not need a new address in `Oblivion.exe`.

`IDirect3DDevice9::Present` sits at **vtable index 17**, counted from Wine's `include/d3d9.h`:
three `IUnknown` entries, then `TestCooperativeLevel`, `GetAvailableTextureMem`,
`EvictManagedResources`, `GetDirect3D`, `GetDeviceCaps`, `GetDisplayMode`,
`GetCreationParameters`, the three cursor methods, `CreateAdditionalSwapChain`,
`GetSwapChain`, `GetNumberOfSwapChains`, `Reset`, and then `Present`.

That is an API fact rather than a game fact, and it is worth more than an address for
exactly that reason: it holds for every game version, every patch, and every D3D9
implementation including DXVK. With the device already in hand, the end of the frame is one
vtable patch away and needs nothing found in the binary.

Not written yet, deliberately. There is nothing to do at the end of a frame until there is a
texture from the game to submit, and code written before it has a purpose is code written
against a guess.

### The order of work for 0.0.5

1. ~~`IVRCompositor` in `OpenVRTypes.h`~~ — **done.** Table, indices, `Texture_t`,
   `VRTextureBounds_t`, the submit flags, all read out of the header and pinned by
   `openvr_pose_test`.
2. ~~Scene rather than background, and a fallback at least as safe as today's~~ — **done.**
   `Render.Enabled` in the INI, off by default, read at startup only. `OpenVRBackend::Start`
   now takes the application type; if the scene registration succeeds but the compositor
   cannot be reached, it gives the registration back and reconnects as background, so the
   picture is lost and head tracking is not.
3. ~~A D3D11 device of OBVR's own, one texture per eye, a generated test image~~ — **built,
   not yet reachable.** `src/render/` holds `D3D11Types.h` (hand-rolled, from Wine's
   `d3d11.idl` and the `D3D11CreateDevice` reference page), `TestPattern` and `EyeTextures`.
   `d3d11.dll` is loaded at runtime like `openvr_api.dll`, so the import list is still
   kernel32, msvcrt and user32 and the SDK-free build is untouched. Nothing calls it yet —
   the linker discards it, and the DLL is the same size as before. Step 4 is what makes it
   run. ← **next**
4. ~~`WaitGetPoses` in a frame loop, and the decision about which clock leads~~ — **built,
   not yet run with a headset.** The compositor's clock leads: `WaitGetPoses` is called from
   the camera hook, on Oblivion's thread, so the game runs at the headset's rate. That is
   the arrangement 0.1.0 needs, since the texture submitted then has to be the frame the
   game has just drawn. `render::HeadsetRenderer` does the per-frame work and
   `render::SubmitPolicy` decides when to give up — see below, because that decision is not
   optional.
5. ~~`GetProjectionRaw` and `GetEyeToHeadTransform`~~ - **read and logged, not yet used.**
   `render::EyeGeometry` turns them into field of view, frustum asymmetry, optical centre
   and interpupillary distance, and `HeadsetRenderer` writes all of it out once when
   rendering starts. Deliberately stopping short of acting on them: the sign convention of
   the frustum is undocumented, and the next in-game log is what settles it. ← **next: use
   what that log says**
6. The in-game run: `Render.Enabled=1`, SteamVR running, and a look at whether the pattern
   arrives whole, upright, unmirrored and on the right eyes.

#### Two things about step 4 that are known to be imperfect

**The hook point is early.** The camera hook runs while the camera is being computed, which
is before the frame is drawn rather than after. A generated test pattern is identical every
frame, so this makes no difference now. Oblivion's own pixels will differ, and submitting
them from here would show the previous frame — which looks exactly like a correct picture,
only wrong. 0.1.0 needs a second hook point at the end of the frame.

**Blocking on `WaitGetPoses` is a loaded gun.** It is what puts the game in step with the
headset, and it is also how the game ends up at ten frames a second: without focus the call
throttles itself to 10 Hz, and a thread blocking on that inherits the rate. Nothing on
screen points at VR, so it reads as a driver fault and the person debugging it is not
looking anywhere near here. `SubmitPolicy` therefore reads the answers and gives up
rendering rather than the frame rate — immediately for failures that cannot pass
(`IsNotSceneApplication`, and both texture faults, which are properties of how the texture
was made), and after ninety consecutive frames for ones that can. The count is consecutive
rather than total on purpose: over a long session an occasional lost frame would otherwise
add up to a shutdown.

**The predicted poses are being discarded.** `WaitGetPoses` hands back the poses to render
this frame with, and they are predicted forward to when it will be shown — strictly better
than the unpredicted ones `ReadHeadPose` asks for separately. Feeding the head tracker from
there is a real improvement, deliberately not made in the same change that introduced the
frame loop.

Steps 1 to 3 are testable without a headset in the same way everything else here is: the
FnTable indices against the header, the texture arithmetic against known values, the
fallback against a machine with no SteamVR.

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
- [sd805/l4d2vr](https://github.com/sd805/l4d2vr) — a second D3D9 precedent, and the more
  explicit one: it ships a modified DXVK fork as the game's `d3d9.dll` and submits Vulkan
  textures per eye. Useful mainly as evidence that the DXVK route is walked rather than
  merely proposed.
- [DR-89/fear-vr](https://github.com/DR-89/fear-vr) — two-process architecture with shared
  memory, should it ever be needed.
