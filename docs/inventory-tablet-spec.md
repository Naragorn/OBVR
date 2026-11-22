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
