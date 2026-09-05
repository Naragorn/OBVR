# Rendering and stereo

How OBVR gets two eye images out of a 2006 Direct3D 9 engine, what that costs, what it
broke, and what was measured along the way. Read [architecture.md](architecture.md) for
the frame lifecycle first.

## Stereo taxonomy, and where OBVR sits

`Scope: General VR`

| Model | What renders the second eye | Same simulation tick | Temporal artefact | Engine access needed |
| --- | --- | --- | --- | --- |
| Native engine stereo | The engine's own multi-view path (UE's `IStereoRendering`, OpenMW's stereo manager) | yes | none | engine has the concept |
| Engine-hook geometry stereo | A mod calls the engine's view/render setup a second time with a moved camera | yes if the mod prevents time from passing | none, if state is held | render entry, camera write, frame clock |
| Sequential stereo | Two full renders one after the other in the same frame; a sub-case of the above | yes | none in principle; per-render state (skinning, culling, temporal effects) can diverge | as above |
| Same-tick geometry stereo | Emphasises that both renders come from one simulation state; what "sequential" must also be to be correct | yes | none | as above |
| Alternate-eye rendering (AER/AFR) | Each display frame draws one eye; the other eye shows its last image | no (one tick per eye) | disparity in time: fast motion doubles, TAA smears | camera write only |
| Depth reconstruction (Z3D) | One image plus its depth buffer, warped per eye | yes | disocclusion holes, halos, wrong depth for transparencies and UI | back buffer and depth buffer |
| Generic injector | Graphics API interception with a heuristic per-shader stereo transform (3D Vision, geo-11) | yes | broken deferred/screen-space effects unless fixed by hand | shaders only |
| Engine source port | The engine itself gains VR (OpenMW-VR) | yes | none | source |

**OBVR is engine-hook, same-tick, sequential geometry stereo.** `CONFIRMED`

- The detour on `kRenderScene` (`0x0040C830`) calls the engine's own world render twice
  per tick (`render/SceneRenderHook.cpp`, `HookedRenderScene`).
- Between the calls the camera node's local position is stepped by the eye baseline along
  the head-carried x axis and the world transform recomputed with the engine's own
  `UpdateSelectedDownwardPass` (`0x00707370`), exactly the two steps the engine itself
  uses after its camera write.
- Both images are submitted in the same compositor frame with no explicit pose, because
  both were drawn from this frame's `WaitGetPoses` pose (`HeadsetRenderer::SubmitDualEyes`).
- Time is held: the frame delta at `0x00B33E9C` is zeroed for the second render, the
  skinning palettes are replayed from the first render, and the culling set is shared.

"Sequential" does **not** mean alternate-eye here. Sequential means one after the other
in one frame; alternate-eye means one per frame. OBVR has both, under `Stereo=dual` and
`Stereo=aer`, and dual is the default since it was verified on 2026-08-26 (`2cba14c`).

The README's phrase "native VR" means "the game's own render pipeline drawn per eye, not a
cinema screen or a depth trick". In this taxonomy it is not "native engine stereo".

## The dual pass in detail

`CONFIRMED`, `Scope: Oblivion` (addresses) / `Scope: Gamebryo` (the render entry pattern)
/ `Scope: Legacy games` (the approach)

Problem: Gamebryo has one camera per scene graph and one render per frame; nothing in the
engine knows about eyes.

Observed behaviour that made it possible: the function Oblivion Reloaded names `kRender`
(`0x0040C830`) draws one whole frame of the world - culling, both scene-graph passes
(world and first-person node), water, HDR image-space shaders - **and not the 2D layer**,
which is drawn later on a different path. It re-reads the camera node's position itself
each call to place the sky and LOD roots. The engine's update step has already run when
it is called. So calling it twice with the camera moved is "draw again, move nothing", as
long as nothing inside the walk advances on its own.

Approaches tested, in order (evidence: `HANDOFF.md` sections "Where 0.1.0 stands" through
"Dual pass is verified", commits `432149c` to `2cba14c`):

1. Mono: one image to both eyes, aligned per eye by texture bounds (`MonoBounds`). Works,
   no depth. Still the `Stereo=none` fallback, and it still uses the old fixed 80% bounds
   (magnified and stretched) by deliberate choice - the fallback is not touched while the
   thing being fallen back from is being fixed.
2. Alternate eyes, one eye submitted per frame: `DEAD END` - the compositor counts a frame
   as delivered only when both eyes arrived; SteamVR faded to Home with every call
   reporting success (`docs/verification/OBVR-aer-refused.log`).
3. Alternate eyes with OBVR-owned copies of both eyes, both submitted every frame: works
   (`OBVR-aer-working.log`), with the inherent one-frame disparity. Kept as `Stereo=aer`.
4. Dual pass through the render detour: works, no ghosting, clean cold start
   (`OBVR-dual-ladder-pass.log`, `OBVR-dual-cold-start-verified.log`), and then two
   months of second-render artefacts (below).

Current mechanism, per world frame (`SceneRenderHook.cpp`, `CameraHook.cpp`
`BetweenScenePasses`/`AfterSecondScenePass`):

1. The camera hook places the camera on the first eye (`StereoEyeStep`, one function so the
   sign cannot be copied wrong twice; `SwapEyeOrder` flips the order for diagnosis).
2. Pass 1 through the trampoline, with `BonePassMode::Capture` and culling capture on.
3. Between: `CaptureEye(first)` copies the back buffer (tone-mapped, no HUD) into that eye's
   texture; the 2D pass is run redirected (see [ui-and-hud.md](ui-and-hud.md)); the camera
   steps to the second eye and `SetBoneEyeShift` tells the lock how far.
4. Pass 2 with the frame delta zeroed (plausibility-checked: only when it reads 0..1 s),
   `BonePassMode::Replace`, culling replay.
5. After: `CaptureEye(second)`, camera restored and updated again so later readers (the 2D
   layer, next frame's smoothing) see the engine's camera.
6. Present: both textures submitted; if any piece is missing (hook refused, mirror
   unusable, a copy failed) the frame falls back to mono rather than pairing a fresh eye
   with a stale one (`DeliversDualEyes`, `request.dualEyes`).

Cost: the whole render pipeline twice per frame. Frame times were not measurably worse on
the test machine (a 2006 game on an RTX 4090); the log's per-frame lines are the
measurement for any other machine.

What is *not* doubled: the 2D layer (drawn once between the passes), the compositor
submit, the simulation. What is doubled and known to be fine: culling (now shared),
water reflection, HDR. What is doubled and known to misbehave: see below.

## Simulation versus rendering

`Scope: Gamebryo` / `Scope: General VR` - this is the question every sequential-stereo
mod has to answer for its engine.

**Does rendering the second eye mutate game state?** For Oblivion, measured:

| Subsystem | Advances during a second render? | Evidence | Status |
| --- | --- | --- | --- |
| Simulation, physics, AI, scripts | No. The update step (`0x0040D800`) runs once, before the camera update; the render walk does not call it. | `HANDOFF.md` "Dual pass is built"; scene counter versus frame counter in the menu trace | `CONFIRMED` |
| Animation controllers, NPC head-aim (the look-at that hair and helmets hang from) | **Yes**, by the frame delta at `TimeInfo+0x0C` (`0x00B33E9C`), on every render walk. Zeroing it for the second render stopped the double-ticking stutter and the sideways helmets. | commits `7e98925`, `664b56d`; comment at `kFrameSecondsAddress` | `CONFIRMED` |
| Skinning palettes (software-skinned actors, uploaded as vertex-shader constants) | The engine re-evaluates only *some* skeletons on the second walk; of those, some land on the wrong same-posed instance (a body length off); the rest stay stale at the first eye. | the whole bone-lock investigation, `docs/verification/pool-timeline-20260828.md` | `CONFIRMED` |
| FaceGen head parts (eyes, teeth, some hair) | Built inside a time-gated update: with the clock zeroed they keep the first eye's camera and render one eye baseline off in the second pass. A microsecond of delta reopened the gate and made it worse (vertical component). | `ceb2389`, `664b56d` | `CONFIRMED` as an artefact; the fix is in the FaceGen path and is **open** |
| Water reflection | Resets as the head moves, identically in mono and dual (`DualPassProbe=3` versus 0, hot-swapped at a pond). Engine behaviour, not the dual pass. | `HANDOFF.md` "Water reflections resetting" | `CONFIRMED` |
| SpeedTree foliage billboards | Cards are built for one camera; two cameras per frame means one eye sees them turned. | not chased | `LIKELY`, unmeasured |
| Particles | Not measured separately. With the clock zeroed the emitters should not advance; whether their vertex data is rebuilt per walk is `UNKNOWN`. | - | `UNKNOWN` |
| Shadow and reflection sub-passes | Drawn again in the second walk from light/mirror viewpoints; their bone rows are camera-free and must not be shifted (doing so put every shadow one eye over). | `1ea2994` | `CONFIRMED` |
| Frame counters, timers | The engine's own frame delta is the lever; OBVR's own counters (`frameCount`, `presentedFrame`, `sceneCall`) are kept apart deliberately. | code | `CONFIRMED` |

Reusable lesson: "the render does not advance the simulation" is only the first half. The
render *walk* carries its own time-driven work in Gamebryo (controllers, look-at,
FaceGen), and a second walk in the same frame replays exactly that. Find the engine's
frame delta and zero it across the second render; then look for what is cached per frame
rather than per walk (skinning, FaceGen), because that is what comes out one eye stale.

## The bone lock

`CONFIRMED` mechanism, `Scope: Oblivion` (registers, residues) / `Scope: Gamebryo`
(camera-relative palettes) / `Scope: General VR` (the failure class)

Problem: with the dual pass, some skinned bodies collapsed onto a point or stood a body
length away in one eye ("all NPCs in a pile", task #21), hair and helmets sat one eye
width beside the face (#11).

How it was found (2026-08-27 to 08-28, some thirty commits from `27fc4a2` to `b75ca70`): a
ladder of probes on the D3D9 device counted, per render, draws, state calls, constant
uploads, palette sums per start register, binding identities, dynamic buffer locks and
their written bytes, index-side state, vertex processing mode, draw addressing, and
finally a one-frame pool timeline replayed in a standalone program (`tools/DxvkRepro`).
Every order-independent sum came back equal between the two renders; the standalone
replay rendered identically; the bone-range sums differed. That located the fault in
what the engine computed, not in what D3D9 or DXVK did with it.

Root cause: the bone palettes are **camera-relative** (matched rows differ between the
renders by exactly the eye baseline, about 4.6 units at 66 mm and 70 units/m), and the
engine's second evaluation in the same frame does not reproduce the first: some rows are
not re-evaluated (stale at the first eye), some are evaluated against the wrong instance
(a same-posed NPC's root).

Approaches tested, in order:

1. Verbatim replay of the first render's rows onto the second (`a1d7791`): held the
   collapse down, froze every skinned body at the first eye's position - no parallax,
   seen double.
2. Pairing by upload position with a register-mismatch stand-down: broke when the mouse
   turned the view, because a camera turn reorders the second render's uploads
   (`cd63974`).
3. Pairing by content: the nine rotation floats of a 4x3 bone row are bit-identical
   between the renders and serve as a fingerprint; a bounded forward scan along a ring
   finds the pair (`cd63974`).
4. Selective lock, keeping "correct" rows and rebasing only "mixups" judged by translation
   distance (`91dd3c8`): `DEAD END` - bit-identical rows are stale, not correct, and most
   "mixups" were ring collisions between same-posed instances; bodies went grey.
5. Blanket replacement with the replaced row's translation **shifted by the camera's own
   eye-step vector**, sign calibrated at runtime from the rows that did re-evaluate
   (`b75ca70`): the current mechanism.
6. Excluding the shadow and reflection sub-passes by render-target size (`1ea2994`), after
   shifted shadow rows made shadows stand in the room.
7. Refusing off-position pairs a body length apart (`fcac8a3`): `DEAD END` - in frames
   where the second render uploaded fewer rows the ring slipped and 1322 of 1414 rows were
   refused, which is the collapse itself; reverted in `e4342b7`.

Current mechanism (`InterfaceRenderHook.cpp` `HookedSetVsConstantF`/`HandleBoneUpload`,
`BoneRebase.h`):

- A bone row is a `SetVertexShaderConstantF` upload of exactly three float4 vectors into
  one of three register classes, distinguished by residue mod 3: skin at c42..c93 (= 0),
  hair at c31..c88 (= 1), head parts at c14..c65 (= 2). The residues are "census-clean":
  the other three-vector constants in range land on other residues.
- During the first render every such row is logged (capacity 4096; busy frames carry
  about 1800). During the second, each arriving row is paired with the first fingerprint
  match along the ring from the running position, replaced by the logged row with its
  translation shifted by the calibrated eye delta, and passed on to the device.
- Rows heading for a render target narrower than the world's (shadow, reflection) pass
  through untouched in both modes so the ring stays in step.
- Rows whose translation differs from their pair by between half a unit and sixteen units
  are treated as re-evaluated rows and measure the true baseline; the sign of the
  palette convention against the camera shift is calibrated from that once and re-judged
  every frame. Until calibration the measured mean drives the shift; before any sample
  the delta is zero, which reproduces the old blanket lock.
- The lock verifies itself: the bone-range sums of a locked frame must come back equal,
  and the log line "Bone lock at scene call N" carries replaced/reordered/passthrough/
  offscreen counts.

Remaining limitations: the edge-of-view collapse of followers (a body one eye's frustum
holds and the other's does not) is mitigated by the shared culling below, and the
hypothesis that an unpaired row takes a same-posed stranger's translation is still a
hypothesis (`e4342b7`). The FaceGen head-part offset is not a palette problem and is open.

Prior art that names the same failure: BioShock VR's sequential stereo re-evaluates the
skeleton on the second pass and replays cached bone transforms
(`mohamad-balouza/bioshock-vr`, `src/game/bioshock1r/bones.cpp`, `reapply()`); Skyrim
Community Shaders keep per-eye data for the same reason (see
[ecosystem-and-prior-art.md](ecosystem-and-prior-art.md)).

Reusable lesson (`Scope: General VR`): in any engine that evaluates skeletons against
camera-coupled state, a second render in the same frame is not a repeat of the first.
Expect to replay the first pass's skinning data onto the second, shifted into the second
eye's frame if the data is camera-relative, and expect the shadow passes to want the
unshifted data.

## Shared culling

`CONFIRMED`, `Scope: Oblivion` / `Scope: Gamebryo`

Problem: `NiCullingProcess::Process` (`0x0070E0A0`) builds its frustum planes from the
camera's world position, which the dual pass moves by one baseline. A body straddling a
plane is culled in one pass and drawn in the other, and a body drawn only in the second
pass has no first-pass palette to be locked to (measured as second-render uploads with no
pair, 1571 against 1655).

Solution (`e0fc88c`, `render/CullingHook`, `CullingSync.h`): the first pass records the
camera position handed to each `Process` call (up to 64 per frame); the second pass
replays those positions into the camera's world transform for the duration of each
matching call and restores it before returning. The draw itself still uses the moved
camera, because the renderer took its view before the cull (`0x00701970` at `0x0070C0DE`).
Camera-pointer mismatches, replay overflow and capture overflow are counted and logged.

Side effect, stated: objects just outside the second eye's frustum but inside the first's
are drawn and clipped; objects inside the second eye's frustum but outside the first's are
missing from the second eye. At one interpupillary distance this is a sliver at the
extreme edge. `LIKELY` invisible; not measured as a complaint.

## Alternate eye rendering

`CONFIRMED`, `Scope: General VR` (the compositor rule) / `Scope: Oblivion`

`Stereo=aer`: the camera hook offsets the camera to alternating eyes (`IsLeftEyeFrame`),
the frame is drawn once, `EyeMirror::CopyBackBuffer` copies it into that eye's texture at
Present, and **both** textures are submitted every frame - the other eye with its last
picture and with the pose it was drawn from (`kSubmitTextureWithPose`), which is what
tells the compositor to reproject the stale eye correctly (the ghosting the GTA V mod's
author reported on ValveSoftware/openvr issue #1253 is what happens without it).

Three facts established here that travel:

1. **The compositor counts a frame only when both eyes were submitted.** One eye per frame
   is a successful call and not a frame; after about ten such non-frames SteamVR fades to
   Home with no error anywhere.
2. **Submitting from before the frame is drawn hands each eye the other eye's viewpoint.**
   Under AER the back buffer at camera-hook time holds the previous frame, drawn from the
   other eye's position. Inverted disparity reads as an unstable picture that gets worse
   when the head moves sideways. `BackBufferEyeIsLeft` in `FrameLogic.h` is the decision
   that got this wrong once, with its reasoning.
3. **AER "feels deeper"** than dual on the same setup and in other titles, per the
   tester. The extra depth is time: two eyes holding two moments, read as parallax while
   the head moves. Do not tune dual towards it (`2cba14c`).

AER exists because it asks nothing of the engine; it is the fallback if dual misbehaves on
a setup. It costs one full-screen copy per frame and shows a one-frame disparity on fast
motion.

## Projection, frustum and world scale

### The pose the compositor assumes

`CONFIRMED`, `Scope: OpenVR`

"Reprojection corrections are applied based on the poses returned by WaitGetPoses. We
assume you render the frames passed to Submit using the poses returned by the previous
WaitGetPoses. Rendering using other poses will result in incorrect behavior."
(ValveSoftware/openvr issue #518, Valve staff). OBVR once called `WaitGetPoses`, discarded
its poses, asked `GetDeviceToAbsoluteTrackingPose` with zero prediction, and rendered with
that - the wearer reported a world that morphed when the head moved and motion sickness.
The order is now wait, read the render pose, render, submit (`3f0d557`,
`OpenVRBackend::GetRenderPose`).

### Placing a game frame inside an eye frustum

`CONFIRMED`, `Scope: General VR`

An eye texture covers the eye's whole (asymmetric) frustum; the game's frame covers a
different, narrower one. Laying one over the other without arithmetic magnifies and
stretches the world by whatever ratio the two frustums have (measured: 1.61x across,
2.31x down on the first attempt, HUD pushed out of view). `render::PlacePicture` /
`PlacePictureFromTangents` (`EyeGeometry.cpp`) computes the destination rectangle from
the eye's tangents and the game's tangents, centred on the eye's optical axis, cropping
source and destination in the same proportion when the game's view is wider.

Facts settled by a headset that no document settles:

- OpenVR's `GetProjectionRaw` top/bottom signs: the texture's v axis runs opposite to the
  frustum's vertical axis ("top" and "bottom" are named backwards, as Valve's wiki says).
  Reading them at face value put the picture a fifth of the view too low. `OpticalCentreV`
  takes `topIsNegative = true`.
- The optical axes sit off-centre (0.583 across the left eye, 0.424 across the right on
  the test headset); two centring crosses placed there fuse into one when looking straight
  ahead (`OBVR-eye-geometry.log`, 0.0.5).

### Reading Oblivion's own field of view

`CONFIRMED`

`fDefaultFOV` (75) is ambiguous on a wide frame: horizontal at the current aspect (75 x
46.7 at 16:9) or a 4:3 figure with the horizontal widening (91.3 x 60). The two readings
differ by a third in world size with no distortion to notice. The engine's own frustum
settled it: `NiCamera+0xEC` reads l/r = 1.0231, t/b = 0.5755 at 75, which is
tan(37.5) x 0.75 x 16/9 and tan(37.5) x 0.75 - the 4:3 reading (`7d01b04`,
`SetFrustumFov` comment). `GameFovIsFor4x3=1` is the shipped default. The projection
matrix read off the device (`GameProjection`) is the cross-check, with an identity-matrix
guard because a shader-driven renderer may never set the fixed-function projection.

### Writing the frustum per eye

`CONFIRMED` for the write, `Scope: Gamebryo`

`MatchHeadsetFov=1` writes the union of the two eyes' tangents into the NiCamera's
`m_kViewFrustum` (`game::WriteGameCameraFrustum`) every camera pass, so the game renders
the headset's own angle rather than 75 degrees, and `PlacePicture` then has nothing to
crop. The write holds (`0eba115`, `OBVR-camera-frustum-used.log`). It is symmetric, not
per eye: writing an asymmetric per-eye frustum would remove `PlacePicture`'s remaining
geometric compensation and is the natural next step; **not built**. The camera's frustum
differs across a frame (1.0231 when the camera is computed, 1.1188 by Present), which is
why `State::cameraTanHalfWidth` keeps the reading from the camera pass.

`GameFovOverride` writes an angle instead; `fDefaultFOV` itself is not touched because the
setting also resizes the menu layer and moves the mouse mapping.

### Eye separation and world scale

`CONFIRMED`

- Units: 69.99125 Oblivion units per metre (Construction Set wiki and Creation Kit wiki
  agree; `UnitsPerMetre`).
- The interpupillary distance is read once from `GetEyeToHeadTransform` for both eyes and
  converted with `UnitsPerMetre`, never with the head-movement scale: the distance between
  two eyes is a fact, the lean gain a preference, and mixing them shrinks the world.
- `EyeSeparationScale` (0.25..4, default 1.0) is the one sanctioned bend: hyperstereo
  applied to the camera step before rendering ("depth boost", the README's PrimaShock
  reference). Above 1 the world reads proportionally smaller.
- `HeadMovementScale` went 1 -> 2 -> 1.7 -> 2 -> 3 while OBVR was mono (a number that
  keeps climbing is chasing a missing depth cue) and ships at 1.0 since stereo.

### Eye-sized frames and the 2D screen size

`CONFIRMED`, `Scope: Oblivion` (addresses) / `Scope: Legacy games` (the shape of the
problem)

Oblivion's back buffer was 2560x1440 upscaled into 4028x3380 eye textures - a real loss of
sharpness with no way to supersample (SteamVR's slider only grows the target, capped at
4096 wide). `SetGameResolution=1` changes `D3DPRESENT_PARAMETERS` in the `CreateDevice`
hook to the headset's recommended size, clears the fullscreen flag (a square frame is not
a display mode), and **resizes the window itself** - the step that was missing the first
time, when a 3200x3200 back buffer presenting into the 320x240 window the game created
lost the device (`d6c9ab2`, `44c09fa`).

The frame size then split the 2D against itself: films and the main menu laid out at the
size the game believes (its INI), in-game menus over the whole buffer, the mouse between
the two. The route through the engine's `iSize` settings is **banned** (the engine
persists them; see [failed-approaches.md](failed-approaches.md)). The route that works
raises the one process-local copy of the screen size the whole 2D reads
(`0x00B06C4C/50`, written once at window creation, some ninety reads, never persisted),
to a 16:9 window into the frame (`UiFollowsFrameSize`, `UiSizeForFrame`), plus a
per-pass viewport shrink for the interface pass (`DecideInterfaceViewport`) and the two
cursor detours. That is the current state; the mouse now clicks where the buttons are.

## Compositor, DXVK and Vulkan

`CONFIRMED`, `Scope: DXVK` / `Scope: OpenVR`

- OpenVR's `Submit` has no Direct3D 9 texture type, and plain D3D9 cannot share surfaces
  (Microsoft: "Direct3D 9c and older runtimes do not support shared surfaces"). Two
  routes existed: force the game onto D3D9Ex and share into D3D11 (`DEAD END`: Oblivion
  crashes on a 9Ex device, native and DXVK alike, `e7d7e3d`), or let DXVK render in
  Vulkan and submit Vulkan images through its interop interfaces. The second is the route.
- DXVK's `ID3D9VkInteropDevice` (`GetVulkanHandles`, `GetSubmissionQueue`,
  `TransitionTextureLayout`, `FlushRenderingCommands`, `LockSubmissionQueue`/
  `ReleaseSubmissionQueue`) and `ID3D9VkInteropTexture` (`GetVulkanImageInfo`) are upstream,
  in `src/d3d9/d3d9_interfaces.h`; no fork. DXVK asks OpenVR for its required extensions
  by itself (`dxvk_openvr.cpp`); **do not set `DXVK_NO_VR=1`**.
- A submittable image needs `VK_IMAGE_USAGE_SAMPLED_BIT` as well as `TRANSFER_SRC`. In
  DXVK `CreateRenderTarget` makes an attachment-only image without the sampled bit;
  `CreateTexture` with `D3DUSAGE_RENDERTARGET` carries it (read back off the image and
  logged: usage `0x17`). Usage is fixed at creation; there is no transition that repairs it.
- The submit sequence (`InteropBracket`): `StretchRect` first with nothing held (it is
  queue work), then read layouts, flush, lock the queue, transition both images to
  `TRANSFER_SRC_OPTIMAL`, submit left and right, transition back under the lock, release.
  Getting the undo half wrong freezes the game with an empty log.
- **`WaitGetPoses` is queue work too** in the default timing mode (Valve's Vulkan wiki:
  "the following functions may also access the queue: ... WaitGetPoses"), and Vulkan
  allows one thread per `VkQueue`. It runs under the same lock since `6f4ccde`. The
  alternative Valve offers, explicit timing mode with `PostPresentHandoff`, is **not** used.
- Frame pacing: WaitGetPoses blocks until the compositor wants the next frame, so the game
  runs at the headset's rate when it can, and without focus the call throttles to 10 Hz;
  `SubmitPolicy` gives up rendering (not the frame rate) after ninety consecutive
  recoverable failures or at once for unrecoverable ones. Measured in the 0.0.5 run: frame
  times were Oblivion's own (17.8 to 18.5 ms) with and without submission.
- `Submit` is given no bounds under dual pass: each texture already covers exactly that
  eye's frustum.

## Render targets and per-eye resources

`CONFIRMED`

- `EyeMirror` owns two `IDirect3DTexture9` at the recommended eye size, in the back
  buffer's format (asked, not assumed - `StretchRect` between formats is a conversion the
  runtime may refuse), blacked out once with `ColorFill`, each with its interop texture,
  destination and source rectangles, a smaller centred `flatDestination` for cinema
  pictures, and a `commonDestination` (the window both eyes show) for the held-pair
  border trim.
- `HudLayer` owns one A8R8G8B8 texture at the frame's size for the 2D layer, plus the
  overlay. `CrosshairLayer` and `LaserLayer` own their overlays; the settings menu has its
  own layer.
- The first `CopyBackBuffer` fills both eyes so the eye not drawn first never shows an
  uninitialised target.
- `MenuShade`: the held or live pair is desaturated and re-toned sepia through a ps_2_0
  shader assembled at runtime through `d3dx9_27.dll` (which Oblivion imports and therefore
  ships), the way vanilla's static menu background looks; a flat brown tint was tried
  first and looked nothing like the game.

## Per-eye state leakage

`CONFIRMED` for the cases found

- Device state between the passes was suspected and acquitted: a full `D3DSBT_ALL` state
  block restore between the renders changed nothing about the collapse and turned every
  menu's text red (`02116d5`, `DEAD END`).
- The moment between the passes (eye copy, 2D capture) sets device state the second
  render inherits; the 2D capture saves and restores the five blend/write states it
  changes (`HudLayer::BeginCapture`/`EndCapture`).
- The second render leaves the interface pass unable to draw (the pass walks past every
  open gate and draws nothing). Not explained; worked around by drawing the layer between
  the passes. See [open-questions-and-known-issues.md](open-questions-and-known-issues.md).
- Water reflection resets with head movement in mono as well; not a leak.

## Foliage, LOD, sky, post-processing

- Sky and LOD roots: `kRenderScene` re-reads the camera node's position each call to place
  them, so the second pass keeps them consistent for free. `CONFIRMED`
- SpeedTree billboards turning with the view: `LIKELY` present, not chased (HANDOFF "The
  first HUD run").
- HDR tone mapping (`0x007B48E0`, called once inside `kRenderScene`) runs per pass; each
  eye is tone-mapped on its own. Whether adaptation state advances per walk: `UNKNOWN`.
- Near plane: the frustum's `n` reads 10 units (14 cm); nothing changes it. Weapon and arm
  clipping in first person at that distance: not reported.
