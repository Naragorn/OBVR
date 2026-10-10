# The laser on the main menu's buttons: why it is not robust, and what would make it so (analysis 2026-10-10, nothing built)

The tester: "wie kann man das hauptmenü noch verbessern damit die buttons
gut mit dem laserpointer funktionieren. bisher wirkt das nicht sehr
robust. wie ist das bei anderen menüs? nur analyse".

Everything below is read in the repo (file and function named) or in the
docs' own measurements; what is a guess is marked as one.

## 1. How the laser works a game menu today

One pipeline for every engine menu, with two front ends:

1. **The ray.** The controller's pose with the laser's own offsets
   (`[Hands] LaserPitchDegrees` 40, `LaserYawDegrees` 5, `LaserOriginMetres`,
   `LaserRightMetres`, `LaserUpMetres`; `vr::HandLaserWorldRay`).
2. **The hit.**
   - An in-game menu hangs on a quad in the room or on the wrist: the ray
     meets the quad (`vr::LaserOnQuad`, vr/HandInput.h) and the hit is a
     pixel of the 2D layer the quad shows, one to one.
   - The main menu, a loading screen, a film and a menu on the cinema
     screen are the **flat picture**: both eyes get the same pixels
     (`EyeMirror::FlatPlacement`, "one size in both eyes"; HeadsetRenderer:
     "The same image to both eyes. This is mono"). The laser has no quad
     there, so it takes a stand-in plane 2 m ahead of the picture's anchor
     (`kFlatLaserPlaneMetres` in vr/HandMode.cpp), finds where the ray meets
     it, and maps that point *as the head sees it* into the frame window the
     picture shows (`vr::LaserOnFlatPicture`; `FlatPicture.pixelLeft/Top/
     Width/Height` from the mirror's flat source crop).
3. **The cursor.** The engine's menu cursor is read every frame from the
   InterfaceManager (+0x2C/+0x34, the two floats its tile search
   0x00581390 takes; `game::InterfaceCursorPosition`). The laser never
   writes it: it sends a *relative mouse step* towards the wanted pixel -
   `CursorStep(current, wanted, LaserGain 0.5, LaserMaxStep 60)` - through
   `mouse_event(MOUSEEVENTF_MOVE)` while the game's window is in front, or
   into the game's DirectInput state at its poll (`game/EngineInput.h`)
   while it is behind another window (`game/InputRoute.h`). The engine's
   cursor-movement function 0x57E7C0 integrates that into the floats; the
   next frame OBVR reads them again - a closed loop that converges whatever
   the engine's mouse scale is.
4. **Hover and click.** The engine's own tile search at the cursor
   (0x00581390, detoured so it runs under the believed viewport) decides the
   highlight and `activeTile`; a click is the left mouse button through the
   attack key (`SetKey(VK_LBUTTON)`), timed by `vr::StepLaserPress`
   (vr/HandInput.h): the trigger pulled on a hit *arms* the press; the
   click comes on the **release**, as the button down for one frame and up
   the next; a beam that moves 2 % of the layer height while the trigger is
   held becomes a scroll (vertical) or a held button (sideways) and then
   **no click comes at all**.
5. **What is shown.** The engine's cursor sprite is hidden
   (`SetMenuCursorHidden`) and the beam's end, the dot, stands in for it -
   drawn at the hit on the stand-in plane (`laserLengthMetres`, 2 m on a
   flat frame).

OBVR's own panels (the settings menu, the walkthrough, the quick menu) do
none of this: they hit-test their rows themselves (`PointAtPanel`) and the
sticks walk the rows (`StepStickNav`) - which is why they feel solid.

## 2. Why the main menu is the weak one

Same loop, worse conditions, in this order of weight:

- **The window is often not in front.** The main menu is where the game
  has just been started - from SteamVR, with its dashboard or the VR view in
  front. The Windows route then feeds the wrong window and the cursor stays
  put: controls-spec 4.16 (2026-10-07, "kann im hauptmenü nicht mehr buttons
  anvisieren oder klicken", the cursor at 0,0 through every step) and 4.17
  ("geht wieder. liegt vll daran das das game nicht im focus war"). In the
  world the window has usually been in front for a while. The engine route
  (`BackgroundInput=1`) covers "behind another window" only when OBVR sees
  that (`GameWindowInFront`), and a focus-less-but-foreground state is not
  told apart; the log's "Menu cursor: the engine's cursor stayed at ..."
  line detects the symptom but changes nothing.
- **The picture and the dot lie at different depths.** The flat picture is
  mono - the same pixels in both eyes - so the eyes fuse it far away (zero
  disparity); the dot is drawn at the 2 m plane. The eyes cannot have both
  sharp at once, and a beam end that doubles or swims makes the aim feel
  loose (the crosshair's lesson: "Doppelbild ist falsche Tiefe"). On a
  quad the dot sits on the quad.
- **The mapping moves under the hand.** The flat picture hangs where the
  head was when it appeared and is taken along by `vr/FlatFollow.h` after
  30 degrees or half a metre for a second - while the laser is on it. The
  head-seen mapping also means the cursor shifts when the head moves
  without the hand moving.
- **Narrow buttons meet the drag threshold.** The main menu is a column of
  text rows; `kLaserDragStart` is 2 % of the layer height (about 45 px at
  the believed 2266 px). A hand that wobbles that much between pull and
  release is a "scroll", and the Scrolling phase never clicks. In a list
  that is wanted; in the main menu it eats clicks.
- **The cursor trails the beam, then jitters.** 60 px a frame at most, half
  the remaining distance: crossing the 4028-px frame takes about seventy
  frames; near the target the integer step and the engine's own mouse
  scale (not measured) leave the cursor a step off. The click lands where
  the *cursor* is, not where the beam is - the "I pointed at Load and
  Continue took it" class. From the main menu's first frame the cursor sits
  at 0,0 and crawls out of the corner.
- **A one-frame click can be missed** (not measured, a plausible guess):
  through Windows the down and the up are two queued events and DirectInput
  is read in immediate mode once a frame; a pair that straddles one poll is
  never seen. The tap holds for keys (`kTapHoldSeconds` 0.12 s) exist for
  exactly this ("the ready weapon tap did not always take").
- **No fallback.** docs/native-menus.md describes sticks navigating entries
  through the engine's XML navigation handler (0x580BA0) from an earlier
  bridge; nothing in `src/` references it today (grep), so there is no
  keyboard-style way through the main menu when the laser fails.

## 3. What would make it robust (ranked)

1. **Write the cursor, do not steer it.** The two floats the tile search
   reads are plain fields; OBVR already detours the search's entry
   (`kFindTileAtCursor`) to run it under the believed viewport. Setting
   +0x2C/+0x34 to the laser's pixel right there, every frame a laser hits a
   menu, puts hover and click exactly where the beam is, with no gain, no
   cap, no mouse route and no focus dependence. The engine still integrates
   real mouse deltas into its own position (0x57E9C4/0x57EA03) each frame;
   writing *after* its update and *before* the search is what the detour's
   place gives. To check first: that the fields are not derived again after
   the search, and that the sprite (hidden anyway) does not matter.
2. **Click like a button, not like a touch screen, on menus without
   lists.** On the main menu (and message boxes, the dialogue's choices):
   the click on the trigger's *pull*, the button held for 2-3 frames
   (0.12 s like the tap holds), no drag conversion - the touch-screen press
   only where a list or a scroll bar is under the beam (`CursorOverScrollBar`
   and the menu id already tell).
3. **Put the dot where the picture is.** Either give the flat picture a
   real depth (shift each eye's flat destination by the pixels a 2 m depth
   wants, the crosshair's own cure) or draw the dot at the depth the mono
   picture is seen at. Small in EyeMirror, judged in the headset.
4. **Click the tile, not the pixel, on the main menu.** xOBSE's
   ClickMenuButton shows the engine's way: `menu->HandleClick(id, tile)`
   (Commands_Menu.cpp). The main menu's six buttons have tiles with render
   nodes whose bounds OBVR can read (tile +0x24, translation +0x54, the
   names); hit-test the beam against them, highlight by writing the cursor
   to the tile's centre (1), and click with HandleClick on the trigger. No
   loop at all; the engine's own handler does the rest. Medium work; the
   biggest win per button for the first menu anyone sees.
5. **Snap and hysteresis.** Pull the wanted pixel to the centre of the
   button under the beam within a margin, and leave a button only once the
   beam is clearly out of it - the haptics stop chattering and the click
   lands on what is lit.
6. **One cursor route.** Cursor motion through the engine's own poll
   detour always (it is OBVR's, independent of focus), Windows only for
   text fields; or at least the "cursor stayed put through 90 frames"
   detector switching routes instead of just logging.
7. **The flat anchor.** No re-anchoring while the beam is on the picture;
   the plane the laser uses at the distance the picture is shown at.
8. **A main-menu hand script.** The runner's scripts start after the load;
   a scenario on the main menu itself (point, hover, click Continue) is
   what would hold these fixes - the runner would need a mode that does not
   load the save first.

Order proposed: 1, 2, 3 first (small, they fix the loop, the click and the
depth for every menu); 4 and 5 for the main menu; 6-8 as the harness.

## 4. Not verified

- The engine's scale from mouse deltas to cursor pixels, and whether a
  one-frame down/up pair is ever missed: both want a measurement (a hand
  script cannot reach the main menu yet; in-game menus can be scripted).
- Whether +0x2C/+0x34 can be written without the engine deriving them back
  from +0x20.. in the same frame (GameAddresses.h: 0x57E7C0 "writes the
  cursor position as floats at +0x20/+0x24/+0x28 and a derived triple at
  +0x2C/+0x30/+0x34") - the detour's position answers it in one run.
