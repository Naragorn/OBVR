# Debugging

The instruments OBVR carries, the harness around it, and the techniques that found the
faults. Generic techniques first; Oblivion-specific addresses and probes after.

## The log is the evidence

`Scope: General VR` (the discipline), `Scope: Oblivion` (the lines)

`OBVR.log` next to `Oblivion.exe`; the previous run survives as `OBVR.log.prev` (kept
because the run that matters is usually the last one, `8721791`). Written unbuffered, so
a crash leaves the last line intact. Lines are budgeted (a handful per event, then
silent) and most report *changes* rather than states, so a log stays readable over a
session.

What a healthy start looks like, in order: version and xOBSE/Oblivion versions; the INI
path; "Camera: hook installed"; the aim wraps ("Aim: ... wrapped"); "Render: the scene
render is hooked", "Culling: process hooked", the interface hook; "OpenVR: connected as a
scene application through FnTable:IVRSystem_026 and FnTable:IVRCompositor_029, tracking
space set to seated"; "Render: Oblivion's D3D9 device ... is DXVK" with the Vulkan
handles and the back buffer's usage bits; the eye geometry (raw frustum, FOV, asymmetry,
eye offsets, IPD with a plausibility flag); "Head: eye separation ..."; "Poses:
WaitGetPoses runs under DXVK's queue lock"; "Resolution: the game asked for ... and is
getting ..."; "Dual pass: the left eye is drawn first"; "Dual clock: frame delta reads
... zeroed"; "Hud: live".

Lines that say a claim is *not* holding: "bytes at ... differ" plus "... was patched
first by <dll>" (a foreign detour); "stopped rendering, WaitGetPoses returned 101" (no
focus, 10 Hz throttle avoided); "NOT submittable as it stands" (image usage); "Aim: a
written heading did not land"; "Bone unpaired"; "Culling sync ... camera mismatches";
"Watchdog: ...".

Per-frame logging: `Debug.LogEveryFrames=N` prints position, head quaternion, lean and
raw lean every N frames; `AimShotTrace`/`AimCastTrace` print the aim's columns during a
shot; the menu trace prints twelve frames of delivery/camera-pass/world-renders/layer
state whenever a menu opens, closes or changes type; the step trace marks the blocking
calls for thirty frames after a view switch.

## The probes (all in `[Debug]`, hot reloaded)

| Key | What it measures | Where it logs |
| --- | --- | --- |
| `DualPassProbe` 0/1/2/3/9 | The dual pass cut down rung by rung: 1 = second render only, 2 = plus the camera move, 0 = everything, 3 = **no second render at all** (mono submit, world still delivered), 9 = a sweep through bands of scene calls | "Scene call N (dual, rung R)", "Hud invocation" |
| `SwapEyeOrder` | Draws the right eye first, everything following (step sign, shift direction, capture order) | "Dual pass: the right eye is drawn first" |
| `HudProbe` | Paints an opaque red square into the layer texture and starts the redirected pass from a visible half-transparent clear; logs the first draw's pipeline state | separates "the overlay path is dead" from "the layer is transparent" |
| `AimProbe` | The player rotation beside the camera heading, sum and difference, read before anything is written | the sign of every aim convention |
| `ThirdPersonProbe` | The chase camera's LOCAL/EYE/pivot columns per frame | `tools/third-person-run.ps1` |
| `FirstPersonTreeProbe` | The first-person node tree by name, once | naming the arms node |
| `MenuWorldProbe` | One self-initiated world render on held/cinema menu frames, draw and setup counts, with a control on a world frame; also from inside the 2D pass ("Place probe") | "IT DRAWS" / "empty" |
| `D3D9ExProbe` | Hands the game an `IDirect3D9Ex` and `CreateDeviceEx` | the crash dump that closed the route |
| `LayoutProbe` | Reads back the covered rectangle of the layer texture / the back buffer every 120 frames | which size each part of the 2D lays out against |
| `CursorProbe` | The interface manager's cursor fields, the tile node's translation, plus BMP dumps of layer and back buffer | `OBVR-layer.bmp`, `OBVR-back.bmp` |
| `CrosshairProbe` (under `[Render]`) | Whether HUDInfoMenu identified itself, the reference under the crosshair, the depth that came out | how often the fallback depth is in use |

Instruments that are always on, budgeted: the scene-call trace (every tenth call between
150 and 500), the dual-frame draw/state/buffer/palette/binding/skinned/pool-write/
zero-matrix/index-side lines every 120 scene calls, the bone-lock self-check line, the
bone-anomaly line on change (120 budget), the culling-sync line, the one-shot pool
timeline dump on the first steady-state divergent frame (`ArmPoolTimeline`, criteria in
`HookedRenderScene`), the engine-side scene graph report (three times), the engine's own
render cost (three times), the between-render HUD trace (rearmed when a menu opens).

## The headless harness

`CONFIRMED`, `Scope: Oblivion` (scripts), `Scope: Legacy games` (the idea)

`tools/*.ps1` start the game through the xOBSE loader with SteamVR running and the
headset asleep (poses read level, stereo never arms, cinema frames are what a run
produces), wait for log lines, inject keyboard scan codes and relative mouse movement
(the route that reaches DirectInput; `xdotool`-style XTEST input never did under Proton),
walk the game's own cursor to positions, click, screenshot the window (`cursor-shot.ps1`
pushes the oversized window up so the menu's lower half is on the monitor), and print the
probe lines. Every script refuses to run if Oblivion is already running ("a session
someone is playing is never touched") and never saves. Each script's header says what
question it answers.

Two lessons from the Linux era (`HANDOFF.md` section 12): the game pauses without focus,
so a hot reload that "did not take" is usually an unfocused window; synthetic clicks
did not arrive in the main menu under Proton while working in game.

`tools/DxvkRepro/` replays a recorded pool timeline against a real `d3d9.dll` twice per
frame with deterministic vertex data and compares the passes pixel for pixel; it
acquitted DXVK and the D3D9 traffic in the skinning collapse.

## The watchdog

`CONFIRMED`

`core/Watchdog`: a thread that watches the presented-frame counter and, when frames stop,
writes the last step mark the render thread reached (`NoteStep`, string literals only) and
how long the stall has stood; once per stall (`StallDetector`, tested). It does not try to
unblock anything: the one call that can wait for ever is the compositor's. Its report is
what put the freeze investigation inside DXVK's condition-variable wait.

## The freeze hunt

`CONFIRMED`, `Scope: DXVK` / `Scope: OpenVR` / `Scope: General VR` (the shape)

Symptom: the game froze after a view switch, in the Esc menu, at loading screens, right
after a load; the monitor stopped, SteamVR's own view kept tracking; the log simply
ended. Dozens of times over a week under two driver versions.

Instruments, in order: `OBVR.log.prev` secured first (it survives restarts); the step
trace around `WaitGetPoses` (a log ending on "about to wait for poses" locates the hang,
one reaching "poses are in" clears it); the watchdog's last-step report; DXVK's
`Oblivion_d3d9.log` (`VK_ERROR_DEVICE_LOST` repeated for ever, reported by the next
submission rather than by its cause); the Windows System event log, provider `nvlddmkm`
("Graphics Exception: Class 0xc9c0 Subchannel 0x0 Mismatch", a corrupted command stream,
one per freeze since 28.08); `DXVK_DEBUG=hang` (DXVK 3.0+, narrows the crash location).

Cause: `WaitGetPoses` accesses the Vulkan queue in the default timing mode (Valve's
Vulkan wiki), DXVK's CS thread submits on the same queue whenever it likes, and Vulkan
allows one thread per queue. The submits were already under DXVK's queue lock; the wait
was not. Fix `6f4ccde`; the headset confirmation is still open (README "The freeze fix").

Generic lesson: a lost device is reported by the *next* submission, so the log alone
cannot name the trigger; the trigger has to be bisected by a ladder of switches that cut
the mechanism down (`DualPassProbe` was built for exactly that: "a lost device names no
culprit, so a ladder is built to find one", `d1cf4a9`).

## Techniques that found things here

`Scope: General VR`

- **Write the risk down before the run.** Every AER fault (one eye is not a frame, the
  stale-frame eye swap, the vertical sign) arrived already attributed because it had been
  named as a risk in the commit that preceded it.
- **Hot-reload a switch and compare while standing at the thing.** `DualPassProbe=3`
  versus 0 at a pond settled the water reflection in a minute; `Menus=cinema` versus
  `world` compares the two deliveries without moving.
- **Count what each pass sets up, not only what it draws.** Draw counts matched between
  the two renders while the constants disagreed; per-register sums separated camera
  blocks from bone palettes; order-independent sums were the blind spot that the pool
  timeline and the zero-matrix-draw counter closed.
- **Read a result back rather than logging an intention.** The aim was proved by reading
  the view's heading after the compensation, and the written heading after the frame,
  not by logging what was written.
- **Measure a rate, do not model it.** The chase camera at 60 Hz physics under a 90 Hz
  renderer; animation lengths in frames, not seconds.
- **Separate the two candidates with one switch.** `SwapEyeOrder`: a fault that follows
  the render order belongs to the render, one that stays in the left eye belongs to the
  eye.
- **Suspect the moment, not only the place.** The self-initiated world render drew
  nothing from Present and 461 primitives from inside the 2D pass; same call, same scene.
- **Ask the binary before reasoning about it.** "Oblivion imports Direct3DCreate9" was
  plausible and false (it loads d3d9 by hand); "the menu redraws the world on some
  frames" was carried over from an earlier investigation and false.
- **Guard every pointer read with a plausibility test** (`LooksLikeObjectAddress`, a name
  that reads, an id that matches) because these hooks run while the object model is torn
  down and rebuilt.

## Matrices and values worth printing

- The four raw frustum tangents per eye and the optical centre they imply; the IPD with
  a 55-75 mm plausibility flag; the recommended render size.
- The game's frustum from the camera (`l r t b n f`) and the projection matrix's
  `m[0][0]`, `m[1][1]` (with the identity guard); the two must agree.
- The lean triple and the raw lean magnitude (tells a small lean from a clamped one).
- The camera heading beside the player's `rotZ` (they sum to zero when the head is
  centred), and the view heading before and after the compensation.
- Per dual frame: draws, state calls and bone-range sums per pass; replaced/passthrough/
  offscreen bone rows; captured/replayed/mismatched culling calls.
- `WaitGetPoses`'s return code with the HMD pose's validity and tracking result, on change.

## External tools

- RenderDoc / PIX: not used in this project (DXVK on Windows plus a 32-bit process plus
  the interop path make a capture awkward); the device-method-table counters were the
  substitute. `UNVERIFIED` that RenderDoc's D3D9 capture works with DXVK interop images.
- Visual Studio debugger / WinDbg: not needed so far; the linker map shipped beside each
  release (`OBVR-<version>.map`) reads a crash address back to a function.
- `dumpbin /disasm`, `objdump -d -M intel`: the disassemblers used for every address.
- Windows Event Viewer (System, `nvlddmkm`): the GPU's own account of a device loss.
- DXVK's log (`Oblivion_d3d9.log`) and `DXVK_DEBUG=hang`.
- The Steam `.bind` section note: SteamStub does not encrypt `.text` on this exe.
