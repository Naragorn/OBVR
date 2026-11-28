# A mirror on the tablet (feasibility and plan, 2026-11-28; stage 1 built, section 11; stage 2 built, section 12; stage 3 and the steady picture built, section 13)

The wish (Nadi, 2026-11-28): "neues feature: im inv egal wie eingestellt, gibts oben am ende ein
neuen tab: spiegel, das gibt mir ein echtes spiegel erlebnis und ich kann mich selber ansehen und
den hintergrund wie bei einem echten spiegel, vll mit leichtem dust shader drüber, whl durch ne
kamera."

In English: on the tablet (the inventory held in the hand, `docs/inventory-tablet-spec.md`),
whatever its settings, one more tab at the end of the tabs at the top: Mirror. It shows the player
and what is behind them as a real mirror would, perhaps with a light dust layer on the glass.

Labels, as in the other specs: [R] read in OBVR's code or in the EXE's decompile
(`../OBNG/re/export/rea-all/functions/0x<address>.c`), [M] measured (with the run named),
[D] derived from those, [G] guessed. Every cost number below that is not [M] is a guess until
stage 2 measures it.

## 1. Verdict

**Feasible, large in total, in five stages.** A true mirror needs two more world renders a frame
(one for each eye) while the mirror is on screen. The engine already has the call that renders the
world into a texture of its own, and OBVR already runs the engine's world render a second time with
the camera moved (the dual pass, the menu-world probe). The parts that are new are the mirror's
camera, the per-pass switches (the player's head shown, the clock held, the bone palettes rebased),
and the mirror page on the tablet.

The frame cost is the open question. My estimate is 2 to 4.4 ms of CPU a frame while the mirror is
shown, and nothing while it is not [G, from the [M] pass costs in section 5]. The first world-render
stage measures it before the rest is built.

## 2. What exists and is reused [R]

| Piece | Where | Use for the mirror |
| --- | --- | --- |
| The tablet drawn into each eye as textured pieces, against the game's depth, per eye, from the eye's capture | `render/TabletQuad.h/.cpp` (`DrawIntoWorld`, `TabletQuadPiece`, `kTabletPieces` = 14), called from `HeadsetRenderer::CaptureEye` | the mirror page is one more piece, its texture the eye's own mirror picture |
| The tabs at the page's top edge (page, figure) and the click on them | `vr/TabletLogic.h` `SetFigureTab`, `TabletTabAt`, `TabletTabState`/`StepTabletTab`; `vr/MenuPlace.h` `StepTabletTabs`; `vr/HandMode.cpp:1010` | grows to three tabs; today the strip exists only with `[Tablet] Figure=tab` |
| The game's paper decoded on the CPU and kept as a managed texture, made at the main menu | `vr/TabletPaper.h`, `TabletQuad::Prepare` (13.4 ms once, section 7 of the tablet spec [M]) | the same way for the dust texture |
| Each eye's camera and both controllers' poses in the game's world, per eye | `render/ControllerModels.h` `WorldView` (`eyePos`, `eyeRot`, `hands[]`, `unitsPerMetre` 70) | the mirror's plane (the tablet's corners) and the eyes to reflect in it |
| A second engine world render with the camera moved and the frame clock zeroed | `render/SceneRenderHook.cpp` `HookedRenderScene` (dual pass, lines 429-565) and `RunMenuWorldProbe` (lines 669-707: calls the trampoline with no OBVR callbacks) | the mirror passes are the same kind of call |
| The bone lock: a second render's palettes replaced by the first's, shifted by the camera's move | `render/BoneRebase.h` (palettes are camera-relative, rotation the same in both renders) | the mirror passes need it, with the shift between the eye's camera and the mirror's |
| Culling capture and replay between two renders | `render/CullingSync.h` (`BeginCullingCapture`, `BeginCullingReplay`) | the second mirror eye can replay the first mirror eye's set |
| The actor shadow maps kept for a second render | `render/ShadowShareHook.h` (`[Render] ShareActorShadows`) | the mirror passes keep the eye pass's maps |
| The water reflection rendered once and reused by the second eye | `render/WaterReflectionHook.cpp` (`ShouldRenderWaterReflection`, `NoteWaterReflectionReused`) | the mirror passes reuse it too |
| The head hidden for the draw: Bip01 Head's world transform at scale 0, and the FaceGen node's hidden bit for the world render | `game/PlayerBody.cpp` `HideHeadForThisDraw` (`CollapseForThisDraw`), `game/HeadCullLogic.h`, `[Body] HideHead`/`CullHiddenHead` (both default on, `core/Config.h:213-217`) | undone for the mirror passes only |
| The profiler's spans per pass | `perf::Profiler`, `EventType::ScenePass` | one more span type for the mirror passes |

Naming: OBVR already has `render/EyeMirror` (the eye copy and the monitor window), `game/BoneMirror`
and `vr/SheathMirror` (left-handed play). The new code should carry another word, for example
`MirrorGlass`, so a search for "mirror" does not mix them up.

## 3. The engine side [R]

### 3.1 The world into a texture

- `OSGlobals::RenderScene` 0x0040C830 (`__thiscall`, one argument, a `BSRenderedTexture*` or 0;
  OBVR's `kRenderScene`, `game/GameAddresses.h:1045`) is the world render. Its order
  (`0x40c830.c`; `../OBNG/knowledge/culling-and-draw-setup.md` "The scene pass as a boundary"):
  the water pre-pass `0x0049E880` (outdoors, or a cell with water), the actor shadow maps
  `0x004073D0` (always), the first-person node `0x00660110(1)` hidden for the main scene, the
  target chosen, the main scene through NiDrawScene `0x0070C0B0(camera, scene, cullingProcess, 0)`,
  a first-person pass with a swapped accumulator, image space `0x007B48E0`, release.
- With a texture argument the engine draws into its own texture: `0x0040D160` makes a texture of
  kind 5 (`0x007C23C0(renderer, 5)` into `DAT_00B333E8`), sets the main camera's port
  (`[0x00B333CC] + 0xDC`, +0x110..+0x11C) to the texture's share of the current target, calls
  `0x0040C830(texture)` and resets the port to 0, 1, 1, 0. That is the engine's own recipe for
  "the world in a smaller texture" (`0x40d160.c` lines 38-124).
- Inside `0x0040C830`, the texture argument decides the end (lines 105-113, 310-343):
  - with image space on (byte `0x00B42F3E`), the world goes into a frame-sized texture of kind 4
    and image space writes the result into the argument (`0x007B48E0(renderer, frameTexture,
    argument)`);
  - with it off and a multisampled target (`1 < DAT_00B34FC0`), the world is drawn into the
    current target and copied into the argument's surface through the device's vtable +0x88,
    which is `IDirect3DDevice9::StretchRect` [D: slot 34 of the interface];
  - with it off and one sample, the argument's target is set (`0x007D7280(7, ...)`); what that
    call does is not read.
  - [D] On the multisampled route without image space the world is drawn into the back buffer
    first, so a mirror render on that route destroys whatever eye picture is still there. On the
    image-space route the back buffer is not the target [D, not checked].
  - **Corrected by stage 2's reading (section 12):** with image space on and antialiasing on (the
    owner's route) the world is drawn into the back buffer too, and image space then maps the
    whole frame into the argument. The image-space route leaves the back buffer alone only with
    one sample.
- A texture of any size: `0x007C2420(renderer, size, format, usage, x)` makes a square one through
  `0x007C1B50(renderer, width, height, format, usage, x)` (`../OBNG/knowledge/water-reflections.md`).

### 3.2 The water reflection (the engine's own planar reflection)

`WaterManager::RenderReflections` 0x0049BEF0, called from `0x0049E880` inside the world render
(`0x49e880.c:106`). Mirrored camera about z = the water's height, a 256 x 256 map (OBVR's
`[Render] WaterReflectionMapSize`), the clip plane carried into clip space with
`D3DXPlaneTransform` and `D3DRS_CLIPPLANEENABLE`, and a culling process of its own
(`TESWaterCullingProcess` 0x0049CBF0) that drops statics, actors, trees and items unless the INI
asks. It hides the first-person node (`0x00660110`) and the outer ring of cells for the pass. All of
it is in `../OBNG/knowledge/water-reflections.md`. OBNG measured the pass at 6-13 draws a frame
(`culling-and-draw-setup.md` "The reflection pass takes the eye pass's finish") [M]: it is cheap
because it leaves almost everything out.

What it teaches for the mirror [D]:
- The engine can render the scene from a second camera into its own target in the same frame. The
  entry it uses is not exposed as a function (it builds the camera, culls and draws inline), so
  calling it with another plane is not an option. `0x0040C830(texture)` is the reusable entry.
- The water's culling cannot be borrowed: a mirror has to show actors, statics and the player.
- A mirror camera is built the way the water's is: a proper rotation (no mirrored axes), so the
  triangles keep their winding, and the picture flipped left to right where it is shown.

### 3.3 Why the dual pass's tools are needed

- The bone palettes are camera-relative, and a second render of the frame takes some rows from the
  first unchanged (`render/BoneRebase.h`, the 2026-08 collapse). A mirror render after the eye's is
  a second render. Its rows need the rebase with the shift between the two cameras. The rotation
  part is world-relative and the same in both [R comment in BoneRebase.h], so the rebase should
  hold for a rotated mirror camera too [D, not tested].
- Time-driven updates inside the walk advance by the frame delta. The dual pass zeroes it
  (`kFrameSecondsAddress` 0x00B33E9C) for the second render, and the mirror passes must too.

## 4. The player in the mirror [R/D]

What the eye pass draws today:
- The third-person body under the headset (`game::ShowPlayerBody`), with the head collapsed to a
  point (`CollapseForThisDraw`: the world transform recomputed at scale 0, the local scale put
  back) and, with `CullHiddenHead`, the FaceGen node hidden for the world render; the bit is
  cleared again in `afterWorldRender` (`game/HeadCullLogic.h`).
- With the body IK (default on) the body's arms reach the pinned hands. The hands the player sees
  are the first-person arms laid onto the body's (`docs/player-body-spec.md` "The arms"); the body's
  own hand models and what hangs on them are hidden.

What the mirror pass must change, and only for its own render:
- **The head, face, hair and helmet.** Recompute Bip01 Head's world transforms at its real scale
  (`UpdateNodeTransforms(head)`) and take the FaceGen hidden bit off before the mirror render. Put
  the collapse back after it, if any eye render still follows. The helmet is skinned to the head's
  bones and the hair is a FaceGen part, so both come back with these two steps [D]. The cost is
  one subtree update a mirror pass [G: small, unmeasured].
- **The step back under a bowed head.** The body steps back when the head looks down by more than
  20 degrees, up to 8 units (11 cm) looking straight down at the default near plane
  (`game/BodyPlacement.h` `kLookDownStartDegrees`, `LookDownBackUnitsFor`). In the mirror, a head
  bowed over a low tablet would sit slightly in front of its neck. With a hand mirror held at face
  height the head is level and the shift is 0 [D]. Proposed: leave it, and check it in the headset.
- **The hands.** The first-person pass inside `0x0040C830` draws the first-person node from the
  mirror's camera, so the hands are where they are in the world [D]. Whether that pass clears the
  depth first is not read. If it does, the hands draw over the body in the mirror. A hand in
  front of the body from the glass's side looks right either way; a hand behind the body would not
  [G].
- **What the mirror does not show.** OBVR's own pieces (the tablet itself, the controller models,
  the laser, the hand HUD) are drawn into the eye pictures, not into the world, so they are not in
  the mirror. Real hand mirrors do not show themselves either; the hand holding it shows at the
  glass's edge.
- **`[Body] Visible=0`.** No body in the world, so the mirror shows only the hands and the room.
  That is the owner's decision (section 9).

## 5. Optics and frame cost

### 5.1 What a true mirror needs [D; the method from the sources below]

For each eye: the eye's position reflected in the glass's plane; a camera there whose forward is
the glass's normal (into the mirrored world) and whose up is the glass's up; an off-axis frustum
through the glass's four corners (`NiFrustum` l, r, t, b as tangents, as OBVR's water code writes
them); the near plane at the glass. Because the image plane is parallel to the glass, the picture
maps onto the page with plain texture coordinates (u flipped), and the near plane is the clip
plane, so nothing behind the tablet in the real world shows in the mirror. No oblique clip plane is
needed. Sources: Kooima, "Generalized Perspective Projection" (cited in Wikibooks, Cg
Programming/Unity/Projection for Virtual Reality); Arm's Unity VR guide, "Stereo reflections":
"the reflection must be computed individually for each eye. If both eyes are shown the same
reflection, users quickly notice that there is no depth in the reflections"
(support.arm.com/documentation/102117/0100/Stereo-reflections).

The glass is the tablet's page: its corners are known each frame in the holding controller's frame
(`TabletQuadPiece::corners`), and the controller's pose in the world is in `WorldView::hands`. The
mirror passes must use the same pose the page is drawn with in that frame. Otherwise the
reflection slides on the glass by the pose's change between the two reads [D].

When to skip a frame's passes: the mirror tab is not chosen, the tablet is down, the eye is behind
the glass's plane, the glass is edge-on, or the page is outside the eye's view. Then nothing is
rendered and the page shows the last picture or plain glass [D]. This is what keeps the cost at
zero most of the time.

### 5.2 What a pass costs today [M]

| Run | What | Per pass | Per frame |
| --- | --- | --- | --- |
| single-pass-gold-road-off/20261104-153026 | dual pass, outdoors, Gold Road | 2.40 ms | 4.80 ms |
| single-pass-gold-road-on/20261104-152647 | single pass (default since 2026-11-12) | - | 3.26 ms |
| single-pass-measure-off/20261104-144236, -150032 | dual, the Walkers scene | 2.94-3.23 ms | 5.88-6.46 ms |
| single-pass-measure-on/20261104-145734, -150318 | single, the Walkers scene | - | 4.19-4.26 ms |
| performance-spec.md section 0 | a pass empty / with five people, instruments off | 0.72 / 2.85 ms | - |

All from `docs/single-pass-stereo-spec.md` and `docs/performance-spec.md`. These scenes ran at
75 Hz, held at the vsync with 4.6-7.9 ms of `poses_wait` (idle) left. A frame is 11.1 ms at 90 Hz
and 13.9 ms at 72 Hz. Nadi's heavy Windows places are GPU-bound already, at 15-19 ms of GPU in a
25 ms frame (`docs/dynamic-resolution.md`).

### 5.3 The mirror's pass, estimated [G]

- A mirror pass sees a cone of about 46 degrees (a 0.34 m page at 0.4 m), against the eye's
  100 degrees or more, so it culls more and draws less. Its fixed share stays: the shadow maps if
  not shared, the walk, the first-person pass, image space. Guess: 40-70 % of an eye pass, so
  1.0-2.2 ms. Both eyes: **2.0-4.4 ms a frame.** The second eye replaying the first's culling
  takes some of that back (unmeasured).
- GPU: two pictures of about 1024 x 1088 are 2.2 Mpx against the two Dream Air eyes' 27 Mpx, about
  8 % more pixels. Harness runs showed the GPU time following the draw count more than the pixels
  (`docs/dynamic-resolution.md` [M]), so the draws are the GPU's cost too.
- Sharpness: the page at 0.4 m covers about 46 degrees; the Dream Air's 4036 px give roughly
  35-40 px a degree [G: the lens' spread not read], so about 1700 px across. 1024 is a little softer
  than the world; 1536 matches it at about twice the pixels.

## 6. Options

| | What the player sees | Built new | Risk | Frame cost while shown | Test cost |
| --- | --- | --- | --- | --- | --- |
| A. The menu figure, mirrored and turned to the head | the inventory's paperdoll (a copy of the player in its idle, `menu-paperdoll.md`) on the page, flipped; no room behind, not the real hands or head | a flip and a turn of the figure's holder (manager +0x60, OBVR moves it already) | low; it does not show the player's real pose | ~0 | unit + harness; small |
| B. A selfie screen | one camera at the tablet looking back, a flat picture like a video call; does not change as the head moves relative to the tablet | one extra world pass, a symmetric frustum | the engine route (3.1), the palettes, the head | +1.0-2.2 ms [G] | medium |
| C. A cyclopean mirror | the reflection from between the eyes, the same picture in both eyes: right parallax as the head moves, no depth, reads as a screen showing a mirror | B plus the off-axis frustum | as B | +1.0-2.2 ms [G] | medium |
| **D. A true mirror, each eye every frame (recommended)** | a real mirror: depth in the reflection, the room behind, the player's face, hair, helmet and hands as they are | C for each eye; per-eye textures on the page; culling replay for the second eye | as B, and the cost doubled | +2.0-4.4 ms [G] | large: unit, harness (eye dumps, film), measurement run, headset |
| E. D with the eyes taking turns | each eye's reflection renewed every other frame (36-45 Hz) | D plus a scheduler | the reflection may jitter or trail when the tablet moves (headset) | +1.0-2.2 ms [G] | D plus a headset check |
| F. D through OBNG's draw-twice | as D, both mirror eyes in one walk | OBNG's eyes API called inside a nested render; one frustum covering both glasses and projective texture lookups | OBNG work; unknown whether it can run inside a texture render | maybe +1.3-3 ms [G] | huge |

**Recommended: D.** It is the only option that gives what was asked: a real mirror with depth,
the room behind and the player's own face. A and B look like a picture on a screen, and C's flat
reflection is the fault Arm's guide warns about. D costs one more extra world pass a frame than
C. That cost is paid only while the mirror tab is shown and the glass is in view. Short glances
at oneself are the use, so a slower frame then is easier to accept than a flat mirror. E stays as
the fallback setting in case stage 2's numbers say D does not fit. F is the optimization for later
if the cost hurts.

## 7. Design of D

- **The tab strip in every mode.** `[Tablet] Figure=tab`: page, figure, mirror. `beside` and
  `off`: page, mirror (the strip that is now only in the `tab` mode stands on the page's top edge in
  every mode). The tab state becomes an index (page, figure, mirror) in place of `showFigure`. A
  popup over the page shows the page, as now. A tap on the mirror's tab reaches nothing of the
  game's. The right stick's page flick leaves the mirror for the next page (proposed).
- **The mirror page.** The page's rectangle on the face. Pieces per eye: the glass (the eye's
  mirror texture, u flipped), the dust (below), the paper frame around it. `kTabletPieces` grows
  from 14.
- **The passes.** Inside `HookedRenderScene`, after the frame's eye render, through the trampoline
  as `RunMenuWorldProbe` does (no OBVR callbacks, no scene-call tick): for each eye, the mirror
  camera written into the main camera (transform and frustum, and the port per the engine's own
  recipe of `0x0040D160`), the clock zeroed, the bone lock in replace mode with the mirror's
  shift, the water reflection reused, the actor shadows shared, the head shown, then
  `0x0040C830(mirror texture)`. Everything put back after.
- **When in the frame.** Best: between the eye render and the eye captures, so the reflection is
  this frame's. That only works on routes where the mirror's render leaves the back buffer alone
  (3.1). Where it does not (multisampled, no image space [D]), the passes run after both captures.
  The reflection is then a frame late on the page, as the tablet's 2D content already is
  (tablet spec section 7). Stage 2 decides per route.
- **Size.** One texture per eye, 1024 a side to start (`[Tablet] MirrorSize`). The port is cut
  to the page's size in the eye that frame, so a far tablet costs fewer pixels.
- **The dust.** A texture made once on the CPU at the main menu, like the paper (no file to ship,
  no shader): sparse specks and a few faint smudges, premultiplied, 5-12 % alpha, drawn as one
  blended piece over the glass. Also a slight darkening (a real mirror reflects about 90 %) through
  the piece's tint. The tablet's pieces already blend premultiplied, so no engine shader work is
  needed. `[Tablet] MirrorDust` on or off.
- **`[Tablet] InScene=0`.** The overlay cannot hold a per-eye picture the way the scene pieces do,
  so the mirror page is drawn in the scene in that mode too. The other pages stay on the overlay.
- **The instrument.** A profiler span per mirror pass, and a log line when the mirror closes:
  passes rendered, skipped and why, mean and worst ms.

## 8. Stages

Each stage: pure logic with every flow in a unit test first, then the wiring, then a harness
scenario on Linux (`--headless --no-steamvr --parallel`, `--exclusive` for the timings), then the
headset.

| Stage | What | Cost | Where the risk sits | Test | Model and effort |
| --- | --- | --- | --- | --- | --- |
| 1 | The tab strip in every Figure mode with the Mirror tab; the mirror page as dark glass with the dust and the frame (no world render yet); the tab state as an index | small | none in the engine; the tab's look in the headset | `tablet_test` (three tabs in each mode, the tab step, a popup), `menu_place_test`; harness `tablet-mirror-tab.txt` (tap each tab, eye dumps) | Sonnet 5.5 medium: ordinary code on known patterns |
| 2 | One mirror pass a frame (the left eye's reflection, shown to both eyes for now) into an engine texture: `vr/MirrorGlassLogic.h` (the reflected eye, the camera, the off-axis frustum, every refusal), the route of 3.1, the clock, the bone rebase, water reuse, shared shadows, the profiler span | large | the engine: which target each route writes (3.1), the palettes in a third render, the first-person pass's depth, the port on the single-pass frame | `mirror_glass_test` (reflection, frustum, behind the glass, edge-on, out of view, degenerate corners); harness `tablet-mirror.txt` (eye dumps: the room behind the player in the glass; with and without image space; MSAA 0 and 4); `tablet-mirror-perf.txt --exclusive` in the Walkers scene and on the Gold Road, mirror on against off | Opus 5.5 xhigh: engine unknowns with hard reasoning; Fable 5.2 high if Opus stalls on the route |
| 3 | The player in the mirror: the head, face, hair and helmet for the mirror pass only, the collapse back after it | medium | that a world-transform update between renders is what the skinned head draws with [D]; the step back under a bowed head | `head_cull_test` and `body_*` flows for the per-pass switch; harness `tablet-mirror-self.txt` (the face in the glass's eye dump, the eye pass still headless); a film (`--record`) of the head turning | Opus 5.5 high: engine state across passes |
| 4 | Per eye: the second mirror pass, the culling replay from the first, each eye's own texture on its page; E (eyes taking turns) as a setting if stage 2's numbers ask for it | medium | the cost (measured here); the culling replay from a widened frustum | `mirror_glass_test` (both eyes, the turn-taking schedule); the perf scenario again; disparity of the two glasses' eye dumps (as `tools/disparity.ps1` reads the eyes) | Opus 5.5 high |
| 5 | The trims and the words: skip when not seen, the port cut to the page's size, the settings rows and help lines in player words, the log line | small | none | unit flows; the perf scenario with the tablet turned away (cost 0) | Sonnet 5.5 medium |
| 6 | The headset | one session | the feel: depth, a trailing reflection, the dust, sharpness at 1024, the frame rate in a busy place | Nadi | - |

Total: large. Stage 2 carries most of the risk and settles it: if its pass costs more than about
2.5 ms in the Walkers scene, D becomes E before stage 4.

## 9. Decisions that are the owner's

1. **True per-eye mirror (D) or a screen (B/C)?** Recommended: D. It is what "echtes Spiegel
   Erlebnis" means, and the screens look flat.
2. **Both eyes every frame, or taking turns (E)?** Recommended: every frame, decided again on stage
   2's numbers.
3. **The mirror's sharpness:** 1024 a side (recommended to start), 768 cheaper, 1536 as sharp as
   the world.
4. **How far the mirror shows:** the full view distance (recommended; measure first) or a short far
   plane for speed.
5. **The world keeps running** under the tablet as it does now, and pauses for a message box or a
   book. The mirror then shows the popup's page. Recommended: as now.
6. **Dust on by default?** Recommended: on, light, with a switch.
7. **The tab's name and place:** "Mirror", the last tab at the top, with a small drawn glass as its
   picture. A live picture on the tab would cost the passes all the time, so no.
8. **`[Tablet] Enabled=0` (the traditional inventory):** no tablet, no tabs. Recommended: no mirror
   there. A floating mirror before the menu would be a separate piece of work (medium).
9. **`[Tablet] InScene=0`:** recommended: the mirror page drawn in the scene anyway.
10. **`[Body] Visible=0`:** recommended: the mirror shows what is in the world (hands, no body).
    Showing the body for the mirror only means placing it every frame the mirror is up (medium).
11. **Heavy places already over budget:** accept the slower frame while looking in the mirror, or
    drop to E or a smaller picture by itself there. Recommended: drop by itself, through the
    same measurement dynamic resolution uses.

## 10. Not verified

Stage 2 read the engine side of the first five points; section 12 says what was read and what
stays open. Before stage 2:

- The mirror pass's cost: every number in 5.3 is a guess.
- Which target `0x0040C830(texture)` draws into on each route, and whether it leaves the back
  buffer alone (3.1). `0x007D7280` and `0x007D6FE0` are not read. *(Read in stage 2: 12.2.)*
- Whether the bone rebase holds for a third render from a rotated camera (3.3). *(Derived in
  stage 2, not seen: 12.2.)*
- Whether the first-person pass clears the depth (section 4). *(Read: it does, 12.2.)*
- Whether OBNG's single-pass draw stays out of a texture render when no eyes are set (the eyes are
  set only around the main render in `HandOverEyes`; [D] that it then draws once). *(Read: 12.2.)*
- Whether the actor shadow share and the water reuse can be triggered from a pass that is not the
  dual pass's second eye (both hooks key on the dual pass's state today). *(Built for the shadows;
  the water is rendered again: 12.2.)*
- The px-per-degree of the Dream Air's lens.
- The headset feel of all of it.
- Mobbin was not checked for the tab's look: the Mobbin MCP is not authenticated in this session.
  The tab reuses the tablet's existing tab style, so there is no new layout to compare.

## 11. Stage 1 as built (2026-11-28, branch mirror-1128)

Asked and approved by Nadi, 2026-11-28: stages 1 and 2; the recommendations of section 9 stand
(name "Mirror", the last tab, a small drawn glass, no live picture, dust on by default and light,
no mirror with the traditional inventory). Stage 2 is built since (section 12). What stage 1 built is the tab, the page
and the way in for the reflection.

**What the tab strip is [R].** OBVR's own, not the game's: pieces of the tablet's quad drawn into the
eyes (`render/TabletQuad.h`, textured and plain quads on the holding hand), laid out in the layer's
pixels on the page's top edge (`vr/TabletTabStrip.h` `SetTabs`), clicked by the finger's tip or the
beam on the same face (`vr/MenuPlace.h` `StepTabletTabs`). It was only there with `Figure=tab`, with
two tabs. Now it is there in every figure mode, `page, [figure], mirror`: `vr/TabletTabs.h` `TabsFor`
decides which (the figure's only with `Figure=tab` and a figure fitted behind it; the mirror's with
`[Tablet] Mirror`; one tab alone is no strip). The tab state is an enum (`TabletTab`), not
`showFigure`.

**Built.**
- `vr/TabletTabs.h` (pure): the tabs of a frame, the click's choice with its refusals (a popup over
  the page shows the page and the choice waits; a tab the strip no longer has is forgotten; the
  tablet closed gives the page), the page turn that leaves the mirror.
- `vr/TabletTabStrip.h` (pure): three tabs at 0.28 of the page's width on its top edge, the third
  inside the page's width; `MirrorTabToLayer`: a press on the glass or a tab reaches nothing of the
  game's.
- `vr/MirrorPage.h` (pure): the frame (0.05 of the page's width), the glass inside it, the tab's
  small hand mirror (a rim and its glass), `GlassTexCoords` (the reflection's rectangle, u flipped).
- `vr/MirrorDust.h` (pure): the dust texture's texels, 256 by 256, the same every time: 2 by 2 texel
  specks of 5 to 12 percent alpha and six faint smudges of at most 5 percent, pale grey,
  premultiplied. Made once on the CPU like the paper (`TabletQuad::Dust`, with `Prepare` at the main
  menu, else at the first draw), drawn as one blended piece over the glass with the tablet's
  premultiplied blend. No shader, no file.
- The game's page underneath is not touched: no key, no click, no scroll reaches it while the
  mirror is shown (`MirrorTabToLayer`, `KeepMirrorFromGame`); a page turn by the right stick leaves
  the mirror (the proposal of section 7) and goes on to the game as it always did. A popup over the
  page shows the page; the mirror comes back when it goes.
- Settings: `[Tablet] Mirror` (default 1; 0 takes the tab away) and `[Tablet] MirrorDust` (default
  1); on the settings page, in "Laser & wrist menus" after "Hand menu in the world": "Mirror on the
  hand menu" ("A mirror tab at the end of your hand menu's tabs") and "Dust on the mirror" ("A
  light layer of dust on the mirror's glass"). `OBVR.ini` documents both.

**Where stage 2 hands in, and how it asks [R].**
- Asking: `render::TabletQuad::MirrorPage()` (through `HeadsetRenderer::MirrorPage()`) returns
  `MirrorPageState`: `up` (the mirror's page is shown this frame, the tablet drawn into the eyes,
  no popup over it) and the glass's four corners in the holding controller's frame (OpenVR's axes,
  metres; top left, top right, bottom left, bottom right). Render only when `up`.
- Handing in: `HeadsetRenderer::SetMirrorGlass(left, right, uv)` (`TabletQuad::SetMirrorGlass`): an
  `IDirect3DTexture9` for each eye (one alone is shown to both) and `MirrorGlassUv` (the part of the
  texture the glass shows, left, top, right, bottom in 0..1). The glass piece (index
  `kTabletMirrorGlassPiece` = 18) is then drawn with that texture, u flipped, in place of its dark
  colour (`TabletQuad::DrawIntoWorld`). The texture must be premultiplied or opaque, as the layer's.

**Deviations from the spec, with the reason.**
- `[Tablet] InScene=0`: stage 1 has no Mirror tab there (section 9, point 9 recommended the mirror
  page drawn in the scene anyway). The strip is made of the scene's pieces and the overlay is
  composited over the scene, so a scene-drawn mirror page would lie behind the overlay's picture of
  the page. Showing it needs the overlay's page hidden while the mirror is chosen, which needs a
  strip on the overlay too. Not built; a decision for stage 2 with the render route in hand.
- The dust is drawn over the glass only; the "slight darkening" of section 7 is the glass's own dark
  colour for now and becomes the piece's tint when the reflection is there.

**Tests.** Written first and run red on stub headers: `tablet_tabs_test` 26 failing checks,
`mirror_page_test` 15, `menu_place_test` 7 (the Mirror tab in the hand mode; one more fix found by
it, `KeepMirrorFromGame`), `tablet_test` 4 (the strip moved to `SetTabs`), `config_test` 3 (the two
settings and the INI's documentation of them), `settings_list_test` 1 (the pinned rows). Green after:
`tablet_tabs_test` 58 checks, `mirror_page_test` 23, `menu_place_test` 120 (11 new). The whole suite:
332 of 334 pass; the two that fail are the known ones (`water_visual_harness`, `grass_flicker_map`:
no numpy on this machine). Scenario `tools/hand-scripts/tablet-mirror-tab.txt` written for the three figure modes,
**not run** (Linux harness).

**Not verified.** How the tab, the frame, the glass and the dust look in the headset (the colours,
the tab's picture, the dust's size and strength); whether the coplanar pieces z-fight in the eye
(the figure's paper pieces draw the same way); the finger's place on the third tab; the dust
texture's creation cost (measured by the log line "the mirror's dust made on the CPU", not yet
read from a run). The unrolled tablet (`share` below 1) draws the mirror page cut like the others.

## 12. Stage 2 as built (2026-11-28, branch mirror-1128)

One reflection a frame on the glass: the world rendered by the engine from a camera behind the
glass, copied into a texture of OBVR's own and drawn on the glass piece. Not run in the game yet:
everything below about the running game is [D] or [G] until the owner's next session.

### 12.1 What the player sees with this build, and what not

- The Mirror tab's glass shows the world as a mirror held there would: the room behind the player,
  the people in it, the player's body from the neck down, the player's hands. 90 percent bright,
  the dust over it. The picture follows the head and the tablet.
- One picture for both eyes, made from between the eyes (the cyclopean eye reflected). It has the
  right parallax as the head moves, but no depth: it reads flatter than a real mirror, like a
  screen showing one. Expected; stage 4 gives each eye its own.
- No head, face, hair or helmet in it: the head stays hidden for every render of the frame, the
  mirror's too. Expected; stage 3 (built since, section 13.1).
- A frame late: the pass runs after the frame's eye pictures are taken (12.2, point 1), so the glass
  shows the previous frame's reflection. The tablet's page lags the same way already.
- The hands are drawn over everything in the mirror (the engine's first-person pass clears the
  depth first, 12.2 point 2): a hand behind the reflected body would show in front of it.
- The glass turned away from the eyes: dark (no old picture on its back). Edge-on, or out of both
  eyes' view: the pass is skipped and the glass keeps its last picture (it is not seen then).
- The first frame after the page opens on a single-pass frame shows the dark glass: the pass waits
  one frame for the bone rows (12.2, point 2).
- `MirrorWorld=0` (settings page: "Live mirror" off): the dark glass of stage 1.
- The monitor (not the headset): while the mirror renders, the back buffer it shows carries the
  mirror's picture in its top left corner over the frame (12.5).

### 12.2 The five unknowns: what was read, what was decided

**1. Which target the world render draws into.** Read in the decompile and OBNG's instruction-level
map of RenderScene (`../OBNG/re/functions/0x0040C830.md`, branches B8, B9, B17, B18, B43-B48) and
the callees: `0x007D7280(flags, group)` begins an offscreen frame (renderer +0x204 := 1 through
vtable +0x130) when no frame is open, then sets the group through `0x007D70A0` -> `0x007D6A80(group,
flags)`; `0x007D71C0` is the same for the default group (+0x200); `0x007D72D0` pops the group stack
(`0x007D7150`) and ends an offscreen frame (vtable +0x134); `0x007D6FE0` is a rendered texture's
group (`[this+8]`, or one picked through its +0x20 object); the image-space copy `0x00803E40`
begins its output group only while +0x200 and +0x204 are both 0 [R]. The routes for a texture
argument [R]:

| Image space | Samples | The world drawn into | Then |
| --- | --- | --- | --- |
| on | 2 or more (the owner's) | the back buffer (renderer vtable +0x7C, 0x0040CA36) | StretchRect of the whole back buffer into a frame-sized texture of kind 4 (0x0040CF5D), image space from it into the argument |
| on | 1 | the kind-4 texture (its port scaled by the engine when the sizes differ) | image space into the argument |
| off | 2 or more | the back buffer | StretchRect into the argument (0x0040D043) |
| off | 1 | the argument's own group | nothing |

The owner's route [R in his OBVR.log of 2026-11-28 08:27]: "back buffer ... 8072x3376 ...
samples=2", the scene's target set from 0x0040CA48 (the multisampled branch of the image-space
route) and image space from 0x0040CF73. So on his machine a world render overwrites the back
buffer whether it is given a texture or not, and image space maps the whole frame into the
argument: a 1024 texture would need the world drawn over the whole 8072x3376 frame. This corrects
section 3.1.

Decided: not the texture argument. The pass takes the eyes' own route (argument 0), with the main
camera's port cut to the picture's size in the frame's top left corner
(`vr::MirrorPortFor`; the engine writes no port on the multisampled route, and on the one-sample
image-space route only when its texture is not frame-sized, B11/B12), and copies that corner into
OBVR's own texture with StretchRect, as the eye captures copy (render/EyeMirror.cpp). That is the
dual pass's mechanism: a world render into the back buffer, taken by a copy. It can disturb the
eyes' pictures only before they are copied, so it runs after both captures of the frame
(`HeadsetRenderer::BothEyesCaptured`), and the glass shows it a frame later. Without antialiasing
and with image space, the frame's copy leaves the frame open and the mirror's copy would draw into
no group (game/SceneFrame.h): the pass closes it first, as the dual pass does between its eyes
(`CloseSceneFrameBetweenPasses`, a no-op on the other routes). After the render it checks that
target 0 is the back buffer, as the second eye's capture does, and copies only then.

**2. The bone rebase, the culling replay, and what is reset.** Read in render/BoneRebase.h and
InterfaceRenderHook.cpp: the palettes' translations are relative to the render's camera (the
origin 0x00B3F92C; the rows that re-evaluate in a second render differ by exactly the camera's
step, which the dual pass measures and calibrates its sign from [M]), the rotations are world
rows, and a second render of a frame takes some rows unchanged or mixed up between look-alike
bodies (the collapse) [R]. The mirror's render is the frame's second (single pass) or third (dual)
render, so the bone lock runs in Replace mode with the shift from the recorded render's camera to
the mirror's: a translation whatever the mirror camera's turn [D, not seen]. A dual frame's first
pass always records; a single-pass frame did not, so it records now while the page is up (Capture
changes no upload; SceneRenderHook.cpp:314). That is decided at the previous frame's pass, so the
first frame with the page up skips the mirror ("eyes not captured"). The culling share (CullingSync)
is closed by the frame's own render, so the mirror culls with its own frustum [R]. The first-person
pass inside RenderScene clears the depth before the arms (renderer vtable +0x13C(0, 4) at 0x0040CE14,
when +0x200 or +0x204 is 1 and the byte +0x20C is 1 [R]; 4 the depth [G: Gamebryo's CLEAR_ZBUFFER,
the value not checked against a header]). The water stays on its single pass: its second-eye
replay (WaterReprojection.cpp, mode 2 the owner's) would lay the first eye's per-draw matrices on
the mirror's water draws, so the reflection map is rendered again inside the mirror's render (a
pass of 6-13 draws, `culling-and-draw-setup.md` [M]). The actor shadows: the mirror keeps the maps
the frame's world render drew (render/ShadowShare.h, under `ShareActorShadows`, the owner's 1).

What the pass changes and where each is put back (render/MirrorGlassPass.cpp RenderFromMirror,
line 169 on): its frustum (set at 210, back at 245) and the main camera's local transform, its
world recomputed after the frustum is in (215, back at 246); its port (218, back at 241); the
device's viewport, which the render leaves at the mirror's corner (saved, back at 239); the bone
lock's mode and shift (223, back at 236); the frame clock, zeroed inside `RunMirrorWorldRender`
(SceneRenderHook.cpp:749) and put back there. The camera moved is the NiCamera itself (the scene
graph's +0xDC, a leaf), the way the water capture moves it (WaterReflectionHook.cpp
RestoreCameraTransforms) - not the camera node, so nothing hanging off the node moves with it and
the camera hook's and the menu stand-in's state is untouched.

**3. The winding.** Read in `0x0049BEF0.c`: the water's camera takes the camera's columns 0 and 2
reflected about z (`0x0070FCC0` builds the matrix from (1,0,0), (0,1,0), (0,0,-1), `0x00710250`
applies it) and makes column 1 again as their cross product, so it stays a proper rotation; the
function writes no cull mode (its only render state is 0x98, the clip plane) [R, in the
pseudocode]. Done the same (`vr::MirrorCameraFor`, `vr::ReflectCameraRotation`): forward the glass's
normal, up the glass's up, right = forward x up, determinant +1; the picture comes out flipped and
the glass flips it back (`vr::GlassTexCoords`, stage 1).

**4. OBNG's single-pass draw.** Read in `../OBNG/plugin/src/RouteStereoDraw.cpp`: with no eye data
set `EyesForDraw` returns none and the draw takes the one-eye plan unless the test shift is set
(`PlanFor(settings, eyes != nullptr || TestMode())`); the owner's OBNG.ini has
`StereoTestEyeShift=0` [R]. OBVR clears the eyes right after the single pass's render
(RenderSinglePassFrame) and never sets them on a dual frame, so the mirror's render draws once [D].
The menu frames: the stand-in arms an ordinary dual frame (PrepareMenuFrameIfNeeded), whose
captures and afterWorldRender are the same callbacks as a world frame's [R]; the pass reads only
the main camera and the tablet's state, so it runs there the same way.

**5. Where the extra render enters.** `RunMirrorWorldRender` (SceneRenderHook.cpp:733) calls the
trampoline like `RunMenuWorldProbe`: no OBVR callbacks, no scene-call tick (the mirror's render
belongs to the frame's scene call), inside the frame's `g_rendering` bracket.

### 12.3 Where it runs in the frame

`AfterWorldRender` (CameraHook.cpp:7758) calls `RunMirrorGlassPass` first (7760): after
`afterSecondPass` - both eyes captured - and before the per-draw changes go back (the hidden head,
the dead body's shift). On the plain path (no second pass) nothing was captured and the pass
refuses. In order: the page's state (render/TabletQuad.h `MirrorPage`: up, the glass's corners, and
from this frame's captures where the glass went into each eye and on which hand pose), the decision
(`vr::DecideMirrorPass`), the texture (made once, MirrorPixels a side, the back buffer's format),
the render (RenderFromMirror), the copy (CopyToGlass), the hand-in (`SetMirrorGlass`, line 348),
the cost line.

### 12.4 The switches and the safety

- `[Tablet] MirrorWorld` (default 1; 0 the dark glass), read with the tablet's keys
  (core/ConfigTablet.cpp), in OBVR.ini, on the settings page as "Live mirror" ("The mirror shows
  the room behind you. Off, its glass stays dark"). `[Tablet] MirrorPixels` (default 1024, 256 to
  2048), in OBVR.ini only: a pixel count is not a player's setting.
- With the page not up the pass reads the page's state, hands back the dark glass once and
  returns: no render, no texture, no capture of bones, no change to the frame. Only while the page
  is up does a single-pass frame record its bone rows (no upload changes).
- The pass runs only on a frame that delivers its two captured eye pictures, both captured.
- The stage trace: the first three passes of a session write "Mirror trace:" lines - the glass, the
  eye's distance and facing, the camera and frustum, the picture's size; the camera set and the
  render starting; the render returned, the picture in the frame or not, copied or not. A log that
  ends in a crash or a lost device names the stage.
- Failures: three passes in a row whose picture did not reach the frame or whose copy was refused
  switch the pass off for the session; a texture that cannot be made, or a copy refused with a lost
  or removed device, at once. Each releases the texture, darkens the glass and writes one line,
  "Mirror: switched off for this session - <why>".
- The device: OBVR hooks no device reset; the texture is in the default pool like the eye copies,
  and a device that is not the one it was made on gets a new one (the old one released).

### 12.5 Reading the cost line

Every 90 frames with the page up, one line:

    Mirror: 90 frames up - 84 passes, 1.83 ms mean, 3.10 ms worst (CPU), 412 draws a pass, picture 704x528 of 1024; skipped 6 (out of view 4, facing away 2, edge-on 0, eyes not captured 0, loading 0, no glass 0, stopped 0, switched off 0)

(the numbers made up). The milliseconds are the pass's own CPU time, QueryPerformanceCounter around
the camera's setting, the engine's render, the putting back and the copy; the draws are OBVR's draw
counter across the render. "stopped" also counts a pass that could not start (no back buffer read,
no texture). With `[Performance] Enabled=1` the profiler's `mirror_pass` span carries the same pass
with its GPU time, and the bone lock's work of the frame is one `bone_lock` span. The decision rule
of section 8 reads off the mean: over about 2.5 ms a pass in the Walkers scene means option E (the
eyes taking turns) before stage 4; the worst says whether single frames stutter.

### 12.6 Tests

Written first, run red, then green (Windows, MSVC):

| Test | Red | Green |
| --- | --- | --- |
| `mirror_glass_test` (new, vr/MirrorGlassLogic.h): reflection of a point, a direction and a camera rotation with its handedness, the glass's plane, the camera and its off-axis frustum (centred, off to one side, tilted), the eye on, near or behind the plane, edge-on, the glass in view, the picture's size and port, the decision with every refusal and its order, the failure budget, the cost window, the trace | 87 of 115 checks failing (a stub header) | 115 |
| `shadow_share_test`: the mirror keeps a single-pass or dual frame's maps, draws with sharing off or at another scene call, its own draw never stands for the world's, the second eye's rule unchanged | 3 of 22 failing | 22 |
| `profile_logic_test`: the `mirror_pass` name | 1 failing | 110 |
| `config_test`: the two settings' defaults, reading and clamps, the INI documenting them | 5 failing | green |
| `settings_list_test`: the pinned rows with "Live mirror" | 1 failing | green, re-pinned |

The whole suite: 333 of 335 pass; the two that fail are the known ones (`water_visual_harness`,
`grass_flicker_map`: no numpy on this machine). Scenario `tools/hand-scripts/tablet-mirror-world.txt`
written (the tablet towards the face, nearer, turned away, edge-on, back, then `MirrorWorld=0`),
**not run** (Linux harness).

### 12.7 Not verified, and what settles each

- **The cost.** Nothing measured. The owner's next session: the cost line, and a capture's
  `mirror_pass` span.
- **That the engine honours an off-axis frustum** (independent l, r, t, b) in its projection and its
  culling planes: NiFrustum keeps them apart, but the projection builder (renderer slot 80 from
  SetCameraData 0x00701970) and the plane builder 0x00717A40 are not read for it; OBVR has only ever
  written symmetric frustums. If not, the picture sits shifted on the glass. Settled by the eye
  dump at the scenario's `mirror-towards` mark: the room in the glass meeting the room around it
  where it should.
- **That the glass's normal points at the eyes** as stage 1's corners are ordered (top left and top
  right as the page is read, derived). If it is the other way, every frame skips as "facing away";
  the cost line says so.
- **The bone lock for a turned camera.** Derived from the palettes' layout, not seen. Settled by the
  eye dumps: bodies in the mirror where they stand; a body a body-length off is the collapse.
- **What image space does on the mirror's render.** The frame is open when it starts (state 1), so
  the image-space copy begins no group of its own and draws into whatever is bound (`0x00803E40`).
  The capture's target check passes after a dual pass's second render on the antialiased route
  (it showed stereo, 2026-10-06 [M]); whether the mirror's corner carries the bloom is not read.
  With HDR (not the owner's: an effective sample count of 2 means no HDR,
  docs/vr-modding/rendering-and-stereo.md) a second image-space run a frame might move the eye
  adaptation; not read.
- **The monitor and a frame that turns flat.** After the pass the back buffer holds the mirror's
  corner over the frame, image-spaced again. The headset never takes it on a stereo frame; the
  monitor shows it while the mirror renders, and a frame that turns flat between the render and
  Present (a menu opening exactly then) would show it on the cinema screen for that frame.
- **The 2D pass after the mirror's render.** After two world renders the game's own later 2D pass
  draws nothing (vr/HeadTracker.h, `hudBetweenPasses`); the layer the tablet shows is captured
  between the passes (`HudBetweenPasses=1`, the owner's), so the page should not change. The
  scenario's layer dumps (pass on, then `MirrorWorld=0`) settle it.
- **Billboards and leaves in the mirror**: OBVR's facing hooks may turn them to the player's eye,
  not the mirror's camera; not read. **The water's reflection seen in the mirror**: the stable
  capture rebuilds the water's world origin from the camera node (`GetCurrentCameraWorldTransform`),
  which the pass leaves at the eye, so the reflection on water seen in the mirror may sit offset.
  **The sky and the distant land** are placed from the camera node too: up to a metre off at the
  horizon [D].
- **The pose the glass is drawn with** is the hands' pose of this frame's captures; the corners'
  frame is the one ControlsOut set before the render. The same pose the page is drawn with, as 5.1
  asks [R].
- The headset: the feel of a flat mirror, the frame of lag, sharpness at 1024.

### 12.8 Deviations from the spec, with the reason

- Not the engine's texture argument (section 7, "the passes"): on the owner's route it needs a
  full-frame render for any texture size and overwrites the back buffer anyway (12.2, point 1).
  The eyes' route into the frame's corner and a copy instead.
- After the captures, a frame late (section 7 allowed it per route): on the owner's route the
  render overwrites the back buffer.
- The size rule of stage 5 (the picture cut to the glass's size on screen) is built now, because
  the port has to be cut anyway.
- Section 8's stage 2 row said the left eye's reflection; built: the eye's middle, as asked for this
  stage.
- `MirrorPixels` (section 7 called it `MirrorSize`), and only in OBVR.ini.
- The glass turned away goes dark instead of keeping its last picture.

### 12.9 The first measurement: the owner's headset run (2026-11-28)

Build: main 40b7b34 (stages 1 and 2), the DLL deployed at 10:14. Log `OBVR.log` of the run ending
12:28 (Windows, Pimax, image space on, 2 samples). The owner: "ja spiegel geht wie erwartet, bis
auf: Kein Spieler Kopf im Spiegelbild [...] außerdem ist das bild super wackelig weil man es ja in
der hand hält, muss also stabilisiert werden."

| What | Value | Mark |
| --- | --- | --- |
| Cost lines | 40 (3597 passes in 3600 frames with the page up) | M |
| The pass's CPU time, the mean over all passes | 1.39 ms | M |
| The lines' means | 0.59 to 4.10 ms, most 1.15 to 1.4 ms | M |
| The worst single pass | 5.28 ms (the line with 4430 draws a pass) | M |
| Draws a pass | 182 to 4581, 1152 the mean over all passes | M |
| The picture | 695x744 to 957x1024 of 1024 | M |
| Skipped | 3 frames in all, each "eyes not captured" (the first frame after the page opens, 12.1); 0 out of view, facing away, edge-on, loading, stopped; no "switched off" line | M |
| The first passes' trace (log lines 9629-9649) | the glass 21.4 x 22.9 units, the eye 41.8 units in front of it (facing 0.96), the frustum -0.350 0.163 -0.003 -0.551 (off-axis), 474-477 draws | M |
| How the cost moves | with the draws (correlation 0.84 over the 40 lines), not with the picture's size (-0.15); a straight fit gives 0.93 ms + 0.40 ms per 1000 draws | D, from the M lines |

Read off it: the picture is the right way round, the off-axis frustum works and the glass's normal
points at the eyes (0 "facing away"), which settles three points of 12.7. The decision rule of
section 8 (over about 2.5 ms a pass means E): the mean is 1.39 ms, over 2.5 ms only in the busy
stretch of seven lines (2.02 to 4.10 ms, 1644 to 4581 draws). So D holds; the busy places are the
open point. The bone lock in the mirror's render: the trace frames show one hair-class row (c31)
rebuilt by the engine with a translation exactly the camera's shift away from its logged pair
(log 9634: delta -34.2 -58.7 47.7 against the shift 34.2 58.7 -47.7) [M]: a palette made inside the
mirror's render is relative to the mirror's camera, which section 13 builds on.

## 13. Stage 3 and the steady picture as built (2026-11-28, branch mirror-3-1128)

Asked by Nadi after the run of 12.9: the head in the mirror only ("lass uns den also nur auf dem
spiegelbild oder wenn der spiegel da ist einblenden dann wieder weg"), and the picture steadied
("muss also stabilisiert werden"). Not run in the game yet.

### 13.1 The head in the mirror's render alone

What the eyes' renders see [R]: Bip01 Head's world transform recomputed at scale 0 after the body is
placed (game/PlayerBody.cpp `CollapseForThisDraw`: local scale 0, the engine's update, the local
scale put back), so everything skinned to the head or hanging under it lands on one point; with
`[Body] CullHiddenHead` the hidden bit on the two FaceGen nodes (the skinned face under the body's
root, the rigid parts under the head: ears, mouth, teeth, tongue, hair, eyes), taken off after the
frame's world render (`ReleasePlayerHeadCull`, CameraHook.cpp AfterWorldRender). The mirror's pass
runs in AfterWorldRender before that release, after both eyes' captures.

The palettes [R/M]: a skinned shape draws through the bone palette 0x007655F0 builds, the head's
row the head's world transform times its bind, every rotation entry multiplied by the composed
scale (`knowledge/bone-palettes.md`) - so the collapsed head's rows are zero. The builder skips a
skin whose stamp (NiSkinInstance +0x18) equals the renderer's (0x00765608 against 0x00422DE0 on
[renderer+0x8B0], read; what that value counts is not read), so a skin built earlier in the frame
can come to the mirror's render with the eyes' zero rows. And the bone lock (Replace) pairs rows by
register and rotation: a zero row pairs with the eyes' zero row and is replaced by it, shifted. A
row the builder makes in the mirror's render with the true head has a rotation the eyes never
uploaded, pairs with nothing and passes through as the engine made it - relative to the mirror's
camera (the engine's SetCameraData 0x00762640 writes the palette's origin 0x00B3F92C from the
camera it is given [R]; and 12.9's rebuilt hair row [M]).

So the direct way, without a second skinning pass of its own: the engine's own builder, made to run
for the skins on the head. What is switched (game/MirrorHeadLogic.h decides, game/MirrorHead.cpp
does; called at render/MirrorGlassPass.cpp:237, right before `RunMirrorWorldRender`, and :239,
right after it):

| Switched for the mirror's render | Where | Put back | Where |
| --- | --- | --- | --- |
| Bip01 Head's world transforms at its own scale (the engine's update of the head's subtree) | MirrorHead.cpp:177 | collapsed again, PlayerBody's three steps | :190 |
| The FaceGen nodes' hidden bit, only where HeadCull.cpp set it | :178 (HeadCull.cpp:132) | set again, until ReleasePlayerHeadCull right after the pass | :191 (HeadCull.cpp:141) |
| The stamp of every skin with a bone on the head (the head or under it), top bit turned: built again at its draw in the mirror's render | :179 | turned again: the rows now hold the shown head, so whatever draws the skin next builds it from the collapse | :189 |

The skins are found by a walk of the body's tree (at most 1024 nodes, 24 deep; NiTriShape and
NiTriStrips, the skin instance at +0xB8, its bones at +0x14, the parents up to 12 steps), into a
fixed list of 32: the face, the FaceGen parts, a helmet or hood skinned to the head, the upper
body's neck ring. A helmet or hood shows as worn: it is one of those skins.

Refusals, in order (each logged once when it changes, "Mirror head: <why>"): no body root; the body
not drawn this frame (`[Body] Visible=0`, the body away for a menu - the mirror shows hands and
room, decision 10); no head bone; the head not collapsed (`[Body] HideHead=0`: it is in the
mirror already); its own scale not a positive number; more than 32 skins on it (a half-shown head
would be worse).

What it costs, by reading [D]: nothing while the mirror does not render (the switch is inside its
pass). In a pass: the walk (a few hundred class lookups, the class names cached); the head's subtree
update twice (the same engine call the collapse makes every frame); the builder for the head's
skins once more (per bone one compose of 13 floats, 0x0053D7A0) - about ten skins, about a hundred
bones; and the head's own draws in the mirror's render (FaceGenFace 0.06 to 0.37 ms a pass, the
FaceGen parts together 0.08 to 0.45 ms in OBNG's occlusion probe, an order of magnitude). The
putting back's second stamp turn costs nothing extra if the stamp changes every frame (the next
frame builds the skins anyway); if it does not, one more build at the next draw. The cost line
says it: "head shown in N of M passes (X ms a pass)", the switch's own time both ways; the pass's
mean shows the draws.

Shadows: the mirror keeps the frame's actor shadow maps (12.2), drawn with the head collapsed: the
head casts no shadow in the mirror either, as before.

### 13.2 Not verified for the head, and what settles it

- That the head's skins come to the mirror's render through 0x007655F0 with the turned stamp and
  build from the restored transform. Settled by the scenario's `mirror-head` mark: the face in the
  glass's eye dump, and "Mirror head: shown".
- That no row of a rebuilt head skin pairs with a logged row by chance (a bit-identical rotation of
  another bone with the same register): only same-posed twins do that (BoneRebase.h), and the
  player's head has none [D]. A face a body-length off in the eye dump would be it.
- That the eyes still see no head: the switch is put back before the pass returns [R]; the
  `Head cull: at the mark` line at `mirror-head` ("culled this render") and the eye dumps settle it.
- The step back under a bowed head (section 4): unchanged; a head bowed over a low glass sits up to
  11 cm in front of its neck in the mirror. Headset.

### 13.3 A steady picture

Why it shook [D]: the glass is held in a hand. A hand's tremor turns the glass by a few tenths of a
degree at 8 to 12 Hz [G: the physiological tremor's known figures; the owner's log carries no glass
pose over frames, only the three trace lines, which agree to 0.1 unit], and a mirror turns its
reflection by twice the glass's turn. On top, the picture is rendered for last frame's glass and
drawn on this frame's (12.1): every move of the hand shifts the picture on the glass by one frame's
move.

The design as built - the projective variant, not the lesser one:

1. **The glass steadied** (vr/MirrorSteadyLogic.h `StepMirrorSteady`, called from
   render/MirrorGlassPass.cpp:169 `SteadyTheGlass`). The held glass's centre and turn (its plane's
   right, up and normal) go through a One Euro filter (Casiez, Roussel, Vogel, CHI 2012): a low-pass
   whose cut-off rises with the filtered speed, so a still hand's tremor is held back and a
   deliberate move is followed. Position and turn each have their speed; the turn is filtered as a
   share of the axis-angle turn from the steadied glass to the held one. It works in the game's
   world on the glass itself: not relative to the head (the head's own moves are real and the
   reflection must follow them, that is its parallax), and not on the controller's origin (a turn of
   the wrist moves the glass's centre). The room would do as well while standing, but its pose is
   not handed to the renderer; walking and turning by the stick are fast moves the filter follows
   at its raised cut-off, and a jump - more than 20 units (29 cm) or 30 degrees in a frame, a
   teleport, a load, a snap turn - is taken at once. A frame time of no number, zero, negative or
   over a quarter of a second starts the filter over from the glass as it is. The clock is the real
   one (the game's stands still in a paused menu). The page going down forgets the filter.
2. **The reflection rendered through the steadied glass with a margin** (`SteadyGlassCorners`):
   the steadied glass's corners grown away from its centre by 1 + 2 x margin, and the mirror camera
   (`MirrorCameraFor`, unchanged) reflected in the steadied plane, its frustum through the grown
   corners. The picture's size grows by the same share, up to MirrorPixels.
3. **The picture laid onto the glass as it is when drawn** (vr/MirrorGlassMap.h `MapGlassCorners`,
   render/TabletQuad.cpp:219 `LayGlass`). The pass hands the camera and frustum it rendered with to
   the glass with the texture (`MirrorGlassUv::view`, MirrorGlassPass.cpp:446). At the next frame's
   draws, each eye's draw projects the glass's four corners as they stand then through that camera,
   and those are the corners' texture coordinates. The glass and the picture's image plane are no
   longer parallel, so the mapping is projective: each corner gets (u q, v q, q), q its depth in the
   camera over the four's mean; the vertex grew a third texture coordinate (32 bytes, FVF
   TEXCOORDSIZE3(0)), and the glass's draw sets D3DTSS_TEXTURETRANSFORMFLAGS to COUNT3 | PROJECTED
   (TabletQuad.cpp:332), which the fixed-function pixel pipeline honours for pre-transformed
   vertices too (Microsoft's D3DTEXTURETRANSFORMFLAGS page; DXVK's fixed-function shader takes the
   third coordinate as the divisor, read through DeepWiki). Per pixel the device then reads exactly
   where that point of the glass projects (mirror_glass_map_test: the diagonal's middle exact,
   plain coordinates 0.5 percent off for a 3 degree turn). So the reflected world stands where it
   was rendered while the glass's frame shakes in the hand, like a real, slightly heavy mirror, and
   the frame of lateness the glass had goes with the same step: the picture no longer rides on last
   frame's glass.
4. **Never garbage past the picture**: the glass's sampler reads black past the texture's edge
   (D3DTADDRESS_BORDER, border colour black, TabletQuad.cpp:300), and the texture is black outside
   the picture (made black, and blackened outside a picture that shrank, MirrorGlassPass.cpp:339:
   two ColorFills only when the size shrinks). A corner at or behind the camera, or of no number,
   would wrap a projective picture round: that glass is drawn plain dark instead.

What stays a frame late: the eye. The picture is made from the eye of the frame before; the glass
is where it is. A head move of a frame shows as a frame-old parallax in the reflection [D].

**The constants [G]** and the one setting on them, `[Tablet] MirrorSteady` (0 to 100, default 60):

| | 1 | 60 (default) | 100 | Why |
| --- | --- | --- | --- | --- |
| Still cut-off | 7.8 Hz | 1.33 Hz | 0.4 Hz | evenly on a log scale from 8 to 0.4 Hz; at 1.33 Hz a 10 Hz tremor comes through about a seventh (first order, fc/f) [D] |
| Margin, each edge | 4.1 % | 7.6 % | 10 % | over the lags below for a 21-unit glass [D] |
| Turn's speed coefficient | 0.15 Hz per degree a second | | | a constant turn lags at most 1/(2 pi 0.15) = 1.1 degrees, however fast [D] |
| Move's speed coefficient | 0.3 Hz per unit a second | | | a constant move lags at most 0.53 units (8 mm) [D] |
| The speeds' own low-pass | 1 Hz | | | the filter paper's value: a tremor's speed swings both ways and averages out |

Measured in the unit test (mirror_steady_test, 90 Hz frames, the default): a 0.3 degree, 10 Hz
tremor comes out at 0.045 degrees (a sixth or less) and its 1 mm of shake at 0.14 mm; a 90 degree a
second turn lags 0.62 degrees after half a second and settles where it was turned to; a half metre
a second move lags 0.29 units. 0 is off: no filter, no margin, the picture stuck to the glass as in
stage 2 (the vertex's third coordinate is then 1 and ignored).

On the settings page, "Laser & wrist menus", after "Live mirror": **"Steady mirror"** - "Keeps the
mirror's picture calm while your hand shakes. 0 lets it shake with your hand" (0 to 100 in steps of
10). OBVR.ini documents it; `tests/settings-rows.txt` is re-pinned.

What it costs [D]: per frame with the page up a filter step (a few dozen float operations); per
eye's draw of the glass four corner projections; two ColorFills of the texture's edge strips when
the picture shrinks; the picture up to (1 + 2 x margin) squared larger - 1.33 times the pixels at
the default, where 12.9 shows the pixels do not move the cost, the draws do.

### 13.4 Reading the new parts of the cost line

    Mirror: 90 frames up - ... switched off 0); head shown in 88 of 88 passes (0.031 ms a pass); steady 60: the glass turned 0.38 degrees mean, 1.20 most from the steadied in 88 passes, laid on 176 eye draws, a corner past the picture in 0, not mapped in 0

(the numbers made up). "head shown": the passes whose head switch went through, and the switch's
own time both ways (13.1). "the glass turned": the angle between the held glass and the steadied
one, a pass each - the tremor the reflection no longer turns with is about twice it; a mean near
the expected few tenths says the filter holds the tremor, a "most" of several degrees with corners
past the picture says the margin is too small for the moves made. "laid on": the eyes' draws of
the glass through the picture's camera (two a frame); "a corner past the picture" are the ones
with a dark edge; "not mapped" the ones drawn plain dark (a corner behind the camera).

### 13.5 Tests

Written first, run red (stub headers, or the new parts stubbed), then green (Windows, MSVC):

| Test | Red | Green |
| --- | --- | --- |
| `mirror_head_test` (new, game/MirrorHeadLogic.h): shown with both bits, someone else's bit, CullHiddenHead=0, no skin; every refusal and its order (no body, body not drawn, no head, not collapsed, a world scale of no number, local scale 0 / no number / endless, too many skins); the skins' list with duplicates and overflow; the stale stamp | 21 of 25 checks failing | 25 |
| `mirror_steady_test` (new, vr/MirrorSteadyLogic.h): the setting's tuning and clamp; the turn between frames; off, the first frame; a still hand's tremor; a deliberate turn and move; jumps of place, turn and a centre of no number; a frame time of no number, zero, backwards, half a second, endless; the corners with the margin | 29 of 43 failing | 43 |
| `mirror_glass_map_test` (new, vr/MirrorGlassMap.h): the view; the same glass on the picture's corners, flipped; the margin; a glass turned 3 degrees, projective exact, plain off; past the picture, behind the camera, at it, of no number, no view | 13 of 20 failing | 20 |
| `mirror_glass_test`: the cost window's head, turn and glass draws | 3, then 4 failing | 122 |
| `config_test`: MirrorSteady's default, 0, clamps, a value as it is | 3 failing | green |
| `settings_list_test`: the pinned rows with "Steady mirror" | 1 failing | green, re-pinned |

The whole suite: 336 of 338 pass; the two that fail are the known ones (`water_visual_harness`,
`grass_flicker_map`: no numpy on this machine). `function_sizes_test` green, no row raised.
Scenario `tools/hand-scripts/tablet-mirror-world.txt`: the `mirror-head` mark (the glass at the
face), two shaking stretches (half a degree each way on two axes, ten times a second, three seconds
each, MirrorSteady 60 then 0) with marks `mirror-shake` and `mirror-shake-off`, and the cost line's
new parts expected. **Written, not run** (Linux harness).

### 13.6 Not verified, and what settles each

- **The head in the mirror and not in the eyes**: 13.2.
- **The projective glass on the owner's driver and under DXVK**: read in the documentation and
  DXVK's source description, not seen. Settled by the eye dump at `mirror-shake` (the room in the
  glass undistorted along the diagonal) and the headset; a picture bent along the glass's diagonal,
  or the glass reading only one corner of the picture, would mean the projected flag is not taken
  for these vertices.
- **The filter's feel**: the constants are guesses from known tremor figures. The cost line's
  "turned" mean and most from the owner's next run, with "a corner past the picture", say whether
  the margin and the cut-off fit; MirrorSteady is the one knob.
- **Walking and turning with the mirror up**: followed at the raised cut-off by the reading of the
  filter; a lag that sends corners past the picture while walking would show in "a corner past the
  picture". A filter in the room (the tracking space) would not see walking at all; it needs the
  room's pose handed to the renderer (small), if the run asks for it.
- **The texture's edge strips**: the ColorFill on a default-pool render target is read in the D3D9
  rules, not seen; a smeared edge where the glass reaches past the picture would mean it failed.
