# A mirror on the tablet (feasibility and plan, 2026-11-28; stage 1 built, section 11)

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

- The mirror pass's cost: every number in 5.3 is a guess.
- Which target `0x0040C830(texture)` draws into on each route, and whether it leaves the back
  buffer alone (3.1). `0x007D7280` and `0x007D6FE0` are not read.
- Whether the bone rebase holds for a third render from a rotated camera (3.3).
- Whether the first-person pass clears the depth (section 4).
- Whether OBNG's single-pass draw stays out of a texture render when no eyes are set (the eyes are
  set only around the main render in `HandOverEyes`; [D] that it then draws once).
- Whether the actor shadow share and the water reuse can be triggered from a pass that is not the
  dual pass's second eye (both hooks key on the dual pass's state today).
- The px-per-degree of the Dream Air's lens.
- The headset feel of all of it.
- Mobbin was not checked for the tab's look: the Mobbin MCP is not authenticated in this session.
  The tab reuses the tablet's existing tab style, so there is no new layout to compare.

## 11. Stage 1 as built (2026-11-28, branch mirror-1128)

Asked and approved by Nadi, 2026-11-28: stages 1 and 2; the recommendations of section 9 stand
(name "Mirror", the last tab, a small drawn glass, no live picture, dust on by default and light,
no mirror with the traditional inventory). Stage 2 is not built. What is there is the tab, the page
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
