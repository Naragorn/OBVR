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

## 6. Open (the tester, 2026-10-09, for later): the hand stays, the panel is touched

The tester, after the menus opened by reaching (controls-spec 4.31): "die
neuen menüs bei annäherung. machen wir die noch so dass die hand des
players nicht verschwindet und touch sensitive." Two asks, nothing built:

- **The hand stays.** While a panel opened by reaching is up, the player's
  hand is not to vanish: the hand that reached is still seen at the chest,
  as in the world. Why it goes today is not read: the panel is an engine
  menu, and the hand mode's menu path (`menusOnly`, the laser) takes over
  while a menu is open. To find first: which step hides the hands (the
  first-person hide, the hand bodies, or the hand model's own draw) for a
  ContainerMenu with the world running.
- **Touch.** The panel's buttons pressed by the finger, the way a touch
  screen works, besides the laser. The poke on the wrist's quad exists
  already (`vr::PokeOnQuad`, `PokeThresholds` in vr/HandInput.h: hover
  puts the cursor under the tip, a touch within `press` clicks once, out
  past `release` before the next); the container panel would take the same
  test against its own quad (its anchor and scale from 4.28). Section 2's
  C (the panel tight on the object) belongs with it.

## 7. Walking with it open, and switching (built 2026-10-10)

The tester (2026-10-09): "es muss möglich sein weiterhin sich normal zu
bewegen mit stick links bei den neuen offenen menüs, wenn ich zu weit weg
gehe schließt es sich automatisch. und ich kann zwischen container und
deren menüs wechseln ohne probleme indem ich meine hand dem jeweiligen
container annähere", pointing at Loot Menu (Nexus 48027, by Ersh: a
Fallout-4-style container panel for Oblivion that never enters menu mode
- the model for section 5's own-UI route, if route A ever runs out).

Section 2 F said the player's controls stay gated and need not change;
they do now, on route A: the one IsMenuMode site that gates the player's
update (0x0040DC61) answers "no menu" while the sticks move the player
under a container's menu the reach opened, with the laser's cursor and
click held back in those frames (they would reach the player's input as a
turn and an attack). The close is by the player's distance once they have
walked (`ReachWalkAwayMetres`, 1.5 m), by the hand's leaving before; a
free hand at another container switches. Controls-spec 4.35 has it all,
with the tests and the hand script.

Open from it: whether anything in the player's update misbehaves under a
menu (the first frames of the walk; the engine's own crosshair and HUD
run then), and the cursor drifting across the panel while the right stick
turns the player (the laser puts it back when the stick rests).

## 8. Open bugs, 2026-11-08: buying from a merchant (Windows headset, DLL 7d8b9397)

Read from OBVR.log of that run (the only barter in it, lines 32573-32950:
Dialog 0x3F1, Container 0x3F0, Quantity 0x3F8, Message 0x3E9, Container,
Dialog) and from the code. Nothing changed yet.

### 8.1 The merchant's menu is the small looting panel, not the standard menu

- **Wanted.** The barter menu at the size and place of the dialogue, the
  Tab and the Esc menus.
- **Measured (log).** The barter is the engine's ContainerMenu (0x3F0),
  opened from the dialogue: "Container: the menu opened with nothing
  activated in the last 180 frames ... its panel stays where menus open",
  "Menu pause: behind menu Container (0x3F0) the world keeps running".
- **Read (code).** `g_containerMenuEpisode` (CameraHook.cpp, by the menu id
  alone) does not tell a chest from a merchant, so `ContainerPanelUp` holds
  and the layer takes `[Look] ContainerPanelScale` 0.6. The dialogue's
  episode (`DialogMenuEpisode`) lasts the whole menu-mode episode, so it is
  still set under the barter and `DialogPanelWidth` applies
  `DialogPanelScale` 0.8 on top.
- **Derived.** The barter panel is 0.6 x 0.8 = 0.48 of the menus' width.
  The world runs behind it (`WorldPausesForMenu`, `containerRuns`), against
  the design rule in MenuPausePolicy.h that everything opened from a
  conversation stays paused.
- **No switch exists.** Only `[Look] ContainerInWorld=0`, which also takes
  the panel off chests and bodies.
- **Fix.** Tell a barter from a container: the dialogue in the menu stack
  under the ContainerMenu (`MenuStackEntries`), or the menu's own barter
  field (not read yet). A barter then takes the standard layer
  (`hudWidthMetres`, no container or dialogue scale, placed where menus
  open) and pauses the world as vanilla. **Small.** Pure decision in
  FrameLogic/MenuPausePolicy with unit flows, two call sites; a harness
  scenario needs a merchant save (dialogue, then barter), the feel needs the
  headset. Pickpocketing also opens the ContainerMenu without a dialogue, so
  it stays a container.

### 8.2 The merchants' sound stutters

- **Measured (log).** No compositor half-rate episode between the dialogue
  opening and its end (half rate ended 22 frames after the dialogue opened,
  the next began after it closed). The frame rate does not explain it.
- **Read (code).** The update step's sound pause is one of the redirected
  IsMenuMode sites (`kUpdateStepIsMenuModeSites` 0x0040DBAB). Under the
  barter's ContainerMenu it answers "running"; under the Dialog, Quantity
  and Message menus "paused".
- **Derived, unproven.** Every switch between the barter list and its
  quantity or message box pauses and resumes sound and AI. The merchant's
  voice and idle sounds start and stop: the stutter. A second candidate: the
  world (sound, animation, AI) runs while the dialogue's own state stays
  paused under it.
- **Cheapest check.** The same merchant with `[Look] ContainerInWorld=0`
  (vanilla pause): if the stutter is gone, 8.1's fix (the barter paused)
  fixes this too, at no extra cost.

### 8.3 The bought item did not arrive (to confirm)

- **Measured (log).** Container, then Quantity, then a **Message box
  (0x3E9)**, then the Container again, the Dialog, and later the inventory.
  The box's text and the button pressed are not logged; nothing about gold
  or items is logged.
- **Read (code).** OBVR moves no items in a barter. The transaction is the
  engine's own, through the laser's clicks.
- **Guessed, candidates.**
  - The box was the engine's refusal (not enough gold, or the merchant's
    gold).
  - The box asked to confirm and the laser's click landed on the wrong
    button.
  - The world running under the barter (8.1) disturbed the transaction. Not
    seen elsewhere; the engine expects a paused world there.
- **Cheapest step.** Log the Message box's text and the button the click
  took (small: a menu-change hook OBVR already has), or note gold before
  and after a purchase in the headset. 8.1's fix removes the third
  candidate.

### 8.4 Built, 2026-11-09

- **8.1 and 8.2.** A barter is told by the menu stack
  (`BarterInMenuStack`, MenuPausePolicy.h: the dialogue and the container's
  menu both in it; a chest, a body or a pickpocket has no dialogue there).
  - `WorldPausesForMenu` pauses as vanilla through the whole barter - the
    list, its quantity box and its message boxes - whatever
    `UnpausedMenus` and `ContainerInWorld` say; the pause line says "(a
    merchant's barter)". The pause no longer flips at every switch between
    them, the stutter's derived cause (8.2, not yet heard in the headset).
  - `PlayerControlsRunUnderMenu`: the sticks do not walk under a barter.
  - The layer: neither the looting panel (`LootingPanelShown`: never with
    the dialogue in the stack - the container's episode outlives the barter
    by the rest of the menu-mode episode, and the first harness run showed
    the looting panel taking over the dialogue after the barter closed) nor the
    dialogue's (`DialoguePanelShown`, vr/DialogPanel.h), so the menus' own
    width; placed anew where the head looks as the barter opens
    (`BarterBegan`, `game::BarterMenuBegan`, which logs "Barter: ...
    opened / closed"); the dialogue's own placement at the speaker comes
    back as it closes (`DialogPanelPlacement::Pending`).
  - Unit flows: menu_pause_policy_test, dialog_panel_test.
- **8.3.** Every message box's text and labels are logged as it is built
  ("Message box: ..."; game/MessageBoxTrace.cpp detours the builder
  0x005BC8B0 that both ShowMessageBox entries call, read with REA;
  MessageBoxLogic.h, message_box_logic_test), with the menu on top and
  whether a barter is up. The button the laser presses is in the laser's
  own "the tile under it" lines. The cause of the missing item is still
  open: the next purchase in the headset says which box it was.
- **Harness.** `tools/hand-scripts/merchant-barter.txt`, PASS 2026-11-09
  11:32 (Linux, headless): Rohssan talked to by the player's own A
  (StartConversation and Activate from the console opened nothing), the
  barter opened by the dialogue's Barter button (id 8) through
  ClickMenuButton, a box raised over it and answered (#4), the barter left
  by its Exit (#33). Measured in the log: "behind menu Container (0x3F0)
  the world pauses as vanilla (a merchant's barter)", the box's line "over
  menu Container (0x3F0), in a merchant's barter", the pause unchanged
  through list, box and list again, "Barter: ... closed", the dialogue
  re-anchored at the speaker, no looting-panel line. Not shown: the
  panel's width and place (headless pictures are black; derived from the
  code), and a purchase (the container list's rows have no fixed ids).
  The script parser keeps a '#' inside a word now (`detail::StripComment`),
  so `ClickMenuButton "#8"` reaches the game.
- **Open bug, 2026-11-09: `reach-open.txt` fails before any menu opens**
  ("reached - activated" missing; the placed chest is never the reach's
  target, the pick stays on another item), with this change (11:35) and
  with main 66b365c without it (11:36) alike - so not this change; the
  chest's looting panel was therefore not re-run in the harness. Its flow
  with no dialogue in the stack is the old expression (dialog_panel_test).
  Cause not looked into.
