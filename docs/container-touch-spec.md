# Containers by hand: the menu in the world, opened by reaching (feasibility 2026-10-09; A, B and D built the same day)

The tester (2026-10-09): "außerdem mach eine machbarkeits analyses zu den
neuen container menüs: momentan verkleinerst du die ja und hängst sie ran
ans objekt. als nächstes will ich die direkt in die welt bringen ohne
stottern von menüs und hangs oder so. wenn ich nah genug rangehe mit der
hand blendet sich als overlay (steamvr vll) das menü ein direkt in real
time in der engine, keine pause. meine hand bekommt einen laserpointer und
strahl und ich kann wie gewohnt mit den menüs interagieren oder mit den
fingern touch machen wie auf einem touchscreen. items werden wie erwartet
aufgenommen oder reingelegt. dann hand weg -> menü ist weg. ich musste nie
A drücken. es kam nie eine pause oder ein background blur. selbes beim
schleichen wenn ich nah an NPCs sneake."

Verdict up front: **feasible on the engine's own ContainerMenu**, most of it
on pieces OBVR already has; the stutter claim needs a measurement before
anything is promised, and the pickpocket half carries one real risk (a
crime raised while the menu is up, with the world running). A container UI
of OBVR's own, with no engine menu at all, is possible but a different size
of job (section 5).

**Built 2026-10-09** (controls-spec 4.31, `[Hands] ReachOpens=1`): A (opened by
reaching, no dwell - the tester: "aber auch nicht kurz gehalten, ich muss
mich nur mit der hand nähern"), B (closed when the hand leaves, by the
engine's close-all-menus), the lock's minigame over the chest with the
world running and the chest's menu after it (the tester: "wenn das
bestanden ist das neue menü"), and D's trigger (a pocket while sneaking -
"wir schauen mal. könnte lustig sein"; the alarm-while-open risk below is
untested). The hand script reach-open passes for a chest; the lock, a
body and a pocket wait for the headset. C, E, F and section 3's
measurement are open.

## 1. What is built already (read in the repo)

| The ask | State | Where |
| --- | --- | --- |
| Menu as a panel on the object, facing the head | built 2026-10-08, headset-confirmed | controls-spec 4.28, `vr::ContainerAnchor`, `[Look] ContainerInWorld` |
| No pause behind the container's menu | built: the update step's 7 `IsMenuMode` sites answer "run" for the ContainerMenu | `game/MenuPausePolicy.h` `WorldPausesForMenu(..., containerRuns)` |
| No background blur / shade | built: no menu shade while the container's panel is up | controls-spec 4.29 (5) |
| A SteamVR overlay | already: the whole 2D layer is the compositor overlay `obvr.hud`, drawn by the engine each frame (route B) | docs/vr-modding/ui-and-hud.md |
| Laser on the panel | built: `LaserOnQuad` walks the game's cursor to the hit and clicks | docs/hand-tracked-mode.md |
| Finger touch on the panel | built for menu quads: `PokeOnQuad` / `StepPoke` (wrist menu and the room quad); not tried on the container's panel in the headset | `vr/HandInput.h` |
| Open without A | not built; the engine call exists: `TESObjectREFR::Activate` at `0x004DD260` (`kRefActivate`) | `game/GameAddresses.h` |
| Close when the hand leaves | not built; xOBSE's `CloseAllMenus` is `0x00579770` (xOBSE `Commands_Menu.cpp`), not yet in OBVR | - |
| Pickpocket by reaching while sneaking | not built | - |

So of the tester's list, the panel, the running world, the missing shade,
the overlay and the laser exist; the trigger, the close, the touch on this
panel and the pickpocket are the work.

## 2. The pieces, one by one

### A. Open by reaching (feasible, small)

- **Trigger**: an open, empty hand (no weapon, nothing grabbed, not a fist)
  within a reach of the container's surface for a dwell (start: 12 cm,
  0.25 s), the container under the hand found the way the hand's reach
  already finds things (the hand-range pick behind the pick hold, d63d6e4), then
  `TESObjectREFR::Activate(player)` on it directly - the same call the A
  button ends in, so the ref's OnActivate script, ownership and the
  container's own rules run as with A.
- **Not for**: a locked container (Activate would open the lockpick
  minigame, which pauses - the trigger skips it and the A button stays the
  way in), a container in combat range of an enemy unless a setting says
  so, NPCs that are alive and the player not sneaking (Activate on them
  starts a conversation - see D).
- **Why a dwell and an open hand**: reaching past a barrel in a fight, or
  holding a sword near a chest, must not open anything.
- Pure: the decision (hand state, distance, dwell, locked, combat, alive,
  sneaking) is a FrameLogic function with every flow tested, as the slap's
  and the stow's are.

### B. Close when the hand leaves (feasible, small; one unknown)

- With hysteresis: the hand further than ~30 cm from the container's
  surface for ~0.3 s, or the player walking or turning away (the stick),
  closes the menu - and only a ContainerMenu that OBVR opened by reaching
  (the A-opened one keeps vanilla's close).
- **How to close**: `CloseAllMenus` (0x00579770, xOBSE's command calls it
  this way) or the menu's own exit (the key it closes on, or a click on
  its exit button through `ClickMenuButton`'s path). Unknown: whether
  `CloseAllMenus` mid-transfer (the quantity popup up) leaves the transfer
  half done; the quantity popup is a second menu (`kMenuIdQuantity`) and
  the close waits while it is up.

### C. The panel: tighter and in place (feasible, medium)

- Today the panel is the whole 2D layer shrunk (`ContainerPanelScale`
  0.6) and hung over the chest's centre. The ContainerMenu's root tile
  knows its own rectangle (tile x, y, width, height); cropping the
  overlay to it (`SetOverlayTextureBounds`, already used for the 2D
  crop) gives a panel the size of the list only, which can then be put ON
  the object at real scale (a chest's lid, a body's chest) instead of a
  board above it.
- Touch: `PokeOnQuad` on the room quad exists; on a small panel the
  rows get small - the poke's press depth and the row height decide
  whether finger touch works there. Headset test first, with the laser as
  the fallback that already works.

### D. Pickpocket by reaching while sneaking (feasible, medium; one real risk)

- Same trigger as A, but only on a living NPC, the player sneaking, the
  NPC not in combat, the hand near the NPC's body (torso/hips, the
  pockets) for the dwell; then Activate - while sneaking the engine opens
  the NPC's inventory for picking instead of talking. That this goes
  through the ContainerMenu: xOBSE's `ContainerMenu` has the reference
  (`refr` at +0x44) and the event list has `OnAlarm_Pickpocket`, but the
  pickpocket mode's flag is not named in xOBSE's layout - **not verified
  in the binary**.
- **The risk**: with the world running, the NPC keeps its AI - walks off,
  turns, may detect the player. B's close covers walking off (the hand is
  no longer near). A detection, a failed pick or a crime raised while the
  menu is up may open a guard's arrest dialogue on top of the
  ContainerMenu; vanilla never has to handle that because its pickpocket
  menu pauses. That could stack menus badly or hang. Mitigation to try:
  close the menu at the first alarm event (the alarm hook xOBSE uses for
  `OnAlarm_Pickpocket`), or keep the pause for pickpocket only (the
  policy already decides per menu; pickpocket would need its mode read).
  This is the test that decides D.
- PiiiP: its own pickpocket ("Grabby", the grab control held 12 frames or
  more on an NPC, controls-spec 4.12) would sit beside this; with PiiiP
  present one of the two should be off.

### E. Putting items in (feasible via the menu; the physical drop is extra)

- Through the menu as now: the inventory side of the ContainerMenu and a
  click (laser or touch). Works today.
- Physically - a grabbed item released over the open container lands in
  it - is the stow (`[Hands] Stow`, #57) aimed at the container instead
  of the player: needs the engine's add-to-container with the item's
  ownership kept (a stolen item stays stolen). Not looked up; a second
  step.

### F. "No pause" for the player too (partly; a design question)

- The world runs behind the container's menu, but the player's own
  controls stay paused the vanilla way (the untouched `IsMenuMode` call
  that gates input, MenuPause.h) - no walking while it is up. With B
  (walking away closes the menu) the player never has to walk with it
  open, so this need not change. Redirecting that gate too would send
  the clicks on the menu into the world as well (attack, activate) - not
  proposed.

## 3. The stutter and the hangs (unknown until measured)

- No container was opened in the 2026-10-09 log, and no stall at a menu
  opening is recorded in the docs since the DXVK defragmenter was switched
  off (2026-10-06, the ~100 ms "Created shared texture" stalls - those hit
  every overlay, menus included).
- Candidates, all guesses until measured: the engine building the menu's
  tiles from its XML and the item list (vanilla cost, larger for long
  inventories and with DarNified UI / MenuQue), the first frame of a new
  menu in OBVR's delivery (layer captured, panel placed), the hot reload
  (fixed by ReloadGate, headset confirmation open).
- **Measurement first**: the frame-time profiler (docs/performance-
  profiling.md) over ten container opens and closes, A-opened, with the
  frames of the open and the close marked in OBVR.log. If the engine's
  menu build is the cost, it can be hidden (open on approach, before the
  hand arrives - the panel shown when the hand is there) but not removed
  on route A; route 5 removes it.
- Hangs: none known at a container's menu. The known hang classes are the
  GPU device losses (fixed 6ee61af) and the POV switch (open); the
  pickpocket stacking in D is the new candidate.

## 4. Proposed order (route A, the engine's menu)

1. Measure the open/close cost (section 3). Half a day.
2. A + B for containers and dead bodies, behind a setting (default to be
   decided), unlocked only, with the decision as a tested pure function
   and a harness scenario (reach to a chest, panel up, hand away, panel
   gone). One to two days.
3. C: the panel cropped to the menu's rectangle, laid on the object;
   finger touch tried on it. One to two days, headset-led.
4. D: pickpocket by reaching, after the alarm-while-open test. Two days
   with the test.
5. E's physical drop into a container, if wanted.

## 5. The other route: OBVR's own container UI, no engine menu (large)

- OBVR draws its own panels already (the settings menu: `ui/SettingsMenu`,
  `MenuCanvas`/`MenuPainter`/`MenuFont`, its own overlay layer). A panel of
  its own over the chest, listing the container's inventory, would need
  no menu mode at all: no pause question, no menu stack, no tile build, no
  close race, both hands free, items shown as the hand lists them.
- What it costs: reading a container's inventory (the base container's
  list plus the reference's changes - xOBSE walks both for its inventory
  commands), moving items with the engine's own remove-to-destination
  (ownership and the crime on taking an owned item seen must stay the
  engine's), and the pickpocket chance and detection, which on this route
  OBVR would have to call or reproduce. None of these addresses is looked
  up. Mod compatibility (DarNified UI, MenuQue looks) is lost for this
  panel.
- Weeks, not days; worth it only if route A's measured stall cannot be
  hidden or the stacking risk in D cannot be closed.
