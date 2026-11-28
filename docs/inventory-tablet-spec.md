# The inventory as a tablet in the hand (feasibility and plan, 2026-11-15)

The ask (Nadi, 2026-11-15): the inventory menu does not pop up in the world
ahead of the player; the player holds it in one hand like a tablet, the world
keeps running, and the other hand's finger taps, scrolls and swipes on it as on
a tablet. On by default, with a switch. It opens with the left B, or by a
gesture: both grips held and pulled apart like a scroll being unrolled; fully
open it can be used, short of that it rolls back.

All four stages are built (sections 7, 8, 10, 11 and 12), and the tap's tip from the index bones (task 6a, section 12); the open cases run and the opening hitch measured in section 13. This file is
the study, the build guide and the task list. It replaces the plan part of `docs/immersive-scroll-menu.md` (2026-09-21,
written against a code base that has since changed); that file's gesture rules
and its two pure modules stay valid and are reused here.

## 1. Verdict

**Feasible, medium in total, in four stages that each stand on their own.**
Most of the base is in the repo. Two points deviate from "exactly like a
tablet" (scrolling by rows, the onboarding switch) and one was decided
(the tablet is drawn in the scene); all three are in section 3.

| Stage | What the player gets | Cost | Why |
| --- | --- | --- | --- |
| 1 | Left B opens the Tab menus on a tablet in the hand, cropped to the menu, drawn in the scene so the finger is seen on it, world running, tap with the finger, laser as before | medium | wrist placement, poke, pause policy and the in-scene draw path exist; the crop to the menu's rectangle, the hand grip pose and the textured quad are new |
| 2 | Tablet touch: tap on release, swipe scrolls the list, sideways swipe changes the page, long press drags | medium | one new pure module; the feel (thresholds) needs the headset |
| 3 | The scroll gesture opens and closes it, with the unrolling shown | medium | gesture and session logic exist and are tested but were never wired; conflicts with every other grip use need the headset |
| 4 | Walking with the tablet open | medium, one engine risk | the controls gate exists for the container only; a click in a walking frame is an attack to the engine |

Recommended order: 1, 2, 3, 4. Stage 1 alone answers the two questions the
rest depends on and that no harness can answer: whether the text is readable at
tablet size, and whether rows can be hit with the finger. Cheapest step to
settle both: build stage 1 and one headset session.

## 2. What exists (read in the repo, main 67c65e9)

| Piece | State | Where |
| --- | --- | --- |
| The menus' picture on a controller | built (`[Hands] WristMenu`, default off: "the 2026-09-25 headset run found the wrists awkward"); it hangs the WHOLE 2D layer, 0.70 m wide, on the wrist | `render/HudLayer.h` `SetWristPlacement`, `vr/HandMode.cpp:1062` |
| Finger press on a menu quad | built: `PokeOnQuad`, `StepPoke` (hover 10 cm, press 1.5 cm, release 4 cm); the click goes down the moment the tip is through | `vr/HandInput.h:1127-1200` |
| The "finger" | the controller's tip 8 cm ahead (`pokeTipForward`), not a tracked fingertip; `FingerPokes` only asks whether the index is out | `vr/HandMode.h:81`, `vr/HandInput.h:1172` |
| Laser on the same quad | built, the fallback | `vr/HandMode.cpp:1207` |
| World runs behind the Tab menus | built: `[Render] UnpausedMenus`, **default off**, a settings row, **not on an onboarding page** | `game/MenuPausePolicy.h`, `ui/SettingsPartView.h:443` |
| Walking under a menu | built for the reached container only; OBVR "sends no click or cursor step in those frames, which the player's input would take as an attack and a turn" | `game/GameAddresses.h:875-880`, `game/MenuPause.h` |
| Left B tapped opens the Tab menus | built | `docs/controls-spec.md` 156, 312 |
| Wheel notches into the game's lists | built (`ScrollMouseWheel`) | `game/HandControls.h:74` |
| A tile's rectangle from the menu XML's tree | built for the HUD menu (`HudTileRect`, locus rule) | `vr/HudTileTree.h`, `game/HudTiles.cpp` |
| Cropping an overlay to a rectangle | built (`SetOverlayTextureBounds`) | `render/HudLayer.cpp:266`, `render/HandHudLayer.cpp:213` |
| The scroll gesture and its session | pure logic with tests in the build (`scroll_menu`, `scroll_session`), used by nothing | `vr/ScrollGesture.h`, `vr/ScrollSession.h`, `vr/MenuSurface.h` |

## 3. Where it cannot be exactly as asked

1. **As an overlay, the tablet hides the tapping finger - so it is drawn in
   the scene instead (decided 2026-11-15, Nadi: "warum nicht B dann ist es
   gleich richtig").** Every panel of OBVR is a SteamVR overlay, and the
   compositor draws overlays over the scene; the OpenVR headers describe no
   depth test between an overlay and the submitted eye pictures (deepwiki
   ValveSoftware/openvr, `headers/openvr.h`; from the API's declarations, not
   the compositor's code). The hand is in the scene, so it would disappear
   where the tablet covers it.
   - **B (chosen): the tablet as a textured quad in the scene.** The first
     version of this study called it large and said OBVR draws no geometry of
     its own into the eye passes. **That was wrong**:
     `render::ControllerModels::DrawIntoWorld` does exactly this for the real
     controllers - drawn per eye into the back buffer just before the eye is
     copied (`HeadsetRenderer::CaptureEye`), "against the game's own depth,
     through the projection the game drew with - so the world hides the
     controller where it stands in front, and the in-game hand can be seen
     around it", with the single-pass eye rectangle handled. The 2D layer's
     capture is an ordinary A8R8G8B8 render-target texture on the game's
     device (`render/HudLayer.cpp:97`), so it can be bound as the quad's
     texture. **Cost: medium**, on top of stage 1. New: a textured,
     alpha-blended, depth-tested quad through that path (pre-transformed
     vertices with a correct w, so the texture is perspective-correct).
     What it changes:
     - The picture is the last frame's: the 2D layer is captured after the
       scene. The quad's place is this frame's; only the content lags a frame.
     - **Risk: softer text.** An overlay is sampled once, by the compositor;
       a quad in the scene is sampled into the eye picture and then again by
       the compositor. How much that costs at tablet size is not measured.
       The overlay path stays behind `[Tablet] InScene=0`, so one headset
       session compares the two.
     - The eye's draw can refuse (the device's projection is not the world
       camera's, `DrawIntoWorld` returns false): the tablet then shows as the
       overlay on that frame rather than not at all.
     - The finger the player sees should be the one that taps. Today the tip
       is the controller's, 8 cm ahead (`pokeTipForward`); with the hand
       visible on the glass the tip should come from the hand's index bones
       (`Bip01 R/L Finger1`, `11`, `12`, `game/HandGrip.h:108`). Small to
       medium; whether the last bone's end is where the fingertip looks to be
       is not verified.
     - It pays twice: the rolled ends of the scroll (stage 3) become a few
       more quads of the same kind instead of `SetOverlayCurvature`.
   - A (dropped): a fingertip mark on top of an overlay tablet - a dot, not a
     finger.
   - C (dropped): leave the finger hidden.
2. **Scrolling goes by rows, not by pixels, and does not coast.** The game's
   lists scroll by wheel notches. A swipe is turned into notches as the finger
   moves; a kinetic glide after the finger leaves can be imitated with decaying
   notches (small), a pixel-smooth list cannot without the list's own code
   (not looked up; large by guess).
3. **"Like the unpaused world in the onboarding" has no model yet.** The
   switch exists only in the settings and is off by default. Proposed: the
   tablet brings its own switch and keeps the world running for its menus
   whatever `UnpausedMenus` says, and one row on the onboarding's comfort page
   (`ui/Onboarding.cpp` `kComfort`) turns the tablet on or off. Cheap.

## 4. Design

### 4.1 Which menus

The big four (inventory, stats, magic, map: `kMenuIdBigFour` and its pages)
and what they raise (`kMenuIdQuantity`, `kMenuIdMagicPopup`, a message box, a
book read from the inventory). Everything else - dialogue, barter, containers,
the pause menu, level-up - stays where it is today. With the tablet off, or
before the player is in the world, nothing changes.

### 4.2 The tablet's quad

- **Cropped to the menu.** The whole 2D layer is 16:9 and mostly empty around
  the Tab menu. The top menu's root tile gives the rectangle (the HUD's tile
  reader, pointed at the menu on top instead of the HUD menu); the overlay
  shows only that rectangle. A popup on top takes the union. Unknown until
  read: the root tile's rectangle for the big four under UI mods (DarN and
  the like) - read at run time, never a constant.
- **Size.** Start: 0.34 m wide (`[Tablet] WidthMetres`), a slider. Readability
  is the open question: the same pixels on about half the wrist menu's width.
- **In the hand, not on the wrist.** Lower edge at the grip, rising from the
  fist, tilted towards the eyes - a clipboard held from below. The transform
  is `WristOverlayTransform`'s kind with its own three numbers. The holding
  hand is the off hand; the weapon hand taps (left-handed: mirrored, as the
  wrist menu does it, `docs/left-handed-spec.md:53`).
- **A weapon in the tapping hand** stays there; the hand taps with the index
  out, or the laser is used. Not verified in the headset.

### 4.3 Touch (stage 2)

Today the click goes down the moment the tip is through the quad. A tablet
decides on the way out. One pure module, `vr/TabletTouch.h`:

| Input | Becomes |
| --- | --- |
| down and up within a small radius and a short time | a click at the place the finger went down |
| down, then moved up or down past the radius | wheel notches, one per row height moved; nothing is clicked |
| down, then moved sideways past the radius | the next or previous page (the F1-F4 keys' pages) |
| down and held still past a hold time | the button goes down and stays: a drag (sliders, the quantity box), up when the finger leaves |
| the tip leaves through the back, tracking lost, the tablet closes | everything released, no click |

The cursor follows the tip while it hovers, so the row lights up before the
tap. The laser path is untouched.

### 4.4 The world

Runs for the tablet's menus (`WorldPausesForMenu` gets a `tabletRuns` input
beside `containerRuns` and `lockRuns`). Open questions with the world running,
both existing already under `UnpausedMenus` and not answered there: what a hit
on the player does while the menu is up, and whether the tablet should fall
shut when a fight starts (decided 2026-11-15 yes, with a switch; **dropped 2026-11-17, Nadi: closing
it is the player's business** - the tablet stays up in a fight). **It must open in a fight all the same** (Nadi,
the same day: potions are fetched from it); it stays open until the player
closes it. The left B is the way in with a
weapon drawn - the scroll gesture asks for two empty hands (4.5).

### 4.5 Opening and closing

- **Left B, tapped**: as today it sends Tab; with the tablet on, the menu
  appears in the hand, unrolled over about 0.2 s. Left B or right B closes.
- **The scroll gesture (stage 3)**, as decided on 2026-09-21 and kept: both
  hands empty, close together (8-18 cm) ahead and below the eyes, grips
  freshly closed after a 0.35 s dwell; pulling apart unrolls by the measured
  distance, no snap; at full width (0.60 m, to be tuned) the next release of
  ONE grip leaves the tablet in the other hand and frees the finger. Short of
  full width, letting go rolls it back and nothing opens. Gripping the free
  end again and bringing the hands together rolls it up and closes it.
- **When the engine's menu opens.** 2026-09-21 it was put after the release,
  so nothing can be used early. **Decided 2026-11-15 (Nadi): at the start of
  the unrolling**, so the picture is there while it unrolls, with the cursor
  and clicks locked until the release. Costs one thing to measure: the hitch when the Tab menu
  opens (measured 2026-11-20, section 13: one frame of 17-24 ms on opening,
  one of about 26 ms on closing, the same without the tablet;
  `docs/container-touch-spec.md` section 3 has the same open measurement).
- **The rolls.** With the tablet in the scene (section 3, B) they are
  geometry of the same kind: the picture's quad cut to the opening, and a
  short many-sided cylinder of parchment at each edge.

### 4.6 Walking (stage 4)

The controls gate (`kPlayerControlsIsMenuModeSite`) answers "no menu" for the
container while the sticks walk, and OBVR sends no click in those frames,
because the player's input takes a click as an attack. For the tablet both
happen at once. Two ways, to be decided by a harness run:
- frames with a touch or a click answer "menu" (the walk stops for the tap);
  small, reuses the gate as it is;
- or the attack is held back while the tablet is open; needs the attack's
  read site, not looked up.

### 4.7 Settings

`[Tablet] Enabled=1`, `WidthMetres`, `InScene=1`, `Gesture=1`,
rows
under "Laser & wrist menus" and one on the onboarding's comfort page. `[Hands]
WristMenu` stays as it is and loses to the tablet for the tablet's menus.
Full VR only.

## 5. Risks, in the order they can sink it

| Risk | Decided by | If it fails |
| --- | --- | --- |
| Text too small at tablet size, softer still in the scene | headset, stage 1, `InScene` 1 against 0 | bigger tablet, the overlay with a fingertip mark, or the tablet becomes opt-in |
| Rows not hittable: the tip is the controller's, about 1 cm off | headset, stage 1 | bigger rows through size, the fingertip mark, the laser |
| The gesture fires in fights or blocks holster grabs, the two-hander, the ragdoll grip | headset, stage 3 | gesture off by default, left B alone |
| A click taken as an attack while walking | harness, stage 4 | no walking with the tablet |
| The Tab menu's opening hitch with the world running | harness measurement, stage 1 | measured 2026-11-20 (section 13): with or without the tablet, in OBVR's own frame flow at the menu switch (not the engine) |

## 6. Tasks

Each task: pure logic with every flow in a unit test first, then the wiring,
then a harness scenario, then deploy to Windows and Linux.

**Stage 1 - tablet by left B**
1. `vr/TabletLogic.h`: which menus are the tablet's (stack in, yes/no out),
   the crop rectangle from a root tile and the popups' union, the hand
   transform, the quad for the poke and the laser. Tests: every menu id, the
   stack with a popup, no tile, a degenerate rectangle, both hands.
2. The top menu's root tile through the tile reader (`game/HudTiles.cpp`
   generalised to a menu id). Harness log line with the rectangle read.
3. `HudLayer`: a crop rectangle with the hand placement
   (`SetOverlayTextureBounds`); the laser and the poke map into the same
   rectangle, one source for both.
4. `WorldPausesForMenu(..., tabletRuns)` with its flows; `ApplyUnpausedMenus`
   passes it.
5. Config `[Tablet]`, the settings rows, the onboarding row.
6. The tablet in the scene: `render/TabletQuad` on the controllers' path
   (`DrawIntoWorld`'s projection, depth and eye rectangle; one textured quad,
   alpha-blended, depth-tested). The quad's corners and texture coordinates
   from `TabletLogic` (tested); the overlay when the draw refuses or
   `[Tablet] InScene=0`.
6a. The tap's tip from the hand's index bones while the tablet is up, the
   controller's tip when the bones cannot be read.
7. Harness `tablet-open.txt`: left B, the rectangle, the placement on the
   hand, the world's clock moving, a tap on a row by script, close. Measure
   the opening frame's time.
8. Headset: readable - in the scene against the overlay (`InScene` 1 and
   0)? rows hittable? the finger on the glass where it taps? the grip pose?

**Stage 2 - touch**
9. `vr/TabletTouch.h` with the table of 4.3 as tests (tap, both swipes, hold,
   every cancel).
10. Wire it in place of the immediate click for the tablet's quad only.
11. Harness `tablet-touch.txt`: scripted tip paths; expect the row clicked,
    the list scrolled by n rows, the page changed, a slider dragged.

**Stage 3 - the scroll**
12. Bring `ScrollGesture`/`ScrollSession` to the current hand frame (empty
    hands, the grips as they are read today) and add the refusals for a held
    weapon, a grab in progress, a holster zone.
13. The bridge: open on unrolling, input locked until the release, close on
    rolling back; an unanswered open gives up after a timeout.
14. The crop following the opening, the parchment strips.
15. Harness `tablet-scroll.txt`: full open and release; let go half way; roll
    up; tracking lost mid way. Then the headset, in a fight too.

**Stage 4 - walking**
16. The gate for the tablet's menus with the click rule of 4.6, flows tested.
17. Harness `tablet-walk.txt`: walk with it open, tap while walking, no attack
    logged.

Not in this plan: a pixel-smooth list, containers and barter on the tablet.

## 7. Stage 1 as built (2026-11-17)

Nadi, 2026-11-17: "go Stufe 1". Built on Linux, tested by unit tests and the
harness without SteamVR; the headset has not seen it.

**What runs.** In Full VR (`[Tablet] Enabled=1`, the default): the left B opens the F1-F4
pages as before; while the menu stack is the tablet's
(`vr::TabletMenusUp`: the F1-F4 entry, with only a quantity box, the spell
popup, a message or a book beside it) the page sits in the off hand
(`vr::PlaceTablet`, a clipboard held from below: the lower edge 3 cm above and
2 cm ahead of the controller, leant back 30 degrees, held a quarter along the
edge from the outside), 0.34 m wide. The weapon hand points (laser) and taps
(finger, the old poke) at it: `vr/MenuPlace.h` puts the tablet before the
menu wrist before the big quad and maps the hit into the crop's pixels. The
world runs behind it (`WorldPausesForMenu`'s `tabletRuns`, whatever
`UnpausedMenus` says) and pauses for a message or a book. A fight does not
close it (built, then dropped the same evening: Nadi, closing it is the
player's business). The menus' shade is off behind it.

**The page's rectangle** (`game/TabletMenu.cpp`, `game::ReadMenuPaper`,
`vr::PaperOf`): the pictures (tiles with a file, a size, not clear, under no
hidden tile) of the four pages down to the third level; the shown page is the
one whose root is not hidden; its largest picture is the page, and every
picture of at least 0.4 of its area widens the crop (the map's extension
does, the tabs' strip does not). A popup in the stack adds all its pictures.
Read every frame the tablet's menus are up: 0.005-0.010 ms a frame, the worst
0.21 ms (tablet-open.txt). Measured in the game (tablet-tiles.txt): every
vanilla page's paper is `*_background` at 27,131, 745x792 HUD units. Not
read: UI mods (DarN and the like) - a layout whose page cannot be told
leaves the menus on their panel ("Tablet: no page read").

**On the hand the player sees** (2026-11-17 evening, Nadi: no hands on the
pictures). The first placement stood on the controller's origin; the drawn
hand sits on its controller by the player's grip (`[Hands] LeftHandGripX/Y/Z`
with the shared `HandGripUpMetres`/`HandGripForwardMetres`; Nadi's 0.15 m
back), so the page floated some 15 cm ahead of the hand (tablet-hands.txt).
`vr::PlaceTablet` takes the drawn hand's place on the controller since
(`vr::DrawnHandOnController`), for the draw and the beam alike. How the page
sits in a closed hand (GripUp, GripForward, TiltDegrees) is the headset's.

**The figure beside the page** (Nadi, 2026-11-17, of two montages: "B" -
the cut wide enough to hold the paperdoll where the game draws it, rather
than a second quad beside the page). `game/MenuFigure.cpp` walks the model
on the interface manager's holder (manager+0x60) and projects through the
menu's own view (`render::FigureScreenRect`: the view at the scene's origin
looking along +y, the y scale read off the model draws) the body's own bones
- head (14 units of crown), spine, pelvis, arms, hands, legs, feet - and
every small bound under 50 units (a weapon, a scabbard); the skeleton's
unused bones are left out ("Bip01 Wing 05 L" stood 75 units off the body
and stretched the cut to the screen's edge in the first run). The rectangle,
grown by 5 % of its height, joins the crop on every page; read every 15th
frame (the walk costs about 0.015 ms; the read with it 0.006 ms a frame on
average, the worst 0.06 ms, tablet-open.txt 20261117-203229-slot1). Seen in
the eye picture: the figure with its sword and scabbard beside the page,
the tablet then about 0.72 m wide. The scabbard's sphere reaches below the
feet, so the cut runs to the screen's foot, under the page's lower edge.

**The figure on its own page** (Nadi, the same evening, asked between a tab
and a second page: "zweite Seite daneben"). Drawn into the eyes the tablet's
face is the page and, abutting it on the right, a page of its own for the
figure (`vr::SetFigurePage`: as high as the page, the figure a gap of 4 % of
the page's width in from the edge, made smaller to fit with a gap above and
below, centred). Four pieces per eye (`render/TabletQuad.cpp`): the page and
its popups where the game draws them, the figure page's dark edge and its
paper in plain colour, and the figure. The beam and the finger are mapped
through the face (`vr::SurfaceToLayer`): on the figure, the figure where the
game draws it; on its paper beside it, nothing. While a popup is up the figure
page goes (the game draws the popup where it would stand), and with
`InScene=0` the overlay shows one cut of the layer with the figure where the
game draws it.

**Where the figure is, a setting** (Nadi, 2026-11-18: keep the second page as
one choice, add "no figure, a smaller tablet" and "the figure behind a tab
of the tablet's own, the tablet as small", the choice on the guide's comfort
page). `[Tablet] Figure` = `tab` (the default since 2026-11-18, Nadi), `beside` (its own page),
`off` (the page alone) or `tab` (`vr::SetFigureTab`: two tabs on the page's
top edge at its left, each with a small picture - the page's, the figure's;
the face grows up by the tabs, not wider; the figure's tab chosen shows the
figure fitted into the page's rectangle on paper). A press on a tab - the
finger through it or the trigger with the beam on it (`vr::StepTabletTabs`,
on the press's rising edge) - chooses it and reaches nothing of the game's;
a popup over the page shows the page whatever is chosen. The settings row
"Figure on the hand menu" (beside / off / tab) is on the comfort page in Full
VR while the menus are in the hand. tablet-figure-tab.txt PASS (the finger
on the figure's tab, then the page's), tablet-figure-off.txt PASS (no figure,
the page's rows and buttons as in tablet-open). Unit: tablet_test (the tabs,
the fit, the tab step, the words), menu_place_test (a tab chosen through the
hand mode, the beam on the figure behind it), native_settings_test.

**The game's own paper on the tablet's pieces** (asked 2026-11-18 as "open,
to improve later"; built 2026-11-20, `vr/TabletPaper.h`,
`render/TabletQuad.cpp`). The figure's page, the two tabs and the empty page
the figure is shown on behind its tab were plain parchment colour; they take
the texture the inventory's page is drawn with,
`Textures\Menus\Shared\shared_main_background_1.dds` (the `inv_background`
image of `menus\main\inventory_menu.xml`: 745 by 792 units on a 1024 by 1024
texture, one texel a unit from its top left - read from the vanilla
archives). It is opened once through the engine's file opener (so a UI mod's
paper of that name is the one taken), decoded on the CPU, premultiplied and
kept as a managed texture - made at the main menu, 13.4 ms there
(20261120-112238-slot1): made at the first draw it cost the first open of
the tablet 12 ms more (section 13). Behind its tab the figure stands on the
page's picture - its frame and bar as the game draws them, then parchment
down to the page's lower edge in place of the leather band (Nadi,
2026-11-20: paper behind the whole figure; `vr::FigurePaperBands`, the strip
above the band stretched about 1.6 times); the tabs and the
figure's own page (`Figure=beside`) take the plain parchment inside the
frame, cut to their shape (not stretched); the tab behind is darkened by a
tint. Without the file (another size, not a DDS this reads) the pieces keep
their colours and the log says so. Harness: tablet-figure-tab.txt PASS with
"Tablet: the game's paper ... (1048704 bytes, 1024x1024)", the eye pictures
show the framed page behind the figure (20261120-102213-slot2);
tablet-gallery-beside.txt PASS (20261120-102233-slot3). Unit: tablet_test.

**Drawn into the eyes** (`render/TabletQuad.cpp`, `[Tablet] InScene=1`): one
quad, the layer's texture, premultiplied over the eye, depth-tested against
the world, on the hand of the left role - from `HeadsetRenderer::CaptureEye`
beside the controllers. Found on the way: in menus the 2D pass between the
two eyes' captures draws the player's figure through a perspective of its
own, so the second eye's "last perspective projection" was not the world's
and it refused (single pass, 20261117-192206); the first eye's projection is
kept for the second within a frame since. The overlay shows the tablet when
the scene did not draw both eyes on the last frame, or with `InScene=0`
(then the overlay on the holding controller, cut to the crop by its texture
bounds). The picture is the layer as last drawn: the first eye's capture
comes before this frame's 2D pass, the second after it - the content may lag
a frame in one eye, the place never.

**Deviations from the plan, and why.**
- On by default (built off first, then on the same evening: Nadi, the code
  default on, the guide offers the switch). The switch is shown the other way
  round as "Traditional inventory", off by default - in the settings and on
  the guide's comfort page (ui/NativeSettings.cpp, after the walk direction,
  Full VR only); the row writes the digits swapped into `Enabled`. The 31
  harness scenarios that aim the laser at the big quad with the left hand
  tracked pin `ini Tablet Enabled=0` (and tools/gen-controls-matrix.py writes
  it).
- The HUD strip at the page's foot (health, magic, compass) lies inside the
  page's rectangle and is on the tablet, as in the flat game; its icons are
  the pages' buttons (the magic page clicked by the beam in tablet-open.txt).
  The category tabs are clicked as on the big quad.
- The tap's tip from the index bones (task 6a) was not built in stage 1;
  built 2026-11-20 (section 12).
- The rising is a 0.2 s reveal of the page from its lower edge
  (`vr::TabletOpenShare`), drawn in the scene only.

**Tests.** `tablet_test` (every flow of `vr/TabletLogic.h`), `menu_place_test`
(the place, the pixels, the tablet through `HandMode::Update`),
`menu_pause_policy_test` (the tablet's answer), `native_settings_test` (the
comfort page offers the traditional inventory in Full VR), settings rows
pinned. first-start.txt PASS with the row on the comfort page
(20261117-200808-slot1). Harness
PASS (Linux, headless, without SteamVR, single pass): tablet-open.txt,
tablet-fight.txt (stays up in a fight), tablet-overlay.txt; tablet-tiles.txt and tablet-sweep.txt
are the measurements.

**Not run:** the headset, SteamVR (the overlay path is not seen), Windows,
a UI mod. Run since (section 13): dual pass, a quantity box, a message, a
book, the map's wide crop, left-handed.

**Worauf achten (headset):** the text readable at 0.34 m, in the scene
against `InScene=0`; rows hit by the beam and by the finger; the page where
the hand holds it (grip, tilt); the finger seen in front of the page; the
world running while it is up; the map's width.

## 8. Stage 3 as built (2026-11-18)

Nadi, 2026-11-18: the scroll with both hands; in a fight, with a weapon or a
shield in hand, the left B, the tablet unrolling in the hand by itself - "go".

**The scroll** (`vr/TabletScroll.h`, pure, tablet_scroll_test; written new for
the tablet - `vr/ScrollGesture.h`/`ScrollSession.h` of 2026-09-21 assumed an
engine bridge and a surface of their own and stay unused). Both hands empty
(nothing drawn, no torch, nothing held, not on a weapon's handle), within
0.2 m of each other, both in the inventory ring (since 2026-11-22, section 15;
before, 0.15-0.75 m ahead of and below the eyes), held 0.35 s with
open grips: ready. Both grips closed (one after the other counts): the menu
key goes at once - the menus open as it starts to unroll, as decided on
2026-11-15 - and the tablet stands between the drawn hands, across from the
left to the right, upright and leant back by `TiltDegrees`, unrolled from
its middle by the hands' distance (`UnrollAmount`: 0 at 8 cm, fully open at
the tablet's own width). Fully open, one hand let go: kept by the other, the
tablet in that hand (`HeldByRight` mirrors the place onto the right hand,
which the left then points at). Let go short of fully open, or both at once,
or the hands back together: rolled back and the menus closed - a close owed
to menus not yet open is sent the frame they are, or dropped after a second.
Held - by the gesture or opened by the left B - the free hand's grip closed
within reach of the tablet rolls it up as the hands come together, and the
menus close at 8 cm; letting that grip go holds it open again. Opened by the
left B the tablet unrolls in the hand over 0.35 s (`TabletUnrollShare`).
While it is not fully open two plain rolls stand at the open part's edges
(`RollStrip`, in the figure page's edge colour - the paper texture is
section 7's open point).

**Not settled, the headset's:** whether the pose and the grips collide with
picking things up with both hands, the two-handed grip or the holster grabs
(the gesture asks for empty hands and a held pose; the menus open on the
grips' close, so the world's own grab sees at most that frame); the feel of
the distances and the dwell; whether the tablet should stand between the
hands at the scroll's middle or lower.

**Tests.** tablet_scroll_test (every flow: the pose, the dwell, both grips,
the amount, kept by either hand, let go short, both at once, squeezed back,
the owed close, rolling up and letting go of it, opened by the B, what keeps
it away - a weapon, another menu, the hands lost, too far, above the eyes,
grips closed all along, one grip), menu_place_test (the menu key, the place
between the hands as the left controller's, the right hand keeping it and
the left pointing, the B in the left hand), tablet_test (the unroll from the
middle, the rolls).

## 9. Asked and answered: the figure as a tab of the game's own inventory

Nadi, 2026-11-18: could the figure be one more tab in Oblivion's own
inventory menu, beside its category tabs, instead of the tablet's tab?

**Possible, large, not recommended.** Read and derived, not built:
- The inventory's tabs are tiles the menu's XML defines (`inv_tabs`, `inv_p1`
  to `inv_p5` with `shared_0N_tab.dds`, read in the game 2026-11-17,
  tablet-tiles.txt). One more needs `menus/main/inventory_menu.xml` changed or
  replaced - the file UI mods (DarN and the like) replace too, so a shipped
  copy fights them for the same path; MenuQue can add tiles at run time,
  which avoids the file but still has to be fitted into each layout.
- The figure is no tile: the engine draws the paperdoll as a 3D model of its
  own beside the menu (render/MenuModelFit.h). A tab showing it would have
  to hide the list's tiles and stand the model in the list's place - through
  MenuQue tile traits and the model's holder (manager+0x60, which OBVR already
  moves), with the engine's own clicks on the tab handled by a script or a hook.
- Cost: large - XML or MenuQue work per layout, the click handling, the model
  placement, and a check against every UI mod in use; the risk sits in the
  UI mods and in the engine's menu code (the click on a tile it does not know).
- The tablet's own tab (`[Tablet] Figure=tab`, section 7) gives the same view
  without touching the game's menu and works with any layout whose page the
  tablet can read. Kept as the way; this stays open only if the headset finds
  the tablet's tab wanting.

## 10. How it is held and unrolled (2026-11-18)

Nadi, 2026-11-18: a setting for unrolling it sideways or up and down,
sideways by default; held at its side - or at its top or bottom with the
other setting - not from below as stage 1 had it.

- `[Tablet] Unroll` = `sideways` (default) or `vertical`, a settings row
  "Unroll the hand menu" and on the guide's comfort page (Full VR, menus in
  the hand). Vertical: the hands one above the other, the tablet upright
  between them from the lower to the upper hand, facing the eyes; fully open
  at the tablet's height; unrolled from its middle up and down, the rolls
  lying across (`RectOnTablet`/`RollStrip` with `vertical`).
- Held (`vr::TabletHold`, `vr::HoldFor`): sideways by its side - the left
  hand its left edge, the right hand its right, the page's middle at the
  hand's height; vertical by its bottom edge (the clipboard of stage 1, also
  when opened by the B) or, when the upper hand kept it, by its top edge
  with the page hanging below.
- Rolled up only at the free end: the other hand's grip closed within 10 cm
  of the middle of the edge opposite the holding hand. Found in the harness
  (tablet-scroll.txt, 20261118-133312): a grip anywhere over the page was
  also the inventory's grab - "the other hand closed on Empty Canteen" - and
  took the item out, closing the inventory. Since, while the scroll holds the
  grips (unrolling or rolling up) the inventory's grab is not armed
  (`tabletGrips`, camera/hands/Grab.cpp).
- `[Tablet] Figure` defaults to `tab` since 2026-11-18 (Nadi).

Harness (Linux, headless, without SteamVR): tablet-scroll.txt and
tablet-scroll-vertical.txt PASS (four menu keys each, no inventory grab);
the tablet-gallery-*.txt runs picture every look (traditional, the defaults,
beside, held from below, a larger tablet); the aimed scenarios pin
`Unroll=vertical` (held from below, where their angles were measured).


## 11. Stage 4 as built: walking with the tablet open (2026-11-19)

Asked (Nadi, 2026-11-18): the left stick walks on while the tablet is open, a
click on it is no attack, and a click does not stop the walk.

- The sticks walk under the tablet's menus as under a reached container's:
  the player's controls gate (0x0040DC61) answers "no menu" while the stick
  walks (`PlayerControlsRunUnderMenu`, its `tabletWalk`). The left stick
  walks, the right one scrolls the list; it turns nothing there.
- A click is no attack: the menu's click is the attack's own button
  (game/HandControls.cpp), and the player's input, running, read it as a
  blow. From the gate to the player's update's end (0x0040DD5F, the paths'
  meeting point) the player's reads of the controls are held to the walk -
  GetControlState (0x00403520) is detoured, and reads of anything but
  forward, back, the slides, run, always run and auto move answer 0 there
  (`PlayerControlReadUnderMenu`). The menus' own reads lie outside that
  span. This now holds under a reached container's menu too.
- No stop for a click: under the tablet the click goes out while the stick
  walks, and the laser's cursor steps go on; the player's reads of the
  mouse's x and y (0x0067195B, 0x00671966) answer 0 under the tablet, so the
  steps turn nobody (`PlayerControlsReadAxis`). Under a container the old
  rule stays (no click and no cursor step while the sticks move: the right
  stick's turn is the mouse's x there).
- Found on the way, the reason the stick walked on the spot: while the F1-F4
  menus show their figure, PlayerCharacter::GetAnimData (0x0065D720) answers
  the figure's anim data (the player's +0x5DC). The player's movement
  animation (0x005FCAB0) and UpdateMovement (0x0063C730) take theirs from it,
  so the walk was played on the figure and the Move got a zero step
  (measured: the run group played on the figure's anim data, the Move trace
  0.000 with the walking flags; under a container the player's own and a
  step of about 5 units a frame). For the player's update under the tablet
  the figure's pointer is set aside and put back at the span's end
  (`PlayerOwnAnimDataForSpan`); the menus never see it missing.
- Any of these sites taken by another mod: the tablet does not walk, its
  sticks scroll as before (`TabletWalkRefused`).
- The walk is said as it ends: "Tablet: walked N units with it open, M
  click(s) sent to its menus on the way".

Harness (Linux, headless, without SteamVR): tablet-walk.txt FAIL before
(d6bc80c: the stick scrolled, the feet stayed), PASS after: 882 units walked
in 2.5 s with one click on the magic page's button, the page changed, the
attack read held back. tablet-open, tablet-fight, tablet-scroll,
tablet-figure-tab, reach-walk-wheel, inventory-pointed-item,
inventory-grab-weapon, analog-walk PASS. Not run: the headset - whether a
tap while walking lands on the row, and how walking with the tablet feels.

### 11.1 The pages by the right stick (2026-11-19)

Asked (Nadi): with the tablet open, the right stick left and right changes
the page. A push to a side goes to the page beside the one shown, in the
order of the page buttons - stats, inventory, magic, map - round from the
last to the first, by the page's own key (F1-F4); one page a push, the
stick back near the middle before the next (vr/TabletPages.h,
`StepTabletFlick`). The page shown is the one the tablet reads
(`TabletFrame::page`): the F1-F4 menus stand in the menu stack as one
entry. Up and down still scroll the list. Not while a popup is over the
page. Harness tablet-pages.txt FAIL with d3feb76, PASS after; not run in
the headset.

A lesson for the harness: `--plugin` copies the DLL under its own file
name, so a renamed copy (green3.dll) ran beside the live OBVR.dll. The
stage-4 runs above were repeated with the DLL named OBVR.dll: tablet-walk
882 units, tablet-open, tablet-fight, tablet-scroll, reach-walk-wheel,
inventory-grab-weapon, analog-walk PASS (2026-11-19 10:55-10:58).

## 12. Stage 2 as built: the finger as on a touch screen, the tip from the index bones (2026-11-20)

Asked (Nadi, 2026-11-20): "go Stufe 2 + Fingerspitze aus den Fingerknochen".

**Touch** (`vr/TabletTouch.h`, pure, tablet_touch_test). On the tablet the
finger no longer clicks the moment its tip reaches the glass; the press
decides on the way out (4.3's table):
- down and up without moving more than 1.2 cm: a click on the release, where
  the finger went down - the cursor is held there from the touch to the
  click;
- moved up or down past 1.2 cm: the list scrolls with the finger, one wheel
  notch per 2 cm, nothing clicked; let go while moving faster than 0.25 m/s,
  it coasts on and slows to a stop (the laser's coast, `vr::StepLaserPress`,
  fed the finger's way in millimetres);
- moved sideways past 1.2 cm: one page per touch - the finger moved left
  turns to the page on the right, as a tablet's page is turned - by the
  page's key, as the right stick does (11.1);
- held still 0.5 s, or put down on a scroll bar's knob: the button goes down
  and stays until the finger leaves (sliders, the quantity box, the bar);
- the finger gone from the face (off its edge, through it from behind past
  the poke's `through`, the trigger pulled or the index curled, the hand
  lost): the touch ends with nothing clicked; a drag lets go.
The distances are metres on the glass, so the feel does not hang on the
tablet's width or the layer's pixels; all four numbers are first guesses for
the headset. The wrist menu and the big quad keep the old poke (the click as
the tip arrives). Wired in `vr::StepFingerOnTarget` (vr/MenuPlace.h), which
`HandMode::PointAtMenu` calls for every target; the wheel joins the sticks'
`menuScroll`, the page step the stick's `tabletPageStep`.

**The tap's tip from the index bones** (task 6a, `vr/FingerTip.h`,
`camera/hands/LaserTargets.cpp` `StepIndexTips`). Right after the fingers are
posed (`game::StepHandFingers` updates their bones), the drawn index finger's
last two joints (`Bip01 R/L Finger11`, `Finger12`) are read; the tip lies past
the last joint by 0.8 of the link before it (a guess from the human finger),
and is taken into its controller's own frame against the hand frame it was
pinned with (`vr::ControllerLocalOf`), eased (0.3 of each reading). The next
frame's hand mode puts it on the controller's pose. Used on the tablet only,
and only while the drawn fingers follow the controller's
(`game::FingerPose::Tracked`: finger tracking on, curls read, the hand holding
nothing) - a hand round a weapon or posed by the animation has no finger out
to see; otherwise, or for a reading over 0.3 m from the controller, the
controller's tip 8 cm ahead as before. The beam's line ends at the tip used.
Logged: "Hands: the right index finger's tip read - x y z m in its
controller's frame" and when it stops being read, with the reason.
`[Tablet] DrawnFinger` (default 1) switches it; 0 taps with the controller's
tip, the comparison for the headset. Read in the harness without SteamVR
(the stand-in reports open fingers, so the bones' tip is used there too): with
the controller held level the drawn right index tip lies 0.013 -0.080 0.047 m
in its controller's frame - 8 cm below it and 5 cm behind it, not 8 cm ahead.
Whether that matches a real hand is the headset's. The scenarios whose finger
places were measured with the controller's tip (tablet-figure-tab,
tablet-hands, tablet-finger-sweep) pin `DrawnFinger=0`.

**Seen in the game** (2026-11-20): the tap equips the row's weapon (the Iron
Longsword in place of the Elven one, tablet-touch.txt's expect); the drag
scrolls a list longer than the page - tablet-touch-scroll.txt gives twelve
more items, the tile under one spot goes from Iron_Arrow to the list's
paper after a 10 cm drag up and back after the drag down, the pictures show
the list moved (20261120-133527-slot1). tablet-touch.txt's own drag runs
over the save's five rows, which fit the page: the notches go out, nothing
can move.

**Log** (camera/hands/ControlsOut.cpp `LogTabletTouch`): "Tablet: the finger
tapped / drawn up or down / swiped sideways / held still / gone from the
glass", "the finger's scroll moved the list N notch(es)", "the finger turns
the page"; at a harness mark with the finger over the tablet, the tile under
it ("Tablet: at the mark the finger is over").

Not settled, the headset's: the four numbers; whether the tip from the bones
sits where the fingertip is seen on the glass (0.8 past the last joint is a
guess); whether a swipe meant as a scroll turns a page now and then; the tap
with a controller without finger curls (the controller's tip, as before).


## 13. Cases run and the opening hitch measured (2026-11-20)

Asked (Nadi, 2026-11-20): run the cases nobody had run, measure the hitch
when the menus open, and hand the hitch to the OBNG session to fix - OBVR
only measures. All runs on Linux, headless, without SteamVR (the stand-in
headset at 72 Hz), single pass unless said, OBVR.dll 8d69775c (main fd9dd5e
with the paper of section 7).

**The cases** (scenarios in tools/hand-scripts/, run folders under
artifacts/hand-script/):

| Case | Scenario | Result |
|---|---|---|
| Quantity box | tablet-quantity.txt | PASS 20261120-104130-slot3: the left A (drop) on a stack raises the box; "shown with 1 popup(s)", the face widens to 2831 px; the world keeps running behind it (as `TabletMenuRunsWorld` says: only a message and a book pause) |
| Message box | tablet-message.txt | PASS 20261120-104130-slot4: the console's MessageBox over the open tablet; 1 popup, the world pauses; after its button 0 popups and the world runs again |
| Book | tablet-book.txt (rows found by tablet-book-sweep.txt) | PASS 20261120-110919-slot1: the finger taps the book's row, the book opens over the page (1 popup, the face 63..3379 px), "behind menu Book (0x402) the world pauses" |
| The map's wide crop | tablet-map.txt | PASS 20261120-104346-slot3: "the Map page read", the face to 2849 px; the picture shows the map's extension past the page's right edge |
| Left-handed, tapped | tablet-left-handed-tap.txt | PASS 20261120-110532-slot2: the right B opens the tablet in the right hand, the left finger taps a row (one tap, one click) |
| Left-handed, the beam | tablet-left-handed.txt | PASS on open and close only; the left hand's beam, its angles mirrored from tablet-open.txt, hit nothing ("laser hit NONE") - not tuned, so whether that is the scenario's aim or OBVR is not known |
| Dual pass | tablet-dual.txt | PASS 20261120-104843-slot2: drawn into the eyes under `Stereo=dual`; the right eye's picture shows the tablet too (the log names only the first eye) |
| The finger's gestures | tablet-touch.txt, tablet-touch-drawn.txt, -drawn-off.txt | section 12; PASS 20261120-104633-slot3, the run before stage 2 FAIL (104653-slot1) |

Still not run: the headset, SteamVR, Windows, a UI mod such as DarN (not
installed on the Linux game; installing one would change Nadi's setup).

**The opening hitch** (tablet-open-hitch.txt; the control
tablet-open-hitch-off.txt with `[Tablet] Enabled=0`, the menus on the big
quad and the world paused; `--exclusive`, the profiler and the render
sampler; tools/perf-phases.py and tools/perf-mark-frames.py). The left B
opens and closes the menus five times. Every other frame stays at the
stand-in's 13.9 ms; one frame per open and one per close is long:

| | open (about 195 ms after the press) | close (about 265 ms after) |
|---|---|---|
| tablet, 20261120-111035-slot1 | 21.9, 17.7, 18.0, 23.6, 21.8 ms | 26.2, 26.2, 24.6, 26.5, 26.6 ms |
| control, 20261120-111245-slot1 | 22.3, 17.2, 18.4, 18.4, 17.4 ms | 26.0, 26.1, 26.4, 26.3, 25.7 ms |

- The tablet's own page read costs 0.005-0.05 ms a frame. One cost was the
  tablet's: the first open made the game's paper (section 7) in its first
  eye capture, 12.2 ms more (the three frames from the press 56.1 ms
  against 42-43 for the later opens and the control). The paper is made at
  the main menu since (13.4 ms there); the first open then takes 42.6 ms
  like the rest (20261120-112238-slot1). Otherwise the hitch is there with
  the menus on the tablet or not.
- No time is lost over the frames, it is uneven: the three frames from an
  open add up to about three periods (42 ms against 41.7), the close's two
  to about two (1.5 + 26.2 ms). The headset sees one picture late, then one
  early - a judder, not a slower frame rate (derived from the frame times;
  the stand-in has no compositor to count the reprojected frames).
- **Whose it is: OBVR's own frame flow, not the engine** (read by the OBNG
  session from these captures, 2026-11-20; the close checked here in
  cpu_events.csv). A first reading put it in the engine's physics step: the
  sampler's stack words w0..w7 named Havok's 0x00893B90 chain - stale words,
  not calls. The eip says otherwise:
  - The close: the last menu frame (live_menu) takes 1.5 ms and has no pose
    wait; the next frame waits twice - once inside the present callback
    (begin_frame 10.4 ms), once at its own start (11.6 ms) - before its
    scene pass at 24.8 ms. The two frames together take 27.5 ms, two frame
    periods, but the picture after the short one comes 26 ms late. Every
    close in both runs; the samples in the gap sit in the stand-in's Sleep
    inside WaitGetPoses (ntdll ZwDelayExecution <- kernelbase <-
    openvr_mock <- OBVR). With SteamVR the second WaitGetPoses would block
    the same way (derived, not run).
  - The open: hud_between (the 2D pass between the eyes) 5.1-5.6 ms instead
    of about 0.1 ms on the first menu frame, and file access
    (ZwOpenFile/ZwReadFile) on the main thread between the pose and the
    scene pass. OBNG first read the file access as OBVR.dll's; it is the
    engine's, see "The file access on opening" below.
  - So the close's fix is OBVR's. The open turned out to be the engine's
    (below).
- **The close fixed** (2026-11-20, `camera/MenuStandIn.h`, frame_logic_test):
  the menu's camera stand-in (CameraHook.cpp `PrepareMenuFrameIfNeeded`)
  now also opens the one frame after a menu frame it opened - the frame the
  menu closes on, whose camera update ran while the world was paused. Once:
  armed without a menu, it does not carry on (loads, a menu reopened).
  Measured, the same machine and OBNG.dll for both (--exclusive):
  - before (5519102), 20261120-132745-slot1: the five closes 26.2, 26.3,
    18.8, 26.3, 25.1 ms, each after a 1.3-5.0 ms frame; five short-then-long
    pairs in the run, 20 frames over 20 ms;
  - after, 20261120-133015-slot1: 16.8, 26.9, 14.9, 15.7, 14.6 ms; no
    short-then-long pair, 15 frames over 20 ms; without the tablet
    (-off, 20261120-133244-slot1) 14.7, 16.8, 26.1, 15.3, 14.7 ms, no pair.
  - The two closes still long have another cause: 13 ms of the engine's
    work before the stand-in's pose wait (with the tablet), a 6.8 ms scene
    pass the frame before (without). Not looked into further.
- **The open is not fixed.** The work the engine does on the opening frame
  (2.5-4.7 ms after the pose wait, a 2D pass of up to 5.6 ms on the first
  open) lands after OBVR's pose wait and pushes that frame out; the next
  frame is short and the time is made up. The pose wait moved to just
  before the render (`[Render] LatePoseWait`, docs/late-pose-wait.md) does
  not change it: the 2D pass comes after the wait in both orders. Handed to
  OBNG for its list (2026-11-20, Nadi: OBNG fixes, OBVR measures with
  tablet-open-hitch{,-off,-late}.txt).
- **The file access on opening is the engine's** (2026-11-20, read from the
  samples of 20261120-111035, -111245 (tablet off), -150732 and
  -150502-slot1; OBVR names from build-linux-clangcl/OBVR.map of the
  DLL 17a6f5d9 of the last two, Wine's from Proton's i386 ntdll and
  kernelbase exports):
  - The reads go through the engine's own read path: 0x0042C462 calls
    0x00747D10, which is `fread(buffer, 1, size, file->FILE)` (+0x1C), then
    the CRT's _read and ReadFile. The same chain in every run.
  - The directory lookups are FindFirstFileA (ZwOpenFile and
    ZwQueryDirectoryFile under kernelbase!FindFirstFileExW). OBVR calls
    FindFirstFileA only at the start (platform/Win32Min.h ForEachFileName:
    core/Log.cpp's log archive, game/SelfCheck.cpp's Steam overlay check),
    never on a menu's opening.
  - The OBVR names in those stacks (ApplyRidingView, ChargenView) sit
    between two kernelbase frames of the same call, which no real call
    does: stale stack words, the same mistake as the Havok reading above.
    Some of the engine words are stale too (0x00718AD8 follows a matrix
    multiply, 0x00634904 an InterlockedIncrement); the sampler's 1 KB of
    stack does not always reach the real caller under Wine's frames.
  - The same accesses with the tablet off (the control run).
  - Size: 0-2 samples of about 1.1 ms in the opening frame (0.5 on
    average), so about 0-2 ms, not the 2.5-3.0 ms first read.
  - Which file is not known. The reads also come 20 ms after the close's
    press, so the menus' sounds are a guess (not verified). Wine's file log
    (WINEDEBUG=+file) on tablet-open-hitch would name it; handed to OBNG
    with the rest.
- **OBNG's measurement of the open and the close** (2026-11-20 evening,
  OBNG's timed probes, Linux stand-in 72 Hz, `LatePoseWait=1`; OBNG
  artifacts/shadow/tablet-open-hitch-late/20261120-203805, -204037, -204517,
  -204750 and obng-menu-open-quad-late/20261120-205021). Read by OBNG, not
  rechecked here:
  - The interface update 0x005821F0 takes 2.4-5.1 ms at an open and
    1.4-6.6 ms at a close. Its largest and most uneven part is the menu
    sound, read from its file on the main thread at every play (0x006AC610
    -> 0x006AABD0, the WAV reader): 0.5-2.7 ms at an open, 2.1-6.1 ms at a
    close, once more at the first hover. That is the file access above.
    OBNG built a cache for it (`[Route] SoundFileCache`, 836 answers
    compared, 0 mismatches), not deployed yet.
  - Each open rebuilds the inventory figure "PlayerInventoryPC" (0x00668D00,
    1.0-1.2 ms; destroyed at every close by 0x0057ECB0) and refills the
    item list (0x005AADC0, 0.7-1.2 ms with the harness save's few items).
  - With `LatePoseWait=1` the opening frame itself is only 0.8-1.3 ms late:
    the wait takes the update up. The session's first open is the
    exception (the 2D render 4.6-5.1 ms once).
  - **Open, OBVR's:** with the tablet on, 1-3 more late frames (15-21 ms)
    come 80-380 ms after an open: the scene pass 2.0-4.6 ms against
    0.9-1.3, the engine's 2D render 1.9-5.2 against 0.7, inside it the tile
    update 0x005903E0 1.6-4.0 ms and single draws of 1-3.9 ms. With the
    tablet off, two such frames in five opens. OBNG's guess for the
    trigger: a focus change by hover shortly after the open (a menu sound
    plays 250 ms after it). Not verified.
- Both runs FAIL on one expect only: the capture's export came after the
  script's last line. The scenarios wait 60 s at the end since; not rerun.

## 14. The first headset run (Nadi, Windows, 2026-11-20 21:37) and what was changed for it (2026-11-21)

Nadi: opening by the left B worked, the laser worked; "warum ist dann der
ganze körper weg. nur die hand war da. und auch iwie an einer komischen
stelle am rand"; the touch "noch nicht"; "die geste mit brust und ausrollen
ging gar nicht".

Legend: [R] read in the run's OBVR.log, [D] derived, [G] guessed. The run was
also the one whose device came up at the game's own 2560x1440 with the
dual pass (rendering-and-stereo.md, the CreateDevice slot) - the picture
was soft and at 60 Hz, whatever the tablet did.

**The body gone, only the hand left.**
- [R] At every tablet open, and at both container menus before, the body
  check read "the head 2010.9 units from it ... the head off any body's:
  the body is left hidden this frame and the IK stands down" (1672 to 2419
  units, drifting by what the player had moved) and "sane" again as the menu
  closed. Hidden for the whole menu; with the IK stood down the first-person
  hands are drawn without an arm. The harness had it too and nobody looked:
  tablet-walk of 2026-11-20 (20261120-133921-slot3) reads "the head 862.6
  units from it" the frame "the player's controls are gated again as
  vanilla", after 882 units walked, and its picture after the walk shows the
  hand as a stump.
- [D] Common to all of them: a menu up with the world running, so the
  engine's update of the player (0x0040DC61's gate) does not run and the
  third-person skeleton is not animated; some other pass of the frame
  brings part of its world transforms up to date from the local ones
  OBVR's give-back left (the body where the engine has it, not under the
  headset) and leaves the rest. Which pass, and why the engine's place for
  the body lies that far off, is not read. The stationary harness runs
  never showed it because there the engine's place and the headset's are
  the same spot.
- **Changed:** a head or an arm reading off before anything is written is
  read again after the engine's own update of Bip01 (NiAVObject::Update
  0x00707370 on the bone, which walks the whole subtree - 0x0070A0D0), and
  only that reading decides (game/BodyPlacement.h `RefreshForVerdict`,
  body_placement_test; game/PlayerBody.cpp). A skeleton that still reads
  off stays hidden, as before, and says "(read again after the engine's own
  update of Bip01)". Log when it was the stale reading: "Body check: the
  head read N units from Bip01 ... before the engine's own update of Bip01,
  M and sane after it - world transforms out of date". Cost: one more
  skeleton update in such a frame only.
- Not verified: no run since. tablet-walk.txt now expects that line and
  rejects "the head off any body's". [G] While the head read within 30 to
  200 units the same partial update went unnoticed and may have moved the
  body by a few units from frame to frame under a menu.
- **The hand "at an odd place at the edge":** [D] the left hand holds the
  tablet at its side edge (section 10, asked for on 2026-11-18), and without
  the arm it floats there. Whether it still sits wrong with the arm drawn
  is the next headset look.

**The touch.**
- [R] In 14 minutes: 9 times "the finger pressed the menu", of them 0 taps,
  5 "gone from the glass - nothing clicked", 2 "held still", 3 sideways
  swipes, 1 scroll. The log did not say how a touch ended or what it
  measured.
- [D] The glass gives no resistance: a finger goes through it and comes
  back a little, without the player knowing. The touch ended only once the
  tip was back 4 cm in front of the glass and was cut off 6 cm through it;
  the way in at an angle reads as a move across the glass (5 cm deep at 30
  degrees is 2.9 cm sideways, and 1.2 cm sideways was a page). Meta's
  touch specification describes the same and ends a press on the pull
  back from the deepest point (developers.meta.com/vr/design/touch_specs,
  read 2026-11-21: "the user's hand can end up tracked below the virtual UI
  plane without realizing it"; recoil assist's exit and re-enter distance
  0.02 m).
- **Changed** (`vr/TabletTouch.h`, tablet_touch_test, menu_place_test):
  - the contact (`StepTabletContact`): a touch ends as the tip is pulled
    back 2 cm from its deepest point (5 cm while it scrolls or drags) or is
    4 cm in front of the glass; through the glass it goes on to 25 cm, past
    that it is a hand behind the panel; after a touch that ended behind the
    glass the next begins when the tip goes in again by 2 cm, or from the
    front; a hand that went far through touches again only from the front;
  - a tap may wander 2 cm (1.2 before); a page needs a swipe of 4 cm
    sideways (1.2 before); a move past 2 cm that is neither is let go with
    nothing clicked ("moved and let go");
  - the log says how each touch ended (lifted off in front, pulled back
    behind the glass, off the edge, too far through, the index curled or
    the trigger pulled) and its measures: seconds on the glass, the
    furthest move, the deepest point. The wrist menu and the big quad keep
    the old poke.
- Numbers are guesses again; the next headset log has the measures to set
  them by. tablet-touch.txt: a tap 8 cm through the glass pulled back 3 cm,
  and the cut-off moved to 30 cm through - not run.

**The scroll gesture.**
- [R] Five tries (both grips closed within a frame or two of each other,
  then opened): at three the left hand was 15 cm from the eyes (10.2 to
  10.4 units in the camera's frame), at one the tablet was just closing,
  at one the hand was 46 cm out. No "the scroll sends the menu key" line.
- [D] The pose asked for the hands 15 to 75 cm ahead of the eyes; at the
  chest they are about level with the eyes' plane, under a bowed head
  behind it. Why the try at 46 cm did nothing is not known - the log had no
  line for a try that was not taken.
- **Changed** (`vr/TabletScroll.h`, tablet_scroll_test): the hands from
  10 cm behind the eyes' plane (`minimumForward` -0.10; replaced the next
  day by the inventory ring, section 15); both grips closed
  with the hands within 40 cm of each other and the scroll not begun says
  why, with the measure - "Tablet: both grips closed with the hands
  together, the scroll not begun - the hands together with open grips for
  0.10 s first (0.35 wanted)", or the distance, the place, a hand holding
  something, the tablet off.
- [R] The grips' other meanings ran beside each try: the off hand's block
  ("the left hand is up - blocking"), the grapple's reach, the right
  hand's grab, a fist. Not changed; section 8's "not settled" stands, and
  the refusal line is what the next run reads them against.

ctest 282/282 on Windows (MSVC, Release). Not run: the game, any
harness scenario (no harness on this Windows session), the headset.

## 15. The scroll begins in the inventory ring (2026-11-22)

Asked (Nadi, 2026-11-22): "lass uns die an den stow spot koppeln. also statt
10/15 cm geht es nur dort wenn ich die geste mach. das dann auch in die texte
eintragen."

- The pose: both hands empty, within 0.2 m of each other, **both drawn hands
  in the inventory ring** - the gold ring where a held item let go goes into
  the inventory (vr/Stow.h, `[Hands] StowRight/StowForward/StowUp/
  StowRadius`, placed by the player in the guide or the settings' "Place the
  inventory ring"). The window ahead of and below the eyes is gone
  (`minimumForward`, `maximumForward`, `minimumHeight`, `maximumHeight`). The
  dwell (0.35 s with open grips) and everything after the grips close stay
  as they were: once unrolling, the hands go anywhere.
- Measured in the ring's own frame (vr::BodyRelative: right, forward, up from
  the eyes along the head's level heading), the same as stowing, so the ring
  the player sees is where the gesture works; the ring's reach is its own
  (Nadi's 0.20 m). `vr::ScrollFrameOf` hands the place over from
  `HandSettings::stow`, so a ring moved in the game moves the gesture with
  it. The ring need not be shown, nor stowing on: the place is used either
  way.
- Nadi's ring sits at 1 cm right, 6 cm behind the eyes' plane, 19 cm down,
  40 cm across (his live INI). The three tries at the chest of the run of
  2026-11-20 (section 14), by the left hand's place in the camera's frame
  as the grapple line gives it: one 17 cm from the ring's centre (in it),
  two 21 cm (1 cm out) [D - the camera's frame carries the head's pitch, the
  ring's does not, so this is near, not exact]. The next headset log's
  refusal line says how far out a try was.
- The log's refusal names the ring: "a hand 0.05 m outside the inventory
  ring (both in it)" - how far the further hand is out of it.
- The words: the guide's inventory ring page and the ring's window from the
  settings ("Grab with both empty hands in it and pull them apart: your
  inventory opens."), and the settings row "Unroll the hand menu" ("Grab with
  both empty hands in the inventory ring and pull them apart: sideways, or up
  and down"). The controls card has no free line on its pages; not changed.
  [G] With "Traditional inventory" on, the guide's sentence promises a
  gesture that does nothing - the B opens the menus then; not split by that.
- Tests: tablet_scroll_test (in Nadi's ring at the chest, the same hands
  with the ring elsewhere refused with the measure, the old place half a
  metre out refused, one hand out, the ring turning with the head, above the
  eyes), menu_place_test (the scroll through the hand mode with the ring at
  the hands), native_settings_test (the ring page and its window), the
  settings rows. tablet-scroll.txt and tablet-scroll-vertical.txt pin the
  ring where their hands are and the first adds a try 25 cm out of it,
  expecting the refusal's line - not run.

## 16. Held in the palm, measured at the palms (Nadi, Windows headset 2026-11-22)

Nadi: "das inventar tablet ist am handgelenk befestigt statt in der
handfläche. fix." And the scroll gesture did nothing.

- [R in the code] The tablet was placed at the drawn hand's place on its
  controller (`DrawnHandOnController`) - the hand bone, which in Oblivion's
  skeleton is the wrist. The palm is some 5 cm further on.
- [R in the log] Every try at the gesture (2026-11-22, five in a row) was
  refused for the distance: "the hands 0.29 m apart" to 0.35 m. The hands
  were measured at the same bone. Nadi's grip puts the drawn hand 17 to 19 cm
  behind the controller (`LeftHandGripY` -0.169, `RightHandGripY` -0.188):
  with the controllers turned towards each other the two wrists swing apart
  by that lever [D].
- **Changed:** each frame, right after the fingers are posed, the palm's
  middle of each drawn hand is read from its bones - halfway from the hand
  bone to the middle finger's base, as `game::PalmCentre` - into its
  controller's frame, eased as the index tip is (vr/FingerTip.h
  `PalmBetween`, camera/hands/LaserTargets.cpp; logged once: "Hands: the left
  palm's middle read - x y z m in its controller's frame"). The tablet is held
  there (`vr::TabletHoldPoint`, in the hand and kept by the right hand), the
  unrolling tablet stands between the two palms, and the scroll measures the
  hands there (`vr::HoldingHandAt`: the distance and the inventory ring). Not
  read (bones not found), the hand bone as before.
- Tests: menu_place_test (the page in the palm 5 cm on from the wrist, each
  hand's own palm, the fallback; two controllers turned in with Nadi's grip:
  the bones 37 cm apart, the palms 16 cm), tablet_touch_test (the palm's
  middle). Not run in the game: whether the palm read is where the hand is
  seen holding it, and whether the tablet then sits right in the hand, is the
  headset's.

## 17. After the tablet: the Esc menu cut off, the item info cut off, a box in the room (Nadi, Windows headset 2026-11-22)

Nadi: "Esc menü ist nun nicht mehr im richtigen format vor mir sodass ich
keine buttons wählen kann. die rechte hälfte ist größtenteils abgeschnitten"
(critical); "die info text der sich rechts vom inventar einblendet ist nun
abgeschnitten! der kommt bei items oder bei stats"; "dialog boxen im inventar
sollen auch über dem inventar erscheinen aber ich kann diese nicht ansteuern
mit laserpointer sondern muss die irgendwo im raum vor mir suchen".

**The Esc menu.**
- [R in HudLayer.cpp, f189392] One flag, `m_tabletBoundsApplied`, meant both
  "send the tablet's bounds again" and "the compositor has the tablet's
  bounds". The tablet's placement, given every frame, cleared it
  (SetTabletPlacement after SetWristPlacement cleared `m_tabletSet`); while
  the scene draws the tablet the overlay is hidden and not placed. When the
  tablet went, the flag read false and the layer's own bounds never came
  back: the quad kept showing the tablet's cut (63..1825 of 4036 px, its left
  44 %, upright) for the Esc menu.
- [R in the logs] 22.11: the laser hit the Generic boxes before any tablet
  (2062,1484), "hit NONE" on every Esc menu after it. 20.11: the Esc menu hit
  before the tablet, after it only at the right edge or nowhere. 16.11 (no
  tablet yet): hit at 2002,1351.
- [R] Second cause: on the wrist or the tablet the overlay's room anchor was
  marked held (for the head-relative anchoring's sake); in the room's
  anchoring that kept the old anchor and the overlay stayed on the left
  controller until the next menu opened.
- **Changed** (render/HudPlacementLogic.h, hud_placement_test): the bounds
  are decided from what the compositor was last given (`StepHudBounds`:
  the tablet's cut when it is the tablet and not already shown, the layer's
  own once it goes); off the wrist in the room's anchoring the anchor is
  dropped and taken again where the head looks, unless one was placed on
  purpose meanwhile (`KeepAnchorOffWrist`). The test replays the run's frames
  with the old flag (the cut kept on the Esc menu) and the new step (the
  layer's own bounds back).
- Not shown in the harness: the stand-in runtime drops
  SetOverlayTextureBounds (tools/mock-openvr/MockOpenVR.cpp), so a run
  without SteamVR cannot show the overlay's bounds.

**The item info beside the page.**
- [R] The tablet's face was the page and its tabs plus the pictures of the
  menus in the stack. The item info is MagicPopupMenu, shown beside the page
  on a hover; it does not stack (NorthernUI's magic_popup_menu.xml:
  `does_not_stack`) [R in that file; that the game's own works the same is
  derived]. [R in the log] Every page read said "shown with 0 popup(s)".
- **Changed** (`vr::TabletFaceMenus`, tablet_test; game/TabletMenu.cpp): the
  item info's pictures join the face whether it is in the stack or not;
  game::ReadMenuPaper counts only shown tiles and pictures that are not
  clear. Logged: "Tablet: the item info (MagicPopup) shown beside the page -
  the face x..y units across" and "gone". [G] Unverified: that the popup's
  pictures read hidden when it is not shown (if they do not, the face stays
  wide and the line never says "gone" - the next log says); and whether the
  stats page's info is this popup or the stats menu's own stat_pop picture,
  which sits inside the page.

**A box over the tablet.**
- [R in the log] The box was a Generic menu (0x3F3) with qadd_* tiles - the
  tutorial's help box - one frame after the stats page; the tablet did not
  carry Generic menus, so the menus went onto the layer's quad, still cut to
  the tablet and hanging from the left controller (the Esc menu's two
  causes). No message or quantity box came up in this run; those two the
  tablet carried already.
- **Changed** (`vr::TabletCarriesMenu`, `TabletMenuRunsWorld`, tablet_test):
  a Generic box over the pages is carried, its pictures joined to the face,
  and pauses the world as a message does. OBVR's own native menus are
  Generic too: opened over the pages they go onto the tablet with them.

Not run: the game, the headset; the harness cannot show the first.

### Open bug, 2026-11-22: tablet-open.txt red since the tablet sits in the palm

Linux, headless, the stand-in runtime, main 976812d (with fcf5e62's palm hold):
tablet-open.txt FAIL with and without the aim fix of 5f17585 (20261122-213328-slot1,
20261122-213506-slot2). The tablet opens ("the Inventory page read", "in the off hand,
drawn into the eyes"), but the scripted beam no longer meets its rows: missing
"Inventory: under the pointer index 5", "Hands: click sent to the HudMain menu" and
"Tablet: the Magic page read". Derived, not checked: the page moved from the wrist to
the palm, so the scenario's beam angles (tablet-sweep.txt) aim beside it; the scenario
needs its aim measured again. OBNG's tablet-open-hitch-late runs of the same day
(OBNG artifacts/shadow/tablet-open-hitch-late/20261122-204150, -204422, DLL efb31c39,
`Unroll=vertical`) read no page at all - not looked into.

## 18. After the tablet: message boxes and the Esc menu very small (Nadi, Windows headset 2026-11-23)

Nadi: "dialog boxen und manchmal auch Esc menüs werden manchmal sehr klein"
(critical).

- [R in HudLayer.cpp, fcf5e62] One flag, `m_wristApplied`, meant both "the
  wrist's width is with the compositor" and "the last placement was on the
  wrist"; the room's width was remembered apart (`m_appliedWidth`) and never
  told that the wrist had changed it. The tablet is placed on its first frame
  (0.34 m), then hidden while the scene draws it - its placement still given
  every frame. A width change meanwhile (the item info joining the face)
  cleared the flag. Put down, the overlay read "not leaving the wrist", the
  room's 1.6 m matched the width remembered, and nothing was sent: the Esc
  menu and the boxes showed at the tablet's width.
- [R in the log of 2026-11-23 22:26] The face changed while the tablet was up
  in five of its nine times (63..1825 to as far as 63..3067). The menus after
  those five (Generic, Pause, Message, Container) are the candidates for
  "small"; the Pause menu after an unchanged tablet (line 5870) is not. [D]
  The log held no width line, so which menu was small is not read.
- **Changed** (render/HudPlacementLogic.h `StepHudWidth`, hud_placement_test):
  one state, the width the compositor was last given and whether the last
  placement was on the wrist; a width is sent whenever it differs, and leaving
  is read from where it was placed last. The test replays the frames with the
  old flags (the Esc menu kept 0.34 m) and the step (1.6 m back). Each width
  sent is logged: "Hud: the overlay x m wide - ...", up to 40 lines.
- Not shown in the harness: the stand-in runtime does not keep the overlay's
  width. The headset is the check.

## 19. The figure cut off beside the item info (Nadi, Windows headset 2026-11-23)

Nadi: "wenn ich zeilen hightlighte sehe ich wie der info text der sich rechts
einblendet abgeschnitten ist und ich sehe auch die inv figur wie sie
abgeschnitten ist".

- [R in the code] The item info joins the face (section 17), and the face is
  one rectangle round the page and the popup (`JoinPopup`). The page was drawn
  as that one rectangle of the layer (`TabletPieces`, piece 0), with whatever
  the game draws inside it beside the popup.
- [R in the log of 2026-11-23] The game's figure stands at 0.545..0.913 of the
  screen (about 2200..3685 of the layer's 4036 pixels); the face reached from
  63 to as far as 2831 with the info up. [D] The strip of that rectangle above
  and below the info showed the figure, cut at the face's right edge.
- **Changed** (`vr::LayerPictures`, tablet_test): with popups up the page's
  outline and each popup are drawn as their own pictures (up to four kept,
  `kTabletPopupsMost`; more, the whole cut as before); what lies between them
  stays empty. The face, the touch and the beam's mapping are unchanged.
- Open: the info text itself cut off. Not found in the code or the log; the
  popup is read from its pictures, and text reaching past them would be cut
  [G]. Needs a picture of it (the harness's eye pictures, or a screenshot).

## 20. Held like the sword (Nadi, Windows headset 2026-11-23; decided 2026-11-24)

Nadi: "invenotry tablet ist immernoch am handgelenk und nicht in der
handfläch. der winkel zu dem es zeigt zu mir ist auch falsch. es sollte sich
wie eine seite papier in meiner hand anfühlen momentan muss ich in mein
handgelenk schauen um es zu sehen." Offered A (the page turning to the eyes
by itself) or B (the page fixed in the hand): "A ist nicht wie man das
erwartet. B ist go. das inv soll wie das schwert ausgerichtet sein."

- [R in the code] The page was turned by the controller alone: upright in
  the controller's frame, leant back by TiltDegrees (30), its face along the
  controller's +z. [R in the log of 2026-11-22] The left palm lies at -0.041
  0.012 0.134 m in its controller's frame - 13 cm along +z, so z runs along
  the Index's handle. [D] The face pointed down the handle at the wrist; to
  read it the wrist had to be turned to the eyes.
- [R in skeleton.nif, Windows' loose _1stPerson, read with pyffi 2026-11-24]
  The sword hangs on "Weapon" (child of Bip01 R Hand), the torch and the bow
  on "Torch" (child of Bip01 L Hand). Weapons lie along their node's y and
  are flat across its z (game/BodyIkLogic.h, MirroredAcrossLocalZ). In the
  bind pose the Torch node's y is the Weapon node's mirrored to 0.2 degrees,
  its x to 6.6, its z the other way round (173.4): the Weapon's z is the back
  of the hand, the Torch's the palm (palms down in the bind pose: the index's
  base ahead of the middle finger's). Linux loads the BSA's skeleton - not
  read [G: the same].
- **Changed** (vr/TabletLogic.h `HeldAxes`, `StepHeldAxes`,
  `PageLikeTheSword`, `PlaceTablet`'s `held`; vr/MenuPlace.h;
  camera/hands/LaserTargets.cpp): with the palm, each drawn hand's held-item
  node is read every frame - its y (the blade) and x (towards the knuckles) in
  its controller's frame, eased as the palm is, logged once per hand ("Hands:
  the left hand's Torch node read - blade x y z, knuckles x y z in its
  controller's frame"). The page's up is the blade; its right the knuckles'
  way, to the body's middle (the right hand's turned back, so the page reads
  the right way round); its face, right by up, the palm's side. It is read as
  a sword's flat is looked at: the palm to the eyes, the thumb up. Every hold
  turns it so; the hold only says which edge is in the palm. In the hand
  GripUp, GripForward and the tilt no longer apply (TiltDegrees still leans
  the scroll between the hands). The node not read, the page sits on the
  controller as before.
- Tests: tablet_test (each hold, the right hand the left's mirror image read
  the right way round, readings skew, short or long squared to the blade, the
  fallbacks, the ease), menu_place_test (through PlaceMenu and HeldByRight).
- Harness 2026-11-24 (tablet-sword.txt, tablet-info.txt, films; docs/hand-
  script-harness.md): with the palm to the eyes the page stands readable
  before them and follows the hand up and down; the DLL from before lays it
  across the wrist. [R in the log, Linux] left Torch blade 0.070 0.096
  -0.993, knuckles -0.040 -0.994 -0.099; right Weapon blade -0.143 0.396
  -0.907 - mirrored the two blades lie 18.4 degrees apart [D: the two drawn
  hands' calibrations differ]. Seen: the left hand covers the rows' starts at
  the lower left. The item info is whole at rest and misses its left part
  while it slides (section 19's open point, not settled).

## 21. The page on the hand that carries it (Nadi, 2026-11-25)

Nadi, on the films of section 20: "toll das ist besser. Jetzt aber das tablet
auf die tragende hand schieben sodass die hand nicht durchklippt, die hand
soll also hinter dem tablet sein und nicht andersherum wie es jetzt ist."

- [R in the code, 981fa47] Held like the sword the page lay in the flat
  through the palm's middle - a point on the bones' line, inside the hand
  (vr::PalmBetween) - its hold edge in the palm, the page across the knuckles.
  [R in the films of 2026-11-24 and in the run of the DLL from before,
  20261125-103111-slot1] The fingers stood before the page and hid the rows'
  starts; closed to a fist the hand lay on the page's lower left.
- [R with pyffi, tools/nif-bone-extents.py, the Linux game's loose hand.nif
  and femalehand.nif] How far a hand stands out of a flat through its bones is
  no one number: a finger link's skin lies 0.3 to 1.3 units off its bone's axis
  on the palm's side, the hand bone's own vertices 2.5 units (the male hand)
  and 3.0 (the HGEC female one) at the thumb's ball; the fingers' curl moves
  them, and a glove or a gauntlet is another model. [R in the harness log,
  20261125-102621-slot1, the save's bare hands, 814 vertices each] Out of the
  page's flat the left hand reaches 7.5 cm before its palm's middle open - its
  fingertips, 10.5 cm across the page from the palm's middle - and 6.9 cm
  closed (its curled fingers, 2.6 cm across), and 2.1 cm behind it; the right
  hand 6.4 and 1.7 cm. Closing, the fingertips sweep further out first: 7.9,
  8.3, 8.6, 9.0 and 9.2 cm at curls 0.05 to 0.25, then 8.9 at 0.40, 8.1 at
  0.50, 7.6 at 0.55 and 7.2 at 0.65. [D] So much for an open hand because the
  blade's flat is not the open hand's: the game's open hand has each link
  bent 4 to 30 degrees (game/HandGrip.h), and its fingers stand out of the
  held item's flat on the palm's side whether they are open or closed round
  it. The page so stands some 7 cm nearer the eyes than it did in section 20.
- **Changed.** The page stands before the hand as it is drawn, the hand
  behind it.
  - vr/HandReach.h (pure): `ReachOfHand` - the hand's drawn vertices against
    its palm's middle, along the face the page shows in that hand
    (`PageLikeTheSword`), for a physical left hand's face and a right one's
    (the two look opposite ways on the same axes); a vertex more than 20 cm
    out is a misread and the reading is not taken. `StepHandReach` eases it
    (0.3 a frame, as the palm), so the page moves out over a few frames when
    the fingers close. `StepHandReachClock` says when the model is read.
  - vr/TabletLogic.h: `HandReach`, `PageLiftMetres` (the reach and 3 mm),
    `PlaceTablet`'s `reach` - the centre moved out along the page's face;
    the page's turn, its size and where the hold's edge meets the hand across
    the page are as in section 20. vr/MenuPlace.h hands each role's own.
  - camera/hands/HandReachRead.{h,cpp}, called from StepIndexTips right
    after the fingers are posed: game::ReadHandVerticesNow - the read the
    hands' contact fit makes outside menus ([Hands] FittedHands; it does not
    run while a menu is up) - against each hand's palm and held-item node as
    the bones stand, in the world. Read on the tablet's second frame up (the
    menu's opening frame is the game's heaviest, section 13); then, with the
    fingers' curls told, when a finger of the holding hand has moved 0.04 of
    its curl since the last read (up to a quarter closed each 5 % of curl
    move the hand's foremost point 0.4 cm out, so 4 % is the page's
    clearance) - every third frame at most, so fingers moving fast or a
    flickering sensor do not cost a read a frame - and every 60 frames for
    what the hand wears changing under the tablet; with no curls told, every
    15 frames, as the contact fit reads. Said in the log: "Tablet: the left
    hand as drawn reaches x cm out of the page's flat ..." at the first
    reading and whenever it has moved 2 mm (40 lines at most), with where on
    the page the hand's foremost point lies and the index finger's curl; and
    at a hand-script mark the reach, the reads and the step's time since the
    last mark.
  - The hand's model not read (no vertices, a misread): the open hand's 7.5
    cm stands in. The held-item node not read: the page on the controller as
    before section 20, no reach applied.
- Cost [R in the harness, tablet-on-hand.txt run alone (`--exclusive`),
  20261125-095302-slot1, the DLL of this change]: with the hand still the
  step takes 0.0015 to 0.0017 ms a frame on average - five reads in 300
  frames, one every 60 - and 0.10 to 0.15 ms on a frame that reads; while the
  fingers close or open, 0.0062 to 0.0064 ms a frame (25 reads in 364). The
  read itself 86 us on the mean, 148 the longest ("Hands: the hand fit's
  read", 11 memory probes a read). The tablet's own menu read beside it
  0.0054 to 0.0057 ms a frame, as before. Beside three other games the worst
  frame of a stretch was 0.15 to 0.29 ms, once 1.6 ms - the contact fit's own
  reads show 1.2 and 1.7 ms in such runs outside the menu, so that is the
  machine's load. [R in Nadi's Windows log of 2026-11-23, the same read made
  by the contact fit] There the read costs more: "the hand fit's read - 120
  reads, 336 us a read on the mean, 641 the longest, 2.0 shapes and 67.0
  memory probes a read". [D] So in that game about 0.34 ms on a frame that
  reads, 0.006 ms a frame on average with the hand still, and at most a read
  every third frame - 0.11 ms a frame - for as long as the fingers move. Not
  measured under Windows with the tablet up.
- Tests: tablet_on_hand_test (the reach from vertices, either side, skew
  axes, the hand turned in the world, the refusals and the misread; the ease;
  the read's clock; the page before the hand in every hold and either hand,
  the stand-in and the node not read), tablet_test and menu_place_test
  (through PlaceMenu and HeldByRight). Written first: against the sources
  before they do not build (no vr/HandReach.h).
- Harness (tablet-on-hand.txt, films; docs/hand-script-harness.md): the
  tablet opened in the left hand with the palm to the eyes, the fingers
  closed and opened, the hand turned up and down and about the upright, open
  and closed. 2026-11-25, Linux, headless, the stand-in runtime, each
  scenario with the DLL of this change and with the one deployed before it
  (b7645e28):
  - tablet-on-hand.txt PASS (20261125-102621-slot1; alone,
    20261125-095302-slot1), the DLL from before FAIL on the two reach lines
    (20261125-103111-slot1). In the films no part of the hand shows before
    the page with the new DLL - held still,
    the fingers closing and opening, turned up and down, and turned about
    the upright until the page is seen nearly edge on, where the hand lies
    behind it; with the old one the fingers and the fist stand before the
    page throughout.
  - tablet-on-hand-gauntlets.txt PASS (20261125-094610-slot1; before FAIL,
    20261125-094614-slot2): the iron set put on with the tablet up, the hand's
    model read again within the second - 929 vertices for 814. Its reach on
    the page's side stays 7.5 cm (the gauntlet's fingers end where the bare
    ones do), the other side's grows from 2.1 to 4.9 cm. The gauntlet lies
    behind the page; with the old DLL it stands before it.
  - tablet-on-hand-left-handed.txt PASS (20261125-100047-slot2; before FAIL,
    20261125-094618-slot3): the off hand on the right controller reaches 7.5
    cm on a right hand's side of the flat and 2.1 on a left's - the side
    `PageLiftMetres` takes for a physical right hand - and lies behind the
    page open, closed and turned.
- Not changed, open: the scroll between two hands while it unrolls (its
  ends are in the fists, as a scroll's rolls are); let go, the page moves out
  to the hand's reach on top of section 20's turn - a jump, not smoothed
  [D, not seen]. The older scenarios that aim a beam or a finger at a page in
  the hand by fixed angles (tablet-open.txt, tablet-touch*.txt) aim behind
  it now. The scroll kept by the right hand of a right-handed player is
  covered by the unit tests and by the left-handed run's right hand, not run
  by itself. The headset is the check for how it feels.
- **Changed with it, found while reading the code** [R in the code; D: not
  seen in the game before]: held at its side by a physical right hand
  (left-handed, or the scroll kept by the right hand) the hold's edge was the
  face's right edge - and the face grows with a popup, so every info box
  sliding out moved the whole page to the left by the box's width and back
  as it went; the stats page's own box (section 22) would have added one
  more. The hand now holds the page's outline, with the figure's own page
  where that stands beside it (`HoldFace`); a popup reaches out past the
  hand. tablet_test (either hand, a box on either side, the figure's page, no
  outline read, held like the sword, held at the lower edge). [R in the
  films of tablet-on-hand-left-handed.txt, a message box raised over the
  tablet in the right hand] With the DLL from before the page jumps to the
  left by the box's width as the box comes up (20261125-094618-slot3); with
  this one it stays and the box reaches out past the hand
  (20261125-100047-slot2).
  Open from that film: the box then hangs over the forearm, which crosses it
  and hides part of its button. A popup could go to the page's other side
  for a right-hand holder - the face's pictures need not lie where the game
  draws them, as the figure's page shows; not built. Answered in section 23:
  with the figure behind a tab or off, the boxes come onto the page.

## 22. The stats page's own info box (Nadi, 2026-11-25)

Nadi: "nein nicht ganz richtig die infobox erscheint im stats tab für ein
paar Einträge du hast die nur abgeschnitten. Also nächste arbeit ist den
stats tab zu verbessern sodass die infoboxen wieder sichtbar sind und nicht
abgeschnitten." (2026-11-22 already: "der info text der sich rechts vom
inventar einblendet ist nun abgeschnitten! der kommt bei items oder bei
stats"; the harness of 2026-11-24 saw no box on the stats page and took it
for none.)

- [R in stats_menu.xml, Oblivion - Misc.bsa, tools/bsa-extract.py] The stats
  page's description of an attribute or a skill is no MagicPopup: it is a
  picture of the stats menu itself, `stat_pop_background`, a child of the
  menu's root with id 61. 419 units wide, its height the text's lower edge
  and 25, at y 170; it slides out from behind the page's right edge as its
  user0 goes from 0 to 1 (0.24 s) and is clear while that is 0; at rest it
  starts 20 units inside the page's right edge and ends 399 beyond it. The
  page is 745 by 792.
- [R in Oblivion.exe 1.2.416, objdump] StatsMenu::SetField at 0x005D9BE0
  (the menu's vtable 0x00A6D9DC, slot 1) keeps the tile of id 61 at the menu's
  +0x58, the focus box of id 60 at +0x54, the pages and their panes of ids 10
  to 52 before them; 0x005D9B80 checks all thirteen. The id is the engine's
  name for the box, whatever a UI mod calls it or lays it out
  (../OBNG/knowledge/stats-menu-info-box.md).
- [R in the code] The page's face was the menu's largest picture and every
  picture of at least 0.4 of its area (`PaperOf`, for the map's extension).
  The box is 419 wide: under that share unless its text makes it 563 high.
  [D] So for most rows it stayed out of the face - cut off whole at the
  page's edge - and for a long text it joined the page's outline. [R in the
  log of 2026-11-24 22:04, tablet-info-rest] "the Stats page read ... 0
  popup(s), the face 40,96..1161,1388" through the whole sweep: the face
  never grew.
- **Changed** (vr/TabletLogic.h `PaperTile::popup`, `PagePopupTileId`,
  `PaperOf`; game/HudTiles `ReadMenuPaper`'s `popupTileId`;
  game/TabletMenu.cpp): the walk over the page's pictures marks the tile of
  the page's own box id and everything under it; `PaperOf` leaves those out
  of the page and its outline, however large, and joins them together as a
  popup - kept on its own and drawn as its own picture like the item info
  (section 19). Logged as the item info is: "Tablet: the page's own info box
  shown beside the page - the face x..y units across" and "gone". The other
  three pages have no box of their own (id 0: nothing marked, as before).
- Section 17's guess is answered: the stats page's info is the stats menu's
  own picture, and it does not sit inside the page.
- Tests: tablet_test (the box and its pictures a popup, the page and its
  outline without it, a tall box no part of the outline, one still behind
  the page, one larger than the page, one off the screen, none without a
  page; unmarked - the read from before - cut at the page's edge). Written
  first: against the sources before they do not build (no `PagePopupTileId`).
- Harness (tablet-stats-info.txt, films), 2026-11-25: PASS with the DLL of
  this change (20261125-094622-slot4), FAIL with the one from before, aimed
  for its own place of the page (20261125-095006-slot1: "the page's own info
  box shown beside the page" missing). [R in the logs] Each tab's icon is hit
  ("click sent to the Stats menu ... the tile under it stat_tabs_p1", p2, p3,
  in both runs); with the new DLL "the page's own info box shown beside the
  page - the face 27..800 units across", the face growing from 1161 to 1765
  layer pixels (772 to 1173 units) as the box slides out, and back as it
  goes. [R in the films] On each of the three tabs the box slides out beside
  the page and stands whole - the race's, the birthsign's and the class's
  with their pictures, the level's, health's and magicka's text, an
  attribute's picture and description, a skill's with its governing
  attribute and level; with the DLL from before the rows highlight and no box
  is seen. tablet-info.txt and tablet-info-rest.txt PASS with the new DLL
  (20261125-100024-slot1, 20261125-100028-slot3): the item info beside the
  inventory, the stats page's box, the map.
- The DLL as deployed (OBVR.dll md5 32026f9e from main a9ff91a, in both
  games since 2026-11-25 10:34): every scenario of sections 21 and 22 PASS
  with it - tablet-on-hand.txt in its film run (20261125-102621-slot1), the
  others in plain runs after the deploy: tablet-stats-info.txt
  (20261125-104501-slot1), tablet-on-hand-gauntlets.txt
  (20261125-104505-slot2), tablet-on-hand-left-handed.txt
  (20261125-104509-slot3), tablet-info-rest.txt (20261125-104513-slot4) and
  tablet-info.txt (20261125-104702-slot1). The films of those five named
  above are of earlier builds of this change from the same morning; [R, git
  diff] the tablet's sources of the later of them differ from the commit in
  comments only.
- Not checked: a UI mod's stats menu (the id is the engine's, [D] it holds);
  the headset.


## 23. The boxes on the page (Nadi, 2026-11-25)

Nadi: "warum kommt die box seitlich ans tablet. wenn wir das tablet
zweigeteilt per option haben (figur auf einem tab und tablet auf anderer)
dann sollten alle msg boxen auch mittig vom tablet kommen statt seitlich.
das löst auch das linkshänder problem." Then, shown the infos on the page:
"die infoboxen nicht mittig, die können seitlich bleiben".

- Why beside [R in the menus' XML, Oblivion - Misc.bsa]: the tablet shows
  each box where the game draws it on a flat screen. The item info
  (magic_popup_menu.xml) and the stats page's own box (stats_menu.xml,
  section 22) slide out from behind the page's right edge. A message
  (message_menu.xml), a quantity box (quantity_menu.xml), a Generic box
  (quest_added.xml, OBVR's own menus) and a book (book_menu.xml) stand in the
  screen's middle, and the page stands left of it. Held by a physical right
  hand at the page's right edge, they hung over the forearm (section 21).
- **Changed** (vr/TabletBoxes.h, pure), when nothing of the tablet's own
  stands beside the page - `[Tablet] Figure` tab (the default) or off - and
  the tablet is drawn into the eyes (`BoxesOnPage`). With the figure beside,
  and on the overlay (InScene=0), the boxes stay where the game draws them.
  - A box that waits for an answer or is read - a message, a quantity box, a
    Generic box, a book (`TabletBoxMoves`) - is moved by the game onto the
    page's middle. game/TabletMenu.cpp shifts its menu's root tile by the
    distance from its pictures' middle to the page's (`ShiftOntoPage`, half a
    unit or more). The game then draws it there and finds its buttons there:
    the beam and the finger press them where they are seen, and the tablet
    maps nothing. [R in Oblivion.exe 1.2.416] Tile::UpdateFloat 0x0058CEB0
    gets the trait's value from 0x0058B220, which makes one when the tile
    has none, and sets it with 0x0058CA00. A formula the trait has stays and
    sets its own value again when what it reads changes, so the shift is
    checked every frame. The tiles under the root count from its place only
    when it is a locus (game::ReadMenuPaper's rule); every vanilla box's root
    is one, and a root that is not is left as the game has it (said once).
    The place before is kept (`KeptOriginal`) and put back when the box
    leaves the page while it is still up - the pages closed under it, the
    figure set beside - unless the game set its own meanwhile (`PutBack`).
  - The infos shown while a row is pointed at - the item info, the stats
    page's own box - stay beside the page (Nadi). A first build drew them on
    the page, centred across it and in the half the beam was not in, so the
    row pointed at stayed free (the stats page's box is 490 to 520 units
    high, two thirds of the page's 792); taken out again after the films.
  - Found with it [R in the probe, 20261125-150011-slot3: the right stick's
    flick from the stats page did nothing]: the stick does not turn the page
    while a popup is up (vr/TabletPages.h), and the item info (since section
    17) and the stats page's box (since section 22) counted as popups - so
    the page did not turn while the beam rested on a row with an info.
    `TabletFrame::popupUp` now counts only a box (`BoxOverPage`); more popups
    than kept count as one. The tab shows the page while a box is up, as
    before.
  - The face holds a moved box on the page; one wider than the page reaches
    past it evenly on both sides (the message box: 788 units for the page's
    745, 2 cm each side at 0.34 m). The hand holds the outline as before
    (`HoldFace`).
  - Logged: "Tablet: the Message box moved onto the page's middle - by x,y
    units, from ... (its root ... to ...)", "Tablet: the Message box gone from
    the page", "... put back where the game had it ...", "Tablet: the ...
    box stays where the game draws it - its menu's root is no locus ...".
- Tests: tablet_boxes_test (when, which boxes, the shift and what is put
  back, a box told from an info, the moved box in either hand),
  tablet_test (the info flag). Written first: against main 912f8d6 it does
  not build (no vr/TabletBoxes.h).
- Harness, Linux, headless, the stand-in runtime; the DLL of this change on
  main 715d79f (fc0e06a4) against main 715d79f alone (bcf499b8, the DLL
  deployed at 15:47):
  - The probe (20261125-150011-slot3, a first build): the beam stepped down
    the page's middle column over the moved message box - "message_text"
    from 330 to 370 of the page's 792, "button_1" (Yes) from 380 to 420,
    "button_2" (No) from 430 to 470: the game finds the box's parts where
    the tablet shows them.
  - tablet-boxes.txt PASS (20261125-161033-slot1), before FAIL
    (20261125-161053-slot2): with the stats page's box up beside the page
    "the right stick turns the page right from Stats" - missing before, the
    page stayed; "the Message box moved onto the page's middle - by -454,47
    units, from 460,347..1248,614", "click sent to the Message menu ... the
    tile under it "button_1"", "the Message box gone from the page" - before,
    the box stood in the screen's middle and the beam on the page's middle
    pressed nothing. Films 0 frames dropped.
  - tablet-on-hand-left-handed.txt PASS (20261125-161224-slot1), before FAIL
    on this section's line alone (20261125-161244-slot2): the face with the
    message box 8..1193 layer pixels across for 40..1876 - the box on the
    page in the right hand, not out over the forearm.
  - tablet-quantity.txt, aimed again for the page held like the sword (a
    probe, 20261125-154456-slot1: "Iron Arrow" under the beam from 167 to 213
    down the page at x 330): PASS (20261125-161442-slot1), before FAIL
    (20261125-161454-slot2): "the Quantity box moved onto the page's middle
    - by -454,47 units, from 510,306..1198,654 (its root 0,0 to -454,47)".
  - tablet-stats-info.txt, tablet-message.txt, tablet-info.txt,
    tablet-info-rest.txt PASS (20261125-161506-slot3, -161518-slot4,
    -161649-slot1, -161701-slot2): the infos beside the page as before, the
    message box moved.
  - Not run: a book, a Generic box (the same root shift as the message box;
    [D] they move the same way), the headset.

## 24. Nadi's Windows headset run of 2026-11-26 (14:09-14:44, main 8095bfd)

The run's log and INI: artifacts/win-run-2026-11-26/ in the main checkout
(OBVR.log, 37,913 lines). Line numbers below are that log's. [R] read or
measured, [D] derived, [G] guessed.

### 24.1 No walk under the tablet with a torch in the off hand

Nadi: "mit tablet in der hand konnte ich im tutorial dungeon nicht mit
linken stick laufen. in der welt draußen ging es."

- [R in the log] Every walk under the tablet in the tutorial's cells
  0001FBB9 and 00022FF6 said "Tablet: walked 0 units with it open" (lines
  11657, 11737, 12017, 12020, 12025, 23542), each after "the player's
  controls run - the sticks walk under menu F1-F4 stack entry (0x001) - the
  tablet's". Outside (after the load at 31135) it walked 336 and 196
  (35664, 35667). In the dungeon the torch was in the left hand at each of
  those opens ("Menu near shapes: first person - "Torch02:0"" at 11036 and
  11924; dropped at 23191 and taken back by the left grip at 23202, the held
  torch's contact lines after it at 23205 and 23213 [D: in the hand at 23540]);
  outside it hung on the hip
  ("Kept on body: the left hip shows 0002CF9F, a torch" at 30946 and 33562).
  [R] Not the interior: the harness save stands in TestingHall, an
  interior, and tablet-walk.txt walks there.
- [R in the harness, the DLL of 8095bfd] tablet-walk-torch.txt (new: the
  tablet-walk with a torch equipped into the closed left grip):
  20261126-154102-slot2 "walked 0 units"; the same scenario with the torch
  dropped (an open grip) walked 877 (20261126-153931-slot2). The Move trace
  ([Debug] WaterCaptureTrace): the player's Move from UpdateMovement
  (0x0063CB42) every frame with the walk's flags 513 (forward, run) and a
  step of 0.000; without the torch 4.4 to 5.5 units a frame.
- [R with a probe build, not committed, that logged both anim data at each
  Move] Under the tablet with the torch the first-person anim data (player
  +0x5CC) plays group 0x0007 (the forward run) in slot 0 with 4.8 to 4.9
  units of root motion a frame; the third-person one (the process's +0x17C)
  has slot 0 at 0x00FF with no sequence, slot 2 at 0x0021 (the torch's arm),
  no root motion (20261126-154425-slot1). Without the torch the third-person
  slot 0 is 0x0007 with its sequence too (20261126-154552-slot1).
- [R in Oblivion.exe 1.2.416, objdump] UpdateMovement (0x0063C730), for the
  player in first person, takes the step from the first-person anim data
  only while its slot-0 group equals the third-person one's (0x0063C7D6 ..
  0x0063C7EE: 0x00470720(0) of each, the word at +0x3C); else from the
  third-person anim data. The anim data's play of a sequence (0x00474530)
  asks IsMenuMode at three sites that, in a menu, treat the player's
  third-person skeleton (the anim data whose root at +4 is the player's own
  node: 0x00660110(0) -> 0x00612220 -> the reference's +0x3C) as a picture of
  one pose: 0x004745FE starts a sequence already playing again,
  0x0047462C clears slots 4, 0, 1, 2 and 3 (0x00470FC0 sets each group to
  0xFF) before setting the one played, 0x0047478A blends nothing. The
  fourth, 0x0047488D, is about the F1-F4 figure's own node (+0x5D8).
- **Cause** [D from the above]: the player's update never runs in a vanilla
  menu; under the tablet (stage 4) it does, and with a torch in the off hand
  its arm's group is played after the walk's and clears it, so the
  third-person slot 0 reads 0xFF, the groups differ, and UpdateMovement takes
  that skeleton's root motion - none. A reached container's walk (the same
  span) has the same rule [D, not run with a torch].
- **Changed** (game/MenuPausePolicy.h `PlayerSkeletonSeesMenu`,
  game/PlayerControlSites.h `kPlayerSkeletonMenuModeSites`,
  game/MenuPause.cpp): the three sites ask OBVR; while the player's update
  runs under a walked menu (the span from the controls gate 0x0040DC61 to
  0x0040DD5F) the answer is "no menu" - the skeleton keeps the walk beside
  the torch's arm as in the world - and vanilla's everywhere else. Redirected
  with the span's other sites at the first walk; said once: "Menu pause: the
  anim data's play (004745FE, 0047462C, 0047478A) asks OBVR at 3 of 3 sites".
  A site another mod holds keeps vanilla's answer (said; the walk without a
  torch is not affected).
- Tests: menu_pause_policy_test (the span, outside it, no menu). Written
  first: it did not build before the function.
- Harness, Linux, headless, the stand-in runtime: tablet-walk-torch.txt FAIL
  with the DLL of 8095bfd (20261126-155327-slot2: "walked 0 units", the
  sites' line missing), PASS with this change (20261126-155201-slot2: 877
  units, the step 4.8 to 5.0 a frame, the torch in the hand throughout).
  reach-walk-wheel.txt PASS (20261126-155557-slot2). tablet-walk.txt walks
  883 units with this change and 882 with the old DLL, and fails with both
  on its beam's click on the magic page's button (20261126-155443-slot2,
  20261126-155741-slot2): its fixed angles aim at the page's place before
  section 20 (the open point of section 21), not this change.
- Not run: the headset; a walk under a reached container with a torch; a
  shield or a bow in the off hand under the tablet [G: the same rule].

### 24.2 The closing frames: the menus floated ahead and the world went sepia

Nadi: "wenn ich das tablet schließe dann kam es noch im cinema mode vor mir
schwebend kurz für paar frames. diese weg."

- [R in the log] At every close: "Hands: menu none (0x000) - the hands
  stay" (menu mode on, the stack empty), "Tablet: put down - the menus as
  before", "Hud: the overlay 1.60 m wide - off the wrist again" (6240-6242,
  6421-6423 and on), and only then "Menu trace: a menu just closed".
- [R in the harness, a probe build, not committed, 20261126-160147-slot3]
  From the menu key on, menu mode stays on for 17 frames (0.24 s at 72 Hz)
  with the stack empty, and the 2D pass still draws 108 pictures a frame -
  the menus fading out (the tablet's frames drew 227, the world's HUD 27);
  the layer is captured each of those frames.
- [R in the code] The tablet's frame needs the pages' entry in the stack
  (vr::TabletMenusUp), so the tablet was put down at the first of those
  frames. With it down: the room's overlay (render/HudLayer) was placed
  "as before" - ahead of the head, 1.6 m wide, showing the layer, i.e. the
  fading menus; and the menu shade, off only while the tablet is up
  (CameraHook.cpp shadeWanted), tinted the eyes, since the world is paused
  and the frame is the stand-in's live menu frame.
- [R in the harness, the DLL of 8095bfd, tablet-close.txt,
  20261126-160458-slot4] At the mark 0.05 s after the closing press:
  "Render: OpenVR overlays shown (at the mark, menu none): obvr.hud", and
  the left eye's picture sepia (the mean colour 52 46 36 against 54 53 43 at
  the open and closed marks).
- **Cause** [D]: the menus' fade after the close ran with the tablet put
  down but menu mode still on, so it got the place and the dressing of the
  menus without the tablet - "the cinema", in front, tinted.
- **Changed** (vr/TabletClose.h, pure: `OnlyHudMenus`, `StepTabletClose`;
  vr::TabletFrame `menusGone`, set by game/TabletMenu.cpp when menu mode is
  on and only the HUD's menus are in the stack; camera/hands/ControlsOut
  steps it and keeps the room's overlay hidden at the tablet's place,
  render::HudLayer::HideTablet; CameraHook.cpp's shade also off while it
  closes, the line count kept): the frames that follow a frame with the
  tablet up, for as long as menu mode is on with only the HUD's menus left,
  are the tablet's still - nothing in the room, no shade; the tablet itself
  is gone from the hand at the menu key, as the menus are. Another menu in
  the stack (the Esc menu over the pages) is no closing: it gets its own
  panel as before. A stack read empty for a frame between two tablet frames
  is held the same way ("the pages back"). Said as it ends: "Tablet: closed -
  the menus' fade kept out of the room and the eyes (N frame(s))". Moved
  with it: the hand mode's reset of ControlsOut's state
  (hands::ResetControlsOut, three lines out of CameraHook.cpp), so a close
  cut short by the hand mode stopping leaves no shade switched off.
- Tests: tablet_close_test (the HUD's menus only, the closing frames and
  their count, another menu taking the pages' place, a menu closing without
  the tablet, the pages back after a frame). Written first: it did not build
  before vr/TabletClose.h.
- Harness, Linux, headless, the stand-in runtime (tablet-close.txt, new;
  EyeDump at the marks): FAIL with 8095bfd (20261126-160458-slot4: "(at the
  mark, menu none): obvr.hud" counted twice - at "closing" and at "closed" -
  and the closing line missing), PASS with this change
  (20261126-161104-slot3: at "closing" no overlay shown, "closed ... (12
  frame(s))", the closing picture 54 53 43 as the others). The pictures side
  by side: artifacts/hand-script/tablet-close/close-old-vs-new.png in the
  worktree. tablet-pages.txt and tablet-message.txt PASS
  (20261126-161231-slot2, 20261126-161401-slot2).
- Not run: the headset - SteamVR's compositing is not in a harness run, so
  the overlay is checked by the overlays the log names, not by a picture;
  the tablet as an overlay (InScene=0).

### 24.3 A box wider than the page cut off at the left

Nadi: "message boxen wo die texte und größer waren waren am rande
abgeschnitten".

- [R in the log] Two boxes were moved onto the page in the run, both the
  tutorial's Generic box (its help cards), 888 units wide: "the Generic box
  moved onto the page's middle - by -454,47 units, from 409,249..1297,711"
  (19816), then "by -22,0 ... from 0,296..844,758", -11, -6, -3, -1, -1
  (19824-19859: the root from -454 to -498), and the next page read "the face
  -105,151..1994,2182" (19817): the tablet's face began 105 layer pixels left
  of the layer. The same at 19969-19973.
- [R in the code, section 23] `ShiftOntoPage` centred the box on the page's
  middle - 399.5 units from the screen's left, as the page stands at 27..772
  - whatever its width. The box's pictures are read cut to the screen
  (vr::PopupRect, vr::OnScreen). The unit test even said so: "a book larger
  than the page: centred on it all the same".
- **Cause** [D]: a box wider than 799 units, centred there, starts left of
  the screen (the Generic box at -45), where the game draws nothing - the
  tablet showed its left part missing. Read cut at the screen's edge, its
  middle seemed right of the page's, and it was shifted on each frame, to 89
  units past the edge in the end (root -498 for -454).
- **Changed** (vr/TabletBoxes.h `KeptOnScreen`, `ShiftOntoPage` with the
  screen's size; game/TabletMenu.cpp): the box is centred on the page as far
  as the screen lets it - a box that would leave the screen stops at its
  edge, and wider than the screen it starts at 0. The Generic box now starts
  at the screen's left edge, 27 units left of the page, and reaches 116 past
  its right; the face holds it whole. A message box (788 units) still
  centres (5.5 units from the edge).
- Tests: tablet_boxes_test (the run's Generic box, a box read cut at the
  edge not shifted further, a book, a box too tall, a page near the right
  edge, a box as large as the screen; the old "centred all the same"
  replaced). Written first: they did not build before.
- Harness, Linux, headless, the stand-in runtime (tablet-box-wide.txt, new: a
  book opened over the tablet by the console - equipping a book reads it):
  FAIL with 8095bfd (20261126-161938-slot3: "the Book box moved onto the
  page's middle - by -454,47 units, from 277,52..1429,908", then -88, -44,
  ... to a root of -630, 353 units of it off the screen; "the face
  -417,151..2306,2258"), PASS with this change (20261126-162140-slot2: "by
  -277,47 units", once; "the face 0,151..2724,2258"). tablet-boxes.txt PASS
  (20261126-162315-slot2: the message box moved by -454,47 as before).
- Not seen in a picture: the eye dumps of a harness run do not show the
  tablet; the rectangles in the log are the evidence. Not run: a Generic box
  in the harness (the unit test has the run's own rectangle), the headset.

### 24.4 Turned towards the head, held at its middle, set in the game

Nadi: "inventory tablet muss 90 grad nach richtung kopf des spielers und
mittig in der hand. lass diese werte einstellbar dann kann ich ingame das
justieren danach. ansonsten ging da alles."

- [R in the code, sections 20 and 21] Held like the sword: the page's up the
  held-item node's y (the blade), its right the node's x (the knuckles, to the
  body's middle), its face right by up - the palm's side; held at its side
  edge (Unroll=sideways, the default and Nadi's: his INI sets no Unroll), the
  page across the knuckles.
- [R in the log, 2381] His left hand's Torch node: blade 0.322 0.302 -0.897,
  knuckles -0.241 -0.890 -0.386 in the Index's frame (-z ahead, +y up, +z
  back to him). [D] So the page's top runs along the controller's pointing
  way, its right edge down, its face +x - the palm's side. With the
  controller held as a hand rests, pointing ahead below the eyes, the page
  stands up out of the fist like a flag, its face to the body's middle: seen
  edge-on. [R in the harness, tablet-held-turn.txt, that pose: the left
  controller 32 cm under and 30 cm before the eyes, pointing ahead, the head
  35 degrees down] the face 87 to 89 degrees off the eyes. To read it the
  wrist turns a quarter, the palm to the eyes (the reading pose of section
  20).
- **The reading chosen** [D from the above; G as to what he meant - the
  headset decides]: "90 degrees towards the head" is that quarter turn of
  the wrist given to the page - about its upright, the blade, the face from
  the palm's side away from the knuckles. In his hand the face then looks
  along 0.24 0.89 0.39: the controller's top and back to him; the page lies
  over the fist like a phone held in the palm, its top ahead, its face up to
  the eyes. "mittig in der hand": its middle on the palm's middle, not its
  edge. In the pose above: 45 to 47 degrees off the eyes (with the
  controller level; [D] raising its tip 45 degrees, as one does to read
  something in the hand, faces them). The other readings are the other two
  angles below: a turn about the page's width (its top towards the head) is
  the lean, one in its own flat the tilt; -90 turns the face the other way
  (to the controller's bottom).
- **Changed** (vr/TabletLogic.h `HeldPlace` in `TabletSettings::held`,
  `TurnAxisPair`, `TurnHeldPage`, `PlaceTablet`'s sword path; vr/HandReach.h;
  camera/hands/HandReachRead and LaserTargets; core/ConfigTablet.cpp,
  Config.cpp's step list; OBVR.ini; ui/SettingsPartHands.h): held like the
  sword, the page is turned in turn about its upright (HeldTurnDegrees, 90),
  its width (HeldLeanDegrees, 0: + its top away from the side read, the face
  up) and in its flat (HeldRollDegrees, 0: + its top to the body's middle),
  each about the axis as it then stands and mirrored for a right hand, so a
  value means the same in either; its middle stands on the palm's whatever
  the hold, moved by HeldAcrossCm (to the body's middle), HeldUpCm and
  HeldOutCm (out of its face, past the hand's reach) - six [Tablet] keys,
  half a turn and 40 cm at most. A box beside the page reaches out past it;
  the page's middle stays in the hand. The hand's reach (section 21) is now
  taken along the face each hand's page shows as turned: unturned the hand's
  two sides, turned a quarter both away from the knuckles. The other holds'
  edges (the scroll's bottom and top) are used off the hand's bones only.
- Six rows in the game's settings, Laser & wrist menus: "Hand menu turn",
  "lean", "tilt", "sideways", "height", "distance" - numbers in steps of 5
  degrees or 1 cm, written to the INI as they change.
- **Live, also with the page up.** [R in the harness, the first build of
  this change, 20261126-164006-slot4] HeldTurnDegrees written with the
  tablet up was not taken while it stayed up - the harness doc had it since
  2026-11-20: "`ini-at` taken in a menu is read only after the menu closes".
  [R in the code] The INI's re-read (MaybeReloadConfig, every
  ReloadEveryFrames frames) ran from the engine's camera update only, which
  does not run in menu mode. **Changed** (CameraHook.cpp, the line count
  kept): it runs at the start of every presented frame, a menu's too, counted
  by the presented frames. When due it reads the files' time (one directory
  read); the full re-read only after an edit, as before - 135 to 162 ms in
  the harness, a stutter as in the world. The settings menu writes its
  changes into the file, so a re-read agrees with it.
- Logged: "Tablet: held turned T, leant L, tilted R degrees, moved A U O cm
  (across, up, out) - its face x y z in the left controller's frame, N
  degrees off the eyes" when the values change with the page up (40 lines)
  and at each hand-script mark (vr::FaceOffEyesDegrees) - the values Nadi
  settles on are in his log.
- Tests: tablet_test (the defaults, the quarter turn, his own axes, the
  right hand's mirror image, the place in centimetres, the lean, the roll, no
  turn, a box beside the page; the old checks of the sword's flat pinned to
  no turn, the edge holds' places now the middle), tablet_on_hand_test (the
  reach along each hand's turned face; the placement checks as above),
  menu_place_test (the default turn through PlaceMenu, FaceOffEyesDegrees),
  settings_list_test (tests/settings-rows.txt: the six rows, rewritten with
  --print). Written first: they did not build before `HeldPlace`.
- Harness, Linux, headless, the stand-in runtime: tablet-held-turn.txt
  (new) FAIL with 8095bfd (20261126-164537-slot3: no place said - it has no
  such values), PASS with this change (20261126-164349-slot3): turned 90 the
  face 0.04 0.99 0.10 in the controller's frame (the harness's left hand),
  45 degrees off the eyes; HeldTurnDegrees=0 written with the tablet up,
  "Config: OBVR.ini changed - re-read in 135.6 ms", the face 1.00 -0.05 0.07,
  87 degrees off; 45, 56 degrees; 45 and a lean of 20, 71 degrees (the lean
  tips this pose's face further from the eyes: here a negative lean faces
  them). The scenarios aimed for the place of sections 20 to 23 pin it:
  HeldTurnDegrees=0 and HeldAcrossCm=17 (half the 0.34 m page) are that place
  exactly (tablet-boxes, -quantity, -stats-info, -info, -info-rest);
  tablet-boxes.txt (20261126-164751-slot3), tablet-quantity.txt
  (20261126-164917-slot3) and tablet-stats-info.txt (20261126-165037-slot3)
  PASS so; tablet-info and tablet-info-rest not run again.
- Open, for the headset: how it feels and which lean and place he settles
  on; [G] a default lean of -20 to -45 may suit a level hand better - his log
  will say. The scenarios that aim at the page held at its edge before
  section 20 (tablet-open, tablet-touch*, tablet-book, tablet-walk's click)
  miss as before (section 21's open point). The right hand's hold and a
  left-handed player are covered by the unit tests, not run.

## 25. Books and letters in the hand (asked 2026-11-27, not built)

The ask (Nadi, 2026-11-27): "bücher und letters wie das inventar in die
hand mit swipes für nä seite usw." A book or a letter, read from the world
or from the inventory, sits in the off hand as the inventory's page does;
the other hand's finger swipes to the next page and back and taps Take and
Exit.

### 25.1 What is there today [R]

- The book menu (`kMenuIdBook`, 0x402) is a tablet menu only beside the
  F1-F4 pages: `vr::TabletCarriesMenu` lists it, but `vr::TabletMenusUp`
  wants the F1-F4 entry (`kMenuIdBigFour`) in the stack. A book read from
  the inventory is a box over the page (`vr::TabletBoxMoves`; tablet-book.txt
  PASS 20261120-110919-slot1). A book or note activated in the world has no
  F1-F4 entry under it and stands on the menus' panel in the room.
- Behind a book the tablet pauses the world (`vr::TabletMenuRunsWorld`
  leaves out the book and the message box). `game::MenuKeepsWorldRunning`
  already keeps the world running behind a book with `Render.UnpausedMenus`.
- A book in the world is let through to its activation, to be read
  (`vr::ActivateWithheld`, src/vr/Stow.h).
- The touch (`vr/TabletTouch.h`, section 12 and 14): up and down scroll by
  wheel notches (one per 2 cm, coasting), sideways past 4 cm turns one F1-F4
  page per touch by the page's key, a tap clicks on the release, a hold of
  0.5 s presses. The right stick left and right turns the F1-F4 page (11.1).

### 25.2 The game's book menu

Read in `menus/book_menu.xml` (the vanilla copy in
OBVR-wt-palm/artifacts/work-2026-11-25/menus/):

- One menu with two looks, chosen by its `user0`: a book
  (`book_background`, 1152 x 856 units: two pages of the text, page numbers,
  Prev, Next, Take, Exit) or a scroll, which is what a note or a letter is
  drawn on (`book_background_scroll`, 990 x 740: one page that scrolls, with
  a scroll bar, Take, Exit).
- `user8`: "clicksound for turning pages - used by code for scrollwheel
  movement". So the wheel turns a book's pages with the page sound and
  scrolls a scroll [R: the file's comment; the rival's docs/design/books.md
  section 3 says the same]. Not run in OBVR yet.
- `book_prev` and `book_next` have no id of their own (`&generic;`). The
  rival's design doc says their clicks feed the page step (`user6`, "force
  page/scroll adjust") [read there, not checked]. Take is id 32 and Exit
  id 31; the scroll's own are `book_scroll_take` and `book_scroll_exit`.
- NorthernUI and DarNified UI bring their own book_menu.xml (both in the
  Painted World mods folder): names and sizes are read at run time, never
  constants, as the page's crop already does.

### 25.3 The rival has it [read in its docs, not seen run]

Chasms VR Oblivion (github.com/chasmlol/chasms-vr-oblivion, MIT;
docs/design/books.md, docs/OBLIVION.md row "Books and notes"):

- A book taken in a hand opens at once over the book's model; one read from
  the inventory is held in the off hand like a sheet; one activated on a
  shelf comes to the off hand. Take and Exit are pressed with the free
  hand's fingertip; let go, Exit or B close it.
- The page turn took nine headset rounds. A fingertip swipe "still missed";
  what stayed is the free hand's grip near the pages, dragged across them:
  one page every 5 cm, with a tick in that hand and the game's page sound.
- The pages shook with the hand and were hard to read: steadied by a
  one-euro filter, heavy while the hand is nearly still.
- The size: a line of the book's text at about 1.4 degrees at 45 cm, which
  makes the book's art about 38 x 28 cm [their arithmetic].
- The world plays on: the book menu is kept off the interface's menu stack.

Two lessons for OBVR: watch for the shake, and keep a second way to turn
the page in case the finger's swipe misses.

### 25.4 Design

1. **Which.** The book menu alone in the stack (beside the HUD's entries) is
   a tablet stack as well: `vr::TabletMenusUp` is true for the F1-F4 entry
   or the book menu. That covers a book, a note and a letter from the world,
   and a book from the inventory as before. A magic scroll is not read (its
   activation takes it), unchanged.
2. **Where.** In the off hand, at the page's place (sections 20 and 21),
   cropped to the book's own pictures (`game::ReadMenuPaper` on the book
   menu when no pages are up: its largest picture is the paper).
3. **Size.** Its own width, `[Tablet] BookWidthMetres`, default 0.40 m.
   [D] The rival's 1.4 degrees a line at 45 cm gives 0.38 m for the book's
   1152 units; the page's scale (0.34 m for 745 units) would make it
   0.53 m, [G] too wide for one hand. The letter's scroll takes the same
   scale (990 units, 0.34 m).
4. **Swipes.** On a book, sideways past 4 cm (the page's threshold) turns
   once: the finger moved left goes on, as a page is turned; moved right
   goes back. Sent as one wheel notch, the menu's own turn and sound; if the
   probe (25.6, task 1) shows the wheel does not turn it, a click on the
   middle of `book_next` or `book_prev` instead. Up and down on a book do
   nothing [D: a wheel notch there would turn pages too]. On a letter, up
   and down scroll as a list does (notches, coasting); sideways does nothing.
   A tap presses Take, Exit, Prev and Next.
5. **The stick.** The right stick left and right turns a book's page, as it
   turns the F1-F4 pages (11.1); up and down scroll a letter.
6. **Closing.** The left B, as for the tablet, clicks the menu's own Exit
   (`book_exit` or `book_scroll_exit`, what its `xbuttonb` does). Take closes
   it as the game does.
7. **Steadying.** Not built at first: the tablet's page has no filter and no
   shake has been reported. If the headset shows the text shaking, a light
   filter on the held place while the hand is nearly still (the rival's
   lesson).

### 25.5 Open for Nadi

- **The world while reading:** (a) runs, as behind the inventory: walking
  and looking round while reading, a fight goes on; (b) pauses, as vanilla
  and as today. Recommended (a), so the book in the hand behaves like the
  tablet. `MenuKeepsWorldRunning` already runs the world behind a book with
  `UnpausedMenus`, so no new engine risk is known.
- **Later, not in this ask:** a book picked up by hand opens on the book in
  the hand (the rival's way). Medium: the page placed on the held object's
  node, and its activation started from the grip.

### 25.6 Tasks and cost

Small to medium in total. Reused: the place in the hand, the crop, the
touch, the pause gate, the stick. New: the book menu alone in
`TabletMenusUp`, a book mode in `vr::TabletTouch` (sideways gives a notch,
up and down off on a book), `BookWidthMetres`, B as Exit.

1. Probe first, in the harness: open a book in the world, send one wheel
   notch each way, read its `_pagenum`; on a letter, read the scroll bar's
   value; try B, Tab and Esc for the close. This settles notch or click.
2. Unit tests first: `TabletMenusUp` with the book menu alone;
   `TabletMenuRunsWorld` as decided in 25.5; the book mode of the touch
   (left and right give +1 and -1 notch, up and down nothing on a book, a
   scroll on a letter); the width.
3. Build. Harness: book-world-in-hand.txt (a book from a shelf: the tablet's
   place said, a swipe left reaches pages 3 and 4, right back to 1 and 2,
   Take puts it in the inventory), letter-in-hand.txt (an up swipe moves the
   scroll bar), tablet-book.txt again (from the inventory). Films before and
   after: the book on the panel in the room, then in the hand.
4. Headset: the text's size at 0.40 m, the swipe's 4 cm on a book, any
   shake.

Model: Sonnet 5.5 medium. It is mostly reuse of tested modules; the one
engine unknown, whether a notch turns the page, is settled by task 1.

## 26. Windows run of 2026-11-27 (Nadi, headset at 90 Hz, OBVR.dll from d96cacb)

Three reports from the run (OBVR.log 13:38-13:55). Marks: [R] read in the
code, the log or the EXE's decompiled functions, [D] derived from those,
[G] guessed, not checked.

### 26.1 The figure flickers - its rectangle held steady, and an instrument

Nadi: with the figure on its own page at the right, "the edge round the
figure still flickers"; with the figure's tab chosen at the top, "the figure
flickers in the tablet".

- [R log] He tried both: `[Tablet] Figure` was set to beside at line 3276
  and back to tab by 4112; the figure's page shows at 3362, 3690, 3834 and
  goes only while an info box is up (3578, 3765), which is by design (the
  game draws the box where that page would stand). The figure's tab was
  chosen at 12554 and 18700 and left by the player's own press (12632);
  the tab chosen did not flip by itself.
- [R log] Nothing in the log alternates frame to frame for the figure: the
  figure's rectangle was said only for the session's first three reads
  (lines 2178, 2317, 2392, all 0.544-0.545..0.913 of the screen), the fit's
  distance only when the bound changed with an equip (16957-17464, each
  right after a tap), and the 2D pass's draw count ("Hud pass trace", 220
  against 231) is logged in bursts after a menu change, not per frame.
- [R code] The figure's rectangle is read every 15 frames from its bones
  (`game::ReadMenuFigure`, TabletMenu.cpp `kFigureReadFrames`) and both the
  figure's page and its tab place the picture by that rectangle; the page's
  edge (`kFigurePageEdge`) is a strip 0.006 of the page's width - about 10
  of the layer's 4036 pixels, near a millimetre on a 0.34 m tablet.
- [R code] Both layouts are made from that rectangle each frame.
  `vr::SetFigurePage`: the figure's place and the page behind it take the
  rectangle's width, and the tablet's face grows by it. `vr::SetFigureTab`:
  the figure is fitted into the page and centred by the rectangle's width
  and height.
- **Cause [D: from the three logged reads and that code]:** the figure
  breathes, so the reads differ - the log's three are 0.545, 0.544, 0.545 of
  the screen at the left edge, about 4 of the layer's pixels. With each
  read the figure's page got another width (its edge moved) and the figure
  behind its tab another place, six times a second at 90 Hz.
  tablet_figure_steady_test lays the logged reads out through both
  functions: as they came, the fitted rectangles differ; held steady, they
  are the same.
- **Fix:** `vr::StepFigureSteady` (vr/TabletFigureSteady.h, pure), in
  `ReadFigure` (game/TabletMenu.cpp). The rectangle taken is kept, wider
  than read by 1.5 % of its height on every side (the breath's room, cut at
  the screen's edges), while the reads stay inside it and no edge stands
  more than 6 % of the height inside. A figure that is turned or changes
  its gear outgrows or leaves it and is taken anew. Started over when the
  tablet's menus go. Both numbers are guesses [G] from the 4 pixels of the
  three reads; the turn's reads are not logged.
- Not ruled out [G]: (b) a read that fails now and then, the tablet showing
  the page alone for 15 frames - in beside mode the log would have said it,
  and did not; (c) the picture itself crawling: the figure's hard edges and
  the thin edge strip drawn smaller than the layer has them, with a linear
  filter and no mip level set by `render::TabletQuad`.
- **The instrument stays:** `vr/TabletFigureWatch.h` (pure,
  tablet_figure_watch_test) watches the rectangle as read, not the steady
  one. Logged once per 90 frames, only when a read moved, at most 40 times
  a session: "Tablet: the figure's rectangle as read over 90 frames - it
  moved in N frame(s), by at most P layer pixels (held steady on the
  tablet); not read in M frame(s)". Cost: four float compares a frame, no
  allocation.
- Next run: that line with N of about 6 and P of a few pixels confirms the
  cause. If the figure still flickers with it, P above 34 pixels says the
  room is too small; "not read" above 0 is (b); else (c) - then the harness
  film, `tablet-figure-watch.txt --record 14` through
  `tools/film-flicker.py`, still against turned.
- Not run: the game, the harness, the headset.

### 26.2 The touch came late

Nadi: "the touch on the tablet comes a few frames late, not direct as one
expects of a tablet".

- [R log] 43 taps, every one "pulled back behind the glass": 0.08 to 0.30
  s on the glass, mean 0.169 s, median 0.17 s; moved 0.2 to 1.4 cm along
  it (all but two under 1 cm); at the deepest up to 2.0 cm through.
- [R code] The touch begins with the tip 1.5 cm in front of the glass; a
  tap's click went out when the tip had come 2 cm back from its deepest
  point (`TabletContactLimits::pullBackTap`) or 4 cm out in front - so a
  finger that went 1 cm through had to travel 3 cm in and 2 cm out first.
  The button goes down in that frame and up the next.
- [R EXE] The game clicks on the release, never on the press:
  0x005821F0 (the interface's update) calls the menu's click (vtable +0x0C)
  and plays the click sound only when the mouse button's state is "up now,
  down before" (0x004031E0 with 2), and only on the tile it hovered; while
  the button is held the hover does not change. No pressed look was found
  in that path (the one use of "went down this frame", under the interface's
  +0xB9, was not followed) [R, partly]. So a button sent down at the touch
  could not be taken back: a drag or a swipe that began as a press would
  click on its release. The click has to wait for the touch's verdict; the
  hover under the finger already shows before the touch (the poke's hover).
- **Changed** (`vr/TabletTouch.h`): a touch that has kept still on the
  glass (`TabletTouchStill`: down, moved under 1 cm along it) lets go as
  soon as the finger turns back 6 mm (`pullBackStill`) - the click goes out
  in that frame. A touch that has moved 1 cm or more keeps the 2 cm way
  out, so a drag that drifts outward on its way to the scroll's 2 cm is no
  tap; a scroll, a page or a drag keeps its 5 cm. A second tap still needs
  the finger 2 cm back in. `vr::StepFingerOnTarget` passes the flag.
- [D] Before and after, from the log's taps with the speed taken as even
  over the way in and out [G]: the click came 0.169 s after the touch
  began on the mean (15 frames at 90 Hz); with 1.4 cm less of the way out
  it comes 0.108 s after (10 frames) - about 61 ms, 5 to 6 frames sooner.
  The engine's own frame for the release is unchanged. A finger that
  slows at its turn gains more [G].
- Tests: menu_place_test "turned back by 8 mm: the tap's click in that
  frame" FAIL before the change, ok after; "moved 1.5 cm along the glass,
  then 8 mm out: still touching" ok both; tablet_touch_test TestStillTurn
  (new). Harness: `tablet-touch-turn.txt` (the tap at the turn, the sword
  equipped; before, the touch ran on and ended off the face's edge with
  nothing clicked). Not run.
- Unverified, the headset's: whether 6 mm is above a hand's tremor at the
  glass (a stray tap), and whether a scroll's start drifts out 6 mm before
  it has moved 1 cm.

### 26.3 The world froze as the tablet closed

Nadi: "when I close the tablet the world freezes for a few frames".

- [R log] 43 closes, "closed - the menus' fade kept out of the room and the
  eyes (22 frame(s))" mostly (19, 21, 11 too): 0.24 s at 90 Hz. Each comes
  right after "Tablet: put down" and "Menu pause: behind menu F1-F4 stack
  entry (0x001) the world pauses as vanilla" (2433-2437, 12778-12781), and
  "Combat ring: the world paused behind a menu" (2427).
- [R EXE] Menu mode is the interface's state byte at +0x08 not being 1
  (IsMenuMode 0x00578F60). When the last menu closes, 0x0057CEE0 sets it to
  4 (leaving); 0x00583F40 sets it back to 1 only once no menu fades any
  more. During those frames the stack holds only the HUD's menus (24.2,
  measured 2026-11-26).
- [R code] The world ran behind the tablet only while the F1-F4 entry was
  in the stack (`vr::TabletMenusUp`); in the fade it is not, so
  `WorldPausesForMenu` gave vanilla's answer: paused. The player's controls
  stopped too (the walk under the tablet needs `frame.tablet.up`).
- **Changed**: `vr::StepTabletRunsWorld` (vr/TabletClose.h): the world that
  ran behind the tablet's menus runs on while menu mode stays on with only
  the HUD's menus left - the fade; any other menu in the stack, a message
  box or the Esc menu before the close, or a fade with no tablet before it,
  keep vanilla's answer. `game::TabletRunsWorld` (MenuPause.cpp) asks it at
  every redirected site; said once a close: "Menu pause: the tablet's menus
  fade out - the world runs on through the fade". The walk goes on through
  the fade too (`vr::TabletWalkFrame`, camera/hands/LaserTargets.cpp). The
  fade stays out of the room and the eyes as before (24.2). Ending the
  fade at once was the other way; it would mean writing the engine's state
  byte and doing 0x00583F40's tail ourselves (the cursor, the HUD's
  return) - more risk for the same result.
- [D] Before: 22 frames (0.24 s) of the world standing still after each
  close at 90 Hz; after: none.
- Tests: tablet_close_test TestWorldRunsThroughFade and
  TestWalkThroughFade (new functions, so red before as a build failure).
  Harness: `tablet-close-runs.txt` (the walk goes on through the close; the
  pause line rejected). Not run.
- Unverified: that nothing in the world minds running while the engine's
  state says "leaving" - the same subsystems run as while the tablet is
  open, but in state 4 rather than 2 [G]; the headset.

## 27. Windows run of 2026-11-27: defaults and the open gesture

Nadi's headset run of 2026-11-27 (OBVR.dll from main d96cacb; OBVR.log
13:38-13:55, two earlier logs of 12:20 and 12:33). Marks: [R] read in the
log, the code or the live INI; [D] derived from that; [G] guessed.

### 27.1 Nadi's tablet place as the default

Nadi: "the values for the inventory tablet I now have in the INI are the
values that should be the standard. As far as I know I could only go -10
to the back, it probably needs -15 to the back."

- [R] His live INI, [Tablet]: HeldTurnDegrees=90, HeldOutCm=-10,
  HeldAcrossCm=20, HeldUpCm=0, HeldLeanDegrees=25, HeldRollDegrees=-15.
- [R] Why -10 was the end: the settings row "Hand menu distance"
  (ui/SettingsPartHands.h) ran from -10 to 20. The INI reader
  (core/ConfigTablet.cpp) allows +-40 cm and +-180 degrees and was not the
  limit. "Hand menu sideways" ran from -20 to 20 and his 20 sat on its end.
- **Changed:** `vr::HeldPlace`'s defaults are Turn 90, Lean 25, Roll -15,
  Across 20, Up 0, Out -15 (his values, the distance 5 cm further back as he
  asked); the shipped OBVR.ini and its comments say the same. The rows: the
  distance -30 to 20, sideways -30 to 30. Height (-20 to 20), turn
  (+-180), lean and tilt (+-90) were not at their ends and are unchanged.
- A live INI that names a value keeps it: his `HeldOutCm=-10` stays until he
  sets it in the game or the file.
- Tests: tablet_test pins the new defaults; the geometric checks that take
  one angle at a time (tablet_test, tablet_on_hand_test, menu_place_test)
  now set the other values to 0 themselves; settings_list_test
  (tests/settings-rows.txt, the four rows edited by hand).
- Harness: the harness copies the live INI, which may or may not name these
  keys, so every scenario that shows the tablet in the hand (43 tablet-*
  files; not tablet-tiles, which only logs tile trees; not those with the
  tablet off) now pins the place it was aimed at: `HeldLeanDegrees=0`,
  `HeldRollDegrees=0`, `HeldOutCm=0` and, where not already set,
  `HeldAcrossCm=0`. Not run.

### 27.2 The open gesture refused every try

Nadi: "the gesture at the stow spot to open the inventory tablet did not
work at all."

- [R in the log, OBVR.log 5815-5868] Five tries, all refused: the hands
  0.24 m apart (0.04 to 0.20); a hand 0.06 m outside the inventory ring; the
  hands 0.24 m apart; 0.29 m apart; together with open grips for 0.00 s
  (0.35 wanted). The 12:20 and 12:33 logs have no try.
- [R in the log, the Holster lines at the same grips] The controllers were
  0.02 to 0.09 m apart in those tries: (-0.04 -0.01 -0.23) and
  (0.05 -0.03 -0.24) right, forward, up from the eyes for the first; 0.06,
  0.08, 0.02 and 0.05 m for the others. [D] Each Holster line is logged as
  its own grip closes, a few frames apart, so this is near, not exact.
- [R in the log, 970-971] The palms were read in the game (not the hand
  bone fallback): right 0.047 -0.004 0.147, left -0.041 0.012 0.134 m in
  each controller's frame - 13 to 15 cm behind the controller and 4 to 5 cm
  outward. [R in the INI] His grip puts the hand bone 17 to 19 cm behind it
  (`RightHandGripY` -0.188, `LeftHandGripY` -0.169).
- **Cause** [D]: section 16 moved the measure from the wrists to the palms,
  but the palms sit almost as far from the controllers. Their outward 9 cm
  and the 14 cm lever turned with the wrists keep two drawn palms 0.24 m
  and more apart whenever the controllers touch; the 0.20 m window could not
  be met. Section 16's unit test had placed the palm 3 cm off the
  controller, not where the game reads it, so it passed. The ring was
  tested per hand: two hands side by side span 20 cm and more, so one of
  them was out (try 2). The dwell asked for the pose held with open grips
  for 0.35 s; it reset when one grip closed, and the palms were never close
  enough while open (try 5). [R] With the old rules, unit frames built from
  the five tries' numbers are refused exactly as the run was: 0.24, 0.06
  out, 0.24, 0.29, 0.00 s.
- [R in the code] GripToggle=1 does not matter here: the toggle changes
  each hand's hold only while it keeps a weapon, a torch or a bow
  (vr/GripToggle.h); the scroll reads the raw grip.
- **Changed** (vr/TabletScroll.h, vr/MenuPlace.h `ScrollFrameOf`,
  camera/hands/ControlsOut.cpp's refusal line). The gates, all at the
  moment the second grip closes:

  | Gate | Before | Now |
  |---|---|---|
  | Hands together | the drawn palms 0.04 to 0.20 m apart | the controllers at most 0.20 m apart |
  | At the ring | each drawn palm inside the ring's reach | the palms' middle within the reach plus 0.10 m |
  | Intent | 0.35 s held with both grips open, then both closed | both grips closed within 0.5 s of each other |
  | Hands empty, in the world, no menu | as before | as before |

  How far it is unrolled is the pull from where it began: 0 at the start,
  open when the palms stand the tablet's width apart, or 12 cm further
  apart than at the start when that is more (before: from a fixed 0.08 m,
  so his start at 0.24 m would have shown it 60 % open at once). Brought
  back to within a tenth of the start after unrolling a quarter, it rolls
  back and closes (before: palms at 0.08 m, out of reach). Rolling up a
  held tablet ends at the start's distance, or 0.24 m if no scroll has
  begun yet (opened by the B) [D from his tries].
- The log line says which gate: "the controllers X m apart (at most 0.20)",
  "the hands' middle X m past the inventory ring (its reach and 0.10)",
  "the second grip X s after the first (at most 0.50)", once per try, 40
  lines a run.
- Still kept away [D from the tests]: a hand holding anything, the tablet
  off or OBVR's own menu up, another menu up, the controllers more than
  0.20 m apart (more than 0.40: no line, a two-handed grab), the hands
  together at the hip, above the eyes or half a metre further out, one grip
  held longer than 0.5 s before the other, grips closed elsewhere and
  carried to the ring closed, one grip alone.
- [G] The 0.20 m together window and the 0.10 m ring margin are chosen
  from five tries of one player with Index controllers; the 0.5 s pairing
  has no measure in the log (no times between the two grips). A tracked
  run with the frames logged would settle them.
- Tests: tablet_scroll_test (his five tries begin and open fully with a
  20 cm pull; every gate and refusal with its measure; the accidental cases
  above; unrolling, rolling back, rolling up, the B), menu_place_test (his
  read palms over 20 cm apart with the controllers 12 cm; the controllers
  handed over as they are). Red before: the five tries refused as in the
  run.
- Harness: tablet-scroll.txt's out-of-ring try moved from 25 to 40 cm out
  (25 cm is now within the margin) and expects the new line;
  tablet-scroll-vertical.txt unchanged in its moves. Not run.
- Not verified: the gesture in the headset. Whether two Index controllers
  closed side by side at the chest always read under 0.20 m (his five
  did), and whether 0.5 s is enough for a natural squeeze.

## 28. The view and the body step back as the inventory opens (Nadi, headset, 2026-11-27 evening)

Nadi, after his second run of the day (OBVR.log 2026-11-27 23:34, the DLL of 23:04, main
4f1124f plus the swimming and books branches): "at inventory open the view and body steps
back one step. closing moves forward." On 2026-11-28: "fix jetzt".

**State: a fix built on 2026-11-28, unit-tested, not run in the game.** The engine-side
mechanism is derived from reading, not measured; the fix does not depend on it, and it
carries the measurement that will say what the engine did.

### 28.1 What was read

- [R log] The run's log has no line for the camera's, the room's or the body's place
  around an open or a close. `Menu trace: ... camera pass=1` stands before and after each
  open: under the tablet's menus the camera pass runs on, every frame.
- [R code] What the camera pass builds the view from, and what of it asks for the menu:
  - the base is the engine's first-person eye - the world place of `Camera01` in the
    first-person skeleton (FUN_0066b710 reads `[0x00B3BB0C] + 0x88`), written to the
    camera node just before OBVR's hook;
  - `camera/StandingEyeHold.h` holds that eye over the feet against the first-person
    animation - and was **not live under a menu** (`!game::IsMenuMode()`): from the
    menu's first frame to its last the engine's eye passed as it was;
  - the room follow stands still under a menu (`RoomFollowAllowed`), which moves nothing
    by itself: the head's offset less what was followed stays what it was;
  - the head's offset, the look control's base rotation and the heights are built the
    same way under a menu.
  So the one place the view's base can change with a menu is the engine's eye itself.
- [R code] The body hangs from the camera (`game::ShowPlayerBody`, the cyclopean camera's
  world place), and the hands are pinned to it: a base that steps takes the body and the
  hands with it - "the view and the body".
- [D] Why the engine's eye would differ under the tablet: `Camera01` hangs under the
  first-person root, and OBVR places that root with the right hand every frame at the
  draw (`game::PlaceFirstPersonArms`: the hand's turn and its offset from the arms' rest
  pose, some tens of units). In a world frame the player's update puts the root back
  before the engine reads `Camera01`. Under the tablet's menus the player's update does
  not run while the player stands (the body's check reads a stale skeleton at each open
  in this run's log and words it so: "world transforms out of date (a frame without the
  player's update: a menu with the world running)" - a reading of the bones, not a trace
  of the update), so the camera update would read `Camera01` where OBVR's placement left it
  the frame before: the eye displaced by the hand's offset, from the menu's second frame
  on, and back with the first world frame. The same leak is on record twice: the mounted
  view (game/FirstPersonArmsLogic.h, 2026-10-19) and the NPCs' look at the player
  (game/PlayerLookAt.h). [R exe] The two halves run apart in the engine: the camera
  update 0x0066B710 is called from the render path (0x00411B70), the first-person root is
  placed by the actor's update (0x00603CA0, then 0x00660130 for its turn) -
  ../OBNG/knowledge/first-person-camera.md. **Not proved for this case** - no run has the engine's eye under
  a menu. If it holds, the view would also follow the right hand while the tablet is up.

### 28.2 The fix

- `camera/StandingEyeHold.h`: under a menu with the world running the view keeps the
  place it had over the feet as the menu came up (the offset in the player's frame, so
  the walk under the tablet carries it and a turn turns it). Nothing is eased toward the
  engine's eye there and no jump is taken. A menu moves nobody's head. For ten frames
  after the menu a far offset is eased like a near one, so one last frame of the menu's
  eye is no jump there and back. Sneaking, the height stays the engine's as in the world.
- Not changed: a conversation (the camera hook holds the dialogue's base itself), knocked
  down under a menu, riding, swimming - the engine's eye, as before; menus that pause the
  world never reach the camera pass.
- The decisions are pure: `StandingEyeLive`, `StandingEyeUnderMenu` (the gates),
  `StepStandingEyeHold` (the hold), covered by `standing_eye_hold_test` - 28 new checks,
  among them "as it was": the menu's first frame showing the engine's eye 40 units back.
- **The measurement** rides along: once a menu is gone the log says
  `Camera: under a menu with the world running the view was kept where it stood over the
  feet for N frame(s) - the engine's eye stood up to U units from it (x right, y forward,
  z up at the furthest; 2 units off from its frame F), the feet went W units meanwhile`.
  - U near 0: the engine's eye does not move under the menu - the step has another cause,
    this fix does nothing for it, and the next thing to read is the feet (W) and the
    head's offset.
  - U of some tens of units: the cause is the engine's eye; the vector says which way,
    and whether it follows the right hand (the [D] above) shows in U changing from menu
    to menu with where the hand was.
- Harness: `tools/hand-scripts/tablet-open-step.txt` (written, not run - Linux): four
  marks, world / open / the right hand moved / closed; the held eye and the eyes' place
  must be the same at all four.
- Cost of the change per frame: a handful of float operations more in the camera pass;
  no allocation, no log line outside a menu's end.

### 28.3 Open

- The headset: whether the step is gone at the open and the close.
- The next log's `Camera: under a menu ...` lines: the size and the direction of what the
  engine's eye did, and with them whether 28.1's [D] is the mechanism.
- If it is: the engine's eye is displaced by OBVR's own placement of the first-person
  root, and whatever else reads `Camera01` under a running-world menu reads it displaced
  too (the engine's own picks and sounds from the eye). Putting the root back after the
  draw on those frames would end it at the source; not built on a mechanism not measured.
- The Linux harness run of the scenario above.

## 29. The evening run of 2026-11-27 (log of 23:34): read, nothing built

The same run as section 28's note (the DLL of 23:04, main 4f1124f plus the swimming and
books branches). Decided that evening: note everything, build nothing for now.

### 29.1 The open gesture: 12 tries, none began

"Tablet: both grips closed with the hands together, the scroll not begun - ..." [R]:

| Log line | Refused for |
|---|---|
| 1937 | the controllers 0.34 m apart (at most 0.20) |
| 9982 | the controllers 0.33 m apart |
| 10041 | a hand holds something (a weapon out, a torch, a thing held, a two-hand grip) |
| 10934 | the controllers 0.40 m apart |
| 11074 | the controllers 0.34 m apart |
| 16813, 16821, 16827 | a hand holds something |
| 19958 | the controllers 0.24 m apart |
| 28081, 28116, 28139 | the hands' middle 0.14, 0.14, 0.13 m past the inventory ring (its reach and 0.10) |

No "unrolling" line in the run. So section 27's gates, made from the midday run's five
tries (the controllers 2 to 9 cm apart there), do not fit what Nadi did in the evening:
his controllers were 24 to 40 cm apart when the grips closed.

- **Not known, to read first:** what he did in each try - the Holster lines at the same
  grip presses give each hand's place from the head; which grip closed first and how far
  apart in time; where the inventory ring stood against his hands' middle; and what the
  hand held in the four "holds something" tries. His `[Hands] GripToggle=1` and the
  toggle's new hold for pulls (game/PullHold.h, built the same day) may make an empty
  hand read as holding [G: not checked].
- **Not to do:** widen the numbers until it fires. If hands together at the ring, pulled
  apart, is not the gesture he makes, the gesture is to be changed to the one he makes,
  and the accident cases (a hand at the hip, the potion belt, a grab) tested against it.
- **Cost:** small once the 12 tries are read. Model and effort: Opus 5.5 medium.

### 29.2 The figure: the reads move, the tablet holds them

"Tablet: the figure's rectangle as read over 90 frames - it moved in N frame(s), by at
most P layer pixels (held steady on the tablet)" - 40 lines (the cap) [R]: 34 of them with
P of 1 or 2 pixels in 1 to 6 frames of 90, four with 51 to 83 pixels (the figure turned or
its gear changed); never "not read". That is 26.1's cause seen: the breathing figure's
rectangle moves by a pixel or two at the reads, up to six times in 90 frames. Whether the
flicker is gone for the eye is Nadi's to say - not said yet.

### 29.3 The close, the touch

- "Menu pause: the tablet's menus fade out - the world runs on through the fade": 33
  times; each close's fade still 22 frames, the world no longer held through it [R].
  Nadi's new report for the open and the close is section 28 (the view steps back and
  forward) - whether the two are related is not looked into.
- 23 taps, on the glass 0.157 s on average (0.169 s at midday). The time on the glass is
  not the click's delay: the click goes out at the finger's turn since 26.2. No word from
  Nadi on the feel yet.

### 29.4 Books and letters, swimming

- "Tablet: a letter (the book menu's scroll look) read alone, held in the hand": 3 times
  [R] - a letter or note was read on the tablet. No line for a book with pages.
- "Swim: the player swims - the arms' strokes are on", 5 x "a hand holds the water" and
  5 x "the water let go", "out of the water" (docs/swimming-spec.md) [R]: the strokes ran.
  How far each carried him and whether diving by a stroke works is not read from the log
  yet.
- Both branches (books-1127-win, swim-1127-win) are in the test DLL only, not on main.

## 30. After the gesture the tablet is in the off hand (Nadi, headset, 2026-11-28)

Nadi, after the run of 2026-11-28 08:18-08:27: "inv geste geht nun. das inv geht aber auf
die rechte hand nach der geste. wenn rechts meine main hand ist sollte sie auf die linke
gehen denn rechts brauche ich zum tippen/zeigen usw." Marks: [R] read in the log or the
code; [D] derived; [G] guessed.

### 30.1 What the log says

- [R, OBVR.log of 08:18] The gesture opened the menus 9 times ("Tablet: the scroll sends
  the menu key (the menus open)", 9330 to 15683). 5 of them were let go fully open, and
  each said "Tablet: no longer between the hands" and then "Tablet: kept by the right hand"
  (9612, 14926, 15383, 15575, 15840): the left let go, the right kept its grip. The other 4
  rolled back and closed. Opened by the left B the tablet is in the off hand ("Tablet: in
  the off hand, drawn into the eyes", 1360 and on, with no "kept by" line).
- [R] Why the gesture began now and not the evening before (the gates of section 27 are
  unchanged): the evening's tries had the controllers 0.24 to 0.40 m apart (section 29.1),
  the last three the hands' middle 0.13 to 0.14 m past the ring. This morning the Holster
  lines at the same grips put the two controllers 4 to 12 cm apart, 25 to 29 cm under the
  eyes and just in front of them (9327/9329: 0.01 -0.05 -0.28 and -0.03 -0.06 -0.26;
  9710/9712: -0.03 -0.02 -0.29 and 0.04 -0.04 -0.29; 9918/9920: -0.06 -0.02 -0.25 and
  0.06 -0.02 -0.26, right, forward, up from the head). Every refusal of this run is "a
  hand holds something" (7 times) or "the second grip 7.90 s after the first" (once); none
  for the distance or the ring. [D] He held the hands closer together. The evening log
  has no Holster line in the 14 lines before its tries, so where his hands were then is
  known only by the refusals' measures.

### 30.2 The rule

- **Before** (vr/TabletScroll.h `StepUnrolling`): fully open, the hand that kept its grip
  held it (`holderRight = f.grip[1]`), and the upper of the two when unrolled up and down.
  The spec's stage 3 (section 8) had it so: "letting go with one hand leaves it in the
  other". A right-hander who lets go with the left had it in the right hand, pointed at by
  the left.
- **Now** (vr/TabletHolder.h `TabletHolderOnceOpen`, called by `StepUnrolling` and for the
  B by `StepIdle`): the off hand holds it, whichever hand let go - the left controller for
  a right-hander, the right one for a left-hander. The weapon hand points, taps and
  scrolls. Unrolled up and down, the off hand holds the top edge when it was the upper
  hand, else the bottom. The B is unchanged: the off hand, its bottom edge.
- There was no hand-over of the tablet between the hands to keep: held, the free hand's
  grip at the free end rolls it up, and that is the free hand's only use of it.
- The roles follow the weapon's side (vr::AssignHandRoles), so after a weapon or shield
  handed to the other hand the off hand is the hand without the weapon, for the B as for
  the gesture.
- The log line of the open names the controller: "Tablet: in the off hand, drawn into the
  eyes - on the left controller"; the held place's line names the controller by its side,
  not its role.
- `MenuPlace`'s right-hand hold (`HeldByRight`) is reached again only through 30.3's last
  point: the sides switched between the pair's grips.

### 30.3 The shield's handover waits for the gesture (built 2026-11-28)

- [R, OBVR.log of 08:18, 9326, 9328, 9711, 9897, 9919] Closing both grips with the hands
  together at the chest while a shield is worn started the shield handover: "Handover: the
  weapon hand closed 0.0 units from the worn shield - the sides switch, the shield to that
  hand". After some of these tries the Holster lines read "left-handed 1" (9329, 9812,
  9898): the sides had switched, and the off hand was the right controller. So with a
  shield worn 30.2 alone did not fix the report.
- Decided 2026-11-28 (the coordinator): where both claim the grips, the gesture has the
  first claim. **Built** (vr/GripClaim.h `StepGripClaim`, grip_claim_test;
  camera/hands/Handover.cpp `StepShieldHandover`):
  - The gesture's zone (vr/TabletScroll.h, `TabletScrollResult::zone`): the scroll idle,
    the tablet on, no menu, both hands empty, the controllers within 0.20 m and their
    palms' middle at the inventory ring - its own gates. In it the shield's closing waits
    up to the gesture's 0.5 s for the other grip.
  - The gesture begins in that time: no handover, and none from those grips until both
    are open again. The log: "... from the worn shield - the tablet's gesture took the
    grips, no handover".
  - The window passes without it (one grip only, or the gesture refused the second): the
    handover 0.5 s late ("... (after the tablet's wait, no other grip)"); a shield taken off
    meanwhile, or a menu come up, drops it.
  - Outside the zone, the tablet off, a hand holding something: at once, as before.
- The other handovers (a weapon, a two-hander, a torch, a thing held) all need something
  in a hand, which shuts the zone, so they are not gated [D from `handsEmpty`]. A drawn bow
  is not counted in `handsEmpty` (it reads the melee weapon, the torch, a held thing, a
  two-hand grip); whether the bow's handover can meet the zone is not read.
- The seven "a hand holds something" refusals of the morning (10241, 10653, 10661, 10671,
  10682, 10740, 10805) [R]: the Iron War Axe drawn from the right shoulder (10185 to 10253;
  10600 on, in the two-hand hold until the handover of 10681; still drawn at 10740), and at
  10805 a thing grabbed by the left grip (870FC9F0, 10788). None came from the shield's
  handover; the gate leaves them as they are - the gesture is meant to refuse a full hand.
- The roles before the grips: the scroll keeps the sides as the pair's first grip closed
  from both open (`TabletScrollState::rolesAtGrip`). If they switched before it is held - a
  handover that slipped through, from outside the zone - the hand that was the off hand
  keeps it: `TabletHolderOnceOpen`'s `sidesSwitched`, the frame's right role then.
- [G] The flip of a handover is taken at the next frame's roles; a role swap in the middle
  of the pair also swaps the frame's grips, so the scroll may read the swapped hand's grip
  as a new closing. Not met in tests beyond the cases above.

### 30.4 Tests and what is untested

- tablet_holder_test (new): the B and the scroll, right- and left-handed, the edge held, the
  sides switched mid-gesture (3 checks red before the change).
- grip_claim_test (new): outside the zone at once; the gesture taking the grips with the
  weapon hand first or second; no late handover after it; both grips open ends the pair;
  one grip only - late by the window; a second closing while waiting; the second grip
  refused by the gesture; a menu up while waiting (10 checks red against a stub of
  today's behaviour). tablet_scroll_test: the zone and its gates (the tablet off, a hand
  holding, apart, at the hip, a menu, a hand lost), the begin, the roles before the grips
  (4 red). menu_place_test: the zone and the begin through the hand mode.
- tablet_scroll_test: either hand letting go leaves it in the off hand; the weapon hand
  rolls it up at the free end. menu_place_test: through the hand mode, right-handed (the
  sideways and the upright scroll) and left-handed (the roles swapped, each hand letting
  go); the weapon hand points. Red before the change: 7 checks in tablet_scroll_test, 5 in
  menu_place_test.
- Harness, written, not run (Linux): tools/hand-scripts/tablet-scroll-offhand.txt
  (right-handed) and tablet-scroll-offhand-left.txt (left-handed); tablet-scroll.txt and
  tablet-scroll-vertical.txt rewritten for the new holder - their roll-up by the right
  hand at the free end is mirrored from the old case [G: the free end's place not
  measured]. The two offhand scenarios wear the Iron Shield: the weapon hand closing
  first, then both at once (the gesture takes the grips, no switch), and right-handed a
  last weapon-hand closing alone at the ring (the handover late).
- Not verified: the shield's gate in the game - whether the hold's closing and the
  scroll's raw grip land in the same frame, as the gate assumes [G].
- Not verified: the feel in the headset - the tablet now jumps to the off hand when the
  off hand lets go first.
