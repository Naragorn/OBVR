# VR porting playbook

A procedure for bringing another flat game into VR, written from what OBVR actually went
through. Every phase names the OBVR experience it rests on; where OBVR has no experience
the step is marked as such. Scope: `Legacy games` unless noted.

## Phase 0 - decide the shape before writing code

- **Bitness decides the runtime.** A 32-bit game needs a 32-bit VR client library.
  OpenVR ships one and is out-of-process; OpenXR runtimes vary (SteamVR: beta since
  2026-06). The alternatives are a 64-bit helper process with shared memory (FEAR2VR) or
  staying on OpenVR. Decide this first; it shapes everything.
- **Graphics API decides the frame handoff.** D3D9 has no route into any compositor
  without either DXVK's interop (Vulkan images) or a D3D9Ex-to-D3D11 shared surface;
  test the 9Ex tolerance of the engine on the binary before choosing it (Oblivion did
  not tolerate it). D3D11/12 submit directly.
- **Choose the stereo model** from the taxonomy in
  [rendering-and-stereo.md](rendering-and-stereo.md): engine-hook geometry stereo is the
  target; AER is the fallback that asks nothing of the engine; depth reconstruction is the
  last resort.
- **Decide the first scope narrowly** (OBVR: seated, no hands, 3DoF first, then 6DoF, then
  stereo) so each run answers one question.
- **Set the evidence standard now**: two sources per address, verify bytes before every
  patch, a log that says what was done, pure decision functions with tests.

## Phase 1 - reconnaissance

1. **Engine and version.** Read the version resource; refuse every other version at
   load. Find the script extender (or write an injector) and its headers: they are the
   first source for names.
2. **Disassemble the binary, not somebody's fork.** Confirm `.text` is readable (Steam's
   stub did not encrypt Oblivion's). Map VA to file offset.
3. **Find the camera write.** Look for the function that writes a 3x3 rotation and a
   position into a scene-graph node (a `rep movsd` of 9 dwords after three stores was the
   tell in Oblivion), and what runs right after it (a downward transform update means
   *local* is the field to write).
4. **Find the frame render entry.** In Gamebryo it is one function calling the scene
   render twice (world, first person), the post-processing once, and re-reading the camera
   node; other engines (LithTech: `SetupPassPerspective`; id Tech-derived: `R_SetViewParms`)
   have an equivalent "the function that sets up a view". Prefer an entry with relocatable
   prologue bytes.
5. **Find the frame delta** the render walk's controllers read, and the update step that
   advances the simulation. Establish whether the update step runs before the render.
6. **Find the 2D pass** and whether it sets its own render target group.
7. **Find the projection.** Read it two ways (the engine's camera struct and the API's
   matrix) and make them agree before trusting either. Establish what the FOV setting
   means at a non-4:3 aspect.
8. **Find the D3D device** (a renderer global) and confirm what implements it (native,
   DXVK) with a `QueryInterface`.
9. **Determine how the game treats a window** it did not size and a back buffer size it
   did not choose, and whether it persists its resolution settings (Oblivion writes them
   back to the INI on its own).
10. **Determine what pauses the world** and whether the render stops with it (Oblivion:
    a snapshot behind pause menus; live behind dialogues).
11. **Determine whether rendering mutates state**: a second render with the clock zeroed
    and draw counters per pass, before any stereo work.

## Phase 2 - minimal runtime integration

1. Load the runtime library at runtime; fail soft, log why; the vanilla game must survive
   a machine without VR.
2. Register as a background application first (poses only); go scene-application only
   when you are ready to own the display.
3. Read the head pose; convert with one change-of-basis function every source goes
   through; verify roll and pitch separately in the game with fixed test angles.
4. Inject head rotation into the camera write site; verify the character does not turn
   with the head; add recenter (yaw-only reference) with rising-edge key detection.
5. Add position (6DoF) with the reference position taken on the first valid pose and a
   lean sphere; log the raw and clamped lean.
6. Validate latency and orientation with the wearer; write down what they said and what
   the log said, separately.

## Phase 3 - a picture in the headset, then the game's picture

1. First a generated test pattern through the compositor: a border, an inset frame,
   coloured markers per eye, a ramp, a centring cross on the *measured* optical axis.
   This separates "talking to the compositor" from "getting the game's pixels out".
2. Establish the frame loop: wait for poses, render with those poses, submit at the end
   of the frame (Present), never before it is drawn; handle focus loss and give up
   rendering rather than the frame rate.
3. Get the game's frame: for D3D9 through DXVK interop (flush, lock the queue, transition,
   submit, transition back, release; one RAII owner), including the wait for poses under
   the same lock.
4. Place the mono picture inside each eye's frustum with the arithmetic, not a fraction;
   read the vertical convention off a headset.

## Phase 4 - stereo

1. Try AER first if the engine can be moved per frame: it proves the eye offsets and the
   compositor rules cheaply (both eyes every frame; tell the compositor the pose each eye
   was drawn with).
2. Then the dual pass: detour the frame render, step the camera between two calls with
   the engine's own transform update, capture each eye after its pass, zero the frame
   delta across the second, submit both with no explicit pose.
3. Expect the second render to disagree with the first in exactly the places the engine
   caches per frame: skinning, look-at, face meshes, culling sets, temporal state. Build
   counters per pass before building fixes. Replay or share what diverges.
4. Write the headset's frustum into the engine's camera so the game renders the eye's
   angle; keep the placement arithmetic for the fallback.
5. Get the frame eye-sized at device creation (buffer, mode, window - all three), then
   chase the 2D coordinate split it opens.
6. Validate world scale (units per metre), IPD (plausibility flag in the log), and
   hyperstereo as a separate knob from lean gain.

## Phase 5 - rendering fixes to expect

From OBVR, and from the prior art (see [ecosystem-and-prior-art.md](ecosystem-and-prior-art.md)):

- Skinned bodies collapsed or stale in the second eye (OBVR, BioShock VR): replay the
  first pass's skeleton data, shifted if camera-relative; exclude shadow passes.
- Bodies at the frustum edge drawn in one eye only: share the culling set.
- Face meshes built once per frame: open in OBVR.
- Shadows, reflections, water: usually camera-free sub-passes; do not shift them; the
  water reflection may reset per camera move regardless.
- Billboards (SpeedTree) face one camera.
- Screen-space and temporal effects (TAA, SSR, SSAO, motion blur): per-eye divergence and
  ghosting under AER; disable or run per eye. Oblivion has none of the temporal ones.
- Post-processing runs per pass; check adaptation state.
- Near-plane clipping of the first-person model at close head positions.

## Phase 6 - gameplay adaptation

- Separate head, body and aim: the body's heading is one field with two jobs in Bethesda
  games; set the aim inside the engine call that reads it and restore it on the way out,
  rather than turning the body in time. Find that call from the stack of a site that is
  certainly reached.
- Turn the drawn weapon (a picture) at render time; nothing is fired along it.
- Third person: the chase camera eases at the physics rate; measure the rate off the
  camera; correct the picture by the camera's share of the aim, not the aim.
- The activation ray: replace its direction, keep the engine's safe origin.
- The crosshair: lift the game's own out of the 2D layer and hang it at the depth of the
  activation target along the view axis, eased; hide it when it is of no use.
- The HUD and menus: intercept the 2D pass, fix alpha, hang it as an overlay; decide per
  frame between stereo world, a held reprojected pair, and a cinema screen from what the
  engine actually did; dress paused worlds so the pause reads.
- Dialogue cameras and zooms: cut the transition, keep the POV dance.
- Menus that must keep the world moving (a persuasion face): supply the camera from
  inside the render when the camera update does not run.

## Phase 7 - comfort and polish

- Recenter on a key that vanilla leaves unbound; the same key re-anchors flat pictures
  and room-anchored overlays on frames the camera hook does not run.
- Level the world reference to yaw only; decide deliberately whether flat anchors keep
  pitch.
- Take the vertical look off the stick; turn it into height in third person, nothing in
  first; offer smooth turning.
- Seated first; standing needs a tracking-space decision that every pose and overlay
  shares (declare it; do not inherit the default).
- Hot-reload every setting and put the ones people touch in an in-headset menu; a
  first-start walkthrough for the two or three choices that matter.
- A watchdog thread that names the last step when frames stop; step marks around every
  call that can block for ever.
- Refuse to patch a site another mod patched first, and name the mod in the log.

## What OBVR has no experience of (do not take this playbook as evidence there)

Motion controllers in a headset (built, unseen), room-scale locomotion, snap turn,
OpenXR sessions and swapchains, D3D11/12 games, 64-bit games, engines with temporal
anti-aliasing, multiplayer.
