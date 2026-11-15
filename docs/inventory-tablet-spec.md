# The inventory as a tablet in the hand (feasibility and plan, 2026-11-15)

The ask (Nadi, 2026-11-15): the inventory menu does not pop up in the world
ahead of the player; the player holds it in one hand like a tablet, the world
keeps running, and the other hand's finger taps, scrolls and swipes on it as on
a tablet. On by default, with a switch. It opens with the left B, or by a
gesture: both grips held and pulled apart like a scroll being unrolled; fully
open it can be used, short of that it rolls back.

Nothing of this is built. This file is the study, the build guide and the task
list. It replaces the plan part of `docs/immersive-scroll-menu.md` (2026-09-21,
written against a code base that has since changed); that file's gesture rules
and its two pure modules stay valid and are reused here.

## 1. Verdict

**Feasible, medium in total, in four stages that each stand on their own.**
Most of the base is in the repo. Three points deviate from "exactly like a
tablet" and are named in section 3 before anything is built.

| Stage | What the player gets | Cost | Why |
| --- | --- | --- | --- |
| 1 | Left B opens the Tab menus on a tablet in the hand, cropped to the menu, world running, tap with the finger, laser as before | small to medium | wrist placement, poke, pause policy exist; the crop to the menu's rectangle and the hand grip pose are new |
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

1. **The tapping finger is hidden behind the tablet.** Every panel of OBVR is a
   SteamVR overlay, and the compositor draws overlays over the scene; the
   OpenVR headers describe no depth test between an overlay and the submitted
   eye pictures (deepwiki ValveSoftware/openvr, `headers/openvr.h`; that
   answer is from the API's declarations, not from the compositor's code). The
   hand is in the scene, so it disappears where the tablet covers it. Options:
   - **A (recommended): a fingertip mark on top** - a small overlay dot with a
     higher sort order at the tip, growing as it nears the glass, plus the
     engine's own row highlight and the menu buzz. Cheap: the laser's dot is
     this already (`render/LaserLayer.cpp:164`). Gains: no engine work. Costs:
     a dot, not a finger.
   - B: draw the tablet inside the scene as a textured quad, so depth is
     right. Large: OBVR draws no geometry of its own into the eye passes
     today, and the 2D layer is captured after the scene, so the picture would
     be a frame late. Not verified beyond that.
   - C: leave it. Free, and probably what makes taps feel blind.
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
shut when a fight starts (**decided 2026-11-15, Nadi: yes, with a switch**,
`[Tablet] ClosesInCombat`). **It must open in a fight all the same** (Nadi,
the same day: potions are fetched from it): the switch closes it once, on the
frame the fight starts, and never refuses an opening; opened during the fight
it stays open until the player closes it. The left B is the way in with a
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
  opens (never measured for these menus; `docs/container-touch-spec.md`
  section 3 has the same open measurement).
- **The rolls.** Stage 3 first cut: the picture cropped to the opening, a
  parchment strip at each edge. Real rolled ends would use
  `SetOverlayCurvature` (in the header: "1 is a fully closed cylinder"); OBVR's
  backend does not call it yet and how it looks is unverified.

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

`[Tablet] Enabled=1`, `WidthMetres`, `Gesture=1`, `ClosesInCombat=1`; rows
under "Laser & wrist menus" and one on the onboarding's comfort page. `[Hands]
WristMenu` stays as it is and loses to the tablet for the tablet's menus.
Full VR only.

## 5. Risks, in the order they can sink it

| Risk | Decided by | If it fails |
| --- | --- | --- |
| Text too small at tablet size | headset, stage 1 | bigger tablet, or the tablet becomes opt-in |
| Rows not hittable: the tip is the controller's, about 1 cm off | headset, stage 1 | bigger rows through size, the fingertip mark, the laser |
| The gesture fires in fights or blocks holster grabs, the two-hander, the ragdoll grip | headset, stage 3 | gesture off by default, left B alone |
| A click taken as an attack while walking | harness, stage 4 | no walking with the tablet |
| The Tab menu's opening hitch with the world running | harness measurement, stage 1 | named, not hidden; no promise of "no stutter" before it |

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
6. The fingertip mark (the laser dot's overlay at the tip, sized by distance).
7. Harness `tablet-open.txt`: left B, the rectangle, the placement on the
   hand, the world's clock moving, a tap on a row by script, close. Measure
   the opening frame's time.
8. Headset: readable? rows hittable? the grip pose?

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

Not in this plan: a pixel-smooth list, the tablet drawn inside the scene,
containers and barter on the tablet.
