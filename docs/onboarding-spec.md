# Onboarding, in-game help and hints - the polish gap to Titanfall 2 VR

The tester, 2026-10-12: "welche teile der VR Port implementation ingame brauchen mehr polish. Zielbild ist Titanfall 2 VR mod. ich denke da an ingame tutorials hilftexte und onboardings."

Scope: what a player meets on the first start and in the first hour, in both modes (VR View and Full VR). The inventory in section 1 was read from the code on 2026-10-12 at `2f2c2f8` (file:line at that commit). Nothing in sections 2 and 3 is built. Only the native MenuQue menus count as "what the player sees": they are on by default (`[Onboarding] NativePrototype=1`, src/core/Config.cpp:648); the overlay walkthrough is the fallback without MenuQue.

## 1. What is built today

### 1.1 The first start

| What | Where | What the player sees | Status |
| --- | --- | --- | --- |
| Native onboarding (MenuQue) | assets/menus/generic/OBVR_Onboarding.xml, src/ui/NativeOnboarding.h:40, src/game/NativeMenuPrototype.cpp:111-121 | One screen, two tiles. "Keyboard/Gamepad + VR" and "Full VR". Both are accepted and saved as `[Hands] Enabled`; then `ShowAtStart` is written to 0. Choosing Full VR opens the settings' Comfort view once the main menu is back (`StepComfortPage`, src/ui/NativeSettings.h:33). | Built. The native onboarding was confirmed in the headset by the tester 2026-09-09 (docs/native-menus.md, "Validation"); which tile was chosen then is not recorded. |
| The Full VR tile's whole introduction | OBVR_Onboarding.xml:60 | "Experimental. Starts in first person. Left B: inventory." | **Gap:** that is the entire text a Full VR player gets. Nothing about recenter, drawing a weapon, blocking, teleport, the quick menu, the hand fit. |
| Overlay walkthrough (fallback) | src/ui/Onboarding.cpp:56-139, src/camera/CameraHook.cpp:8151 | Five pages: Welcome, Controls, Comfort, Teleport, Done. "Standing Experience" is shown and refused (`kStandingUnderConstruction`, :56). | **Stale:** refuses what the native tile accepts; "snap turning is planned" (:97) although `[Look] SnapTurning` exists; every text is keyboard-centric ("Del recenters", "arrow keys steer OBVR's menus"). |
| Hand fit guides | src/game/HandAdjust.h (RequestHandAdjustGuide, RequestHolsterFit, RequestStowPlaceWindow); opened only from NativeMenuPrototype.cpp:130-137 | "Adjust your hands", "Weapon places" (three steps), "Place the stow spot": finished guided windows with their own text (ui::GuidePanelText, CameraHook.cpp:2044-2063). | Built, **never offered.** A player reaches them only through a settings row they do not know exists. |
| Left-handed | `[Hands] LeftHanded`, docs/left-handed-spec.md | A settings row. | Not asked on the first start. |

### 1.2 Help inside the game

| What | Where | Today | Status |
| --- | --- | --- | --- |
| Settings menu | src/ui/SettingsList.cpp (215 definitions), src/ui/NativeSettings.h:5 (`kNativeSettingsRows = 7`), :116 (`SettingsView All / Comfort / Sections`) | About 31 pages of seven rows, Previous/Next, Page Up/Down. Each row has one help line. The Sections view filters by mode. Reset and Undo exist. | **Gap:** no jump to a category, no "the ten that matter" page, no search. The first row a new player sees is "Recenter view", the second "Head movement". |
| In-world hints, Full VR | reach ring and icon (`[Hands] ReachRing`, `ReachTooltip`), teleport arc, quick-menu ring, laser dot | Visual only, no words. | A player who does not know that the grip at the hip draws the weapon, that a raised hand blocks, that both sticks held for 3 s recenter, or that left A drops in the inventory, never finds out in the game. |
| Vanilla tutorial messages | the engine's MessageMenu (0x3E9, src/game/MenuType.h:30); the prison intro's quest messages | Shown as the game writes them: "Press W", "Press E", the keyboard names. | **Gap, unverified in the game:** no code rewrites them for the controllers; I found none. Where the engine builds the key name for a message has not been researched. Titanfall 2 VR rewrites its prompts (UploadVR, 2026-10-06, named as the reason it "feels like a native VR game"). |
| Failures | OBVR.log only | SteamVR not running, `openvr_api.dll` missing, MenuQue missing, Enhanced Camera loaded: the game runs flat or without menus, and the headset says nothing. | **Gap.** A native notice window exists (the update notice, `OBVR_Update.xml`, NativeMenuPrototype.cpp:521-548) and could carry these. |
| Update notice | src/core/UpdateNotice.h, NativeMenuPrototype.cpp:521 | A native window in the main menu with the new version. | Built. |

### 1.3 What the public texts say

| Where | Says | True today? |
| --- | --- | --- |
| README.md:8-11 | "Playing standing with motion controllers is being built right now and is still switched off." | No. The native tile offers Full VR and saves it. |
| README.md:40-41 | "the welcome walkthrough shows it but won't let you pick it" | Only the overlay fallback refuses. |
| OBVR.ini:1609 | "UNDER CONSTRUCTION. None of this has been in a headset yet, and it is not working as intended" | No. Full VR has been played in the headset since 2026-09-27 (docs/controls-spec.md). |
| OBVR_Onboarding.xml:60 | "Experimental." | Yes. |
| docs/native-menus.md ("Restored to main") | "Only Keyboard/Gamepad + VR can complete onboarding on current main." | No, superseded by NativeOnboarding.h:40. |

Four sources, three answers. This is the cheapest thing on this page to fix and the one a new player meets first.

## 2. The target picture, from Titanfall 2 VR

What CircuitLord's mod does, as far as the published sources show it. None of this was read from its code.

| Titanfall 2 VR | Source | OBVR today |
| --- | --- | --- |
| One installer: finds the game, checks the EA sign-in, installs into its own profile, "Launch in VR", update check before every launch, crash report for Discord | github.com/CircuitLord/BigWalkVRInstaller README; vr.org 2026-10-04; arvrtips 2026-10-02 | Seven manual steps (README "Installing"). Out of this spec's scope, but the largest distance to the target. |
| The game's own button prompts rewritten for the VR controllers | uploadvr.com 2026-10-06 | Vanilla keyboard names (1.2). |
| A small VR settings menu inside the game's options: HUD, Titan HUD, gun angle | arvrtips 2026-10-02 | 215 rows, 31 pages (1.2). |
| Cutscenes: a 2D panel or full VR, a toggle | uploadvr.com 2026-10-06 | OBVR plays videos on the cinema screen; no toggle, and none needed so far. |
| A contextual cue in the world: the gun lights up when it needs reloading | arvrtips 2026-10-02 | The reach ring is the nearest thing. No cue for "draw", "block", "quick menu". |
| Recenter with the height: click both sticks | github.com/CircuitLord/CircuitLordVRModInstaller/issues/16 (the author's comment) | Both sticks held 3 s, or `Del`. Not told to the player anywhere in Full VR. |
| Snap turn in the options | roadtovr via vr.org 2026-10-04 | Built, settings row "Snap turning". |

## 3. Proposals, in the order to build them

### 3.1 One truth about Full VR's status

- Pick one word and one sentence. Proposed: **"Experimental"** - "Playable, tested on one setup, rough edges; please report."
- Put it in README.md:8-11 and :40-41, OBVR.ini:1609, the overlay walkthrough's Welcome and Controls pages (Onboarding.cpp:69-78), and strike the sentence in docs/native-menus.md.
- The overlay walkthrough's "Standing Experience" either accepts the choice as the native tile does (`kStandingUnderConstruction = false`, and `onboarding_test` follows) or the row goes. Accepting is the consistent one.
- "snap turning is planned" (Onboarding.cpp:97) becomes a `Look / SnapTurning` setting row.
- **Test:** `onboarding_test` for the choice flows; the texts by reading.

### 3.2 The Full VR first start as a chain

Today: tile, then the Comfort view. Proposed, each step one native page, each skippable, each reachable again from the settings' Help section:

1. **Recenter.** "Stand (or sit) the way you will play, look ahead, hold both sticks in for three seconds." The page waits for the recenter (`g_recenterRequested` or the chord) and says "Done" when it fired.
2. **Which hand.** Right or left (`[Hands] LeftHanded`). One question, two tiles, the same shape as the first screen.
3. **Hands.** The existing "Adjust your hands" guide (`RequestHandAdjustGuide`), opened from here instead of only from a settings row.
4. **Weapon places.** The existing "Fit weapon places" guide (`RequestHolsterFit`). The text already says what to do. (Built 2026-11-09 differently: a page after the T-pose, `FirstStartPage::Places`, with the four places as gold circles to drag - `vr/HolsterPlace.h`, docs/controls-spec.md.)
5. **The controls card.** One page, the table of docs/controls-spec.md section 1 reduced to twelve lines, by role ("weapon hand", "other hand") so it reads right for a left-hander. Reopenable from the settings (a Help row "Show the controls"). (Since 2026-10-21: five pages with a drawing each, see 4.)
6. **Comfort.** The view that opens today.

- The chain lives where `g_comfortPending` lives (NativeMenuPrototype.cpp:32, :374-388): one pending step instead of one flag, stepped the same way (`StepComfortPage` becomes a step over an enum).
- The guides in 3 and 4 already hand their finish back (`TakeHandAdjustFinishRequest`); the chain continues on it.
- Nothing in the chain should start a game. All of it runs in the main menu, where the native menus already work; the hand guides need the hands tracked, which they are in the main menu with `ControllerMenus` on.
- **Test:** the step logic as a pure function (`StepFirstStart(pending, genericRoot, mainMenuOnTop, guideFinished)`), every path: skip, finish, the main menu going away mid-chain, a failed save.
- **Built 2026-10-15 (ui/FirstStart.h, the host in NativeMenuPrototype.cpp; the tester: "wir brauchen die erststart kette, bei jedem start whl und pflicht beim new game dungeon").** Four of the six steps, in this order: **Recenter** (the adjust-hands window with its own words; the page sees the recenter - `game::NoteRecentred` from `DoRecenter`, taken by the page - and says "Recentred"; Next or Skip), **Which hand** (Right, Left or Skip; Right and Left write `[Hands] LeftHanded`), **the controls card** (its own window, two pages by role, Next/Back turn, Close goes on) and **Comfort** (the existing view; the "Start guide" switch is on it too, so the chain can be turned off where it ends). At every start with Full VR and `[Onboarding] StartGuide` (default on) in the main menu, once per start, nothing else of OBVR's on the way; choosing Full VR in the walkthrough starts it the same way (the comfort page alone follows only with the guide off). Esc ends the chain; a page whose XML is missing is skipped. Steps 3 and 4 of the proposal (the hands' fit, the weapon places) are not in the chain: the hands' fit offers itself the first time the player stands in the world (`FirstHandFitDue`, as before, now held back while the chain or the card is up), the weapon places stay a settings row. The pure parts (`FirstStartDue`, `FirstStartNext`, `ChooseFirstStart`, `FirstStartTextFor`, `FirstStartWindow`) are covered in native_settings_test; the harness drives the pages with the script actions `menu-primary`, `menu-secondary` and `menu-reset` (`first-start.txt`, begun in the world by the script's `action first-start`), and the runner writes `[Onboarding] StartGuide=0` into every run's OBVR-test.ini unless the scenario says otherwise, so the chain never stands in front of the main menu's console. Harness `first-start.txt` PASS and `controls-card.txt` PASS (Linux, 2026-10-15; five runs to get there: a window of OBVR's must not open before the script runs, Escape over a generic menu opens the pause menu on top instead of closing it, and the chain's place has to be kept across its pages). **Not built: "mandatory in the new game's dungeon"** - it needs the engine to tell a new game from a load (the first cell's form ID, or the game's own new-game path), not read yet; the chain can be skipped everywhere for now.

**The combat page (2026-11-20, Nadi: "mach eine eigene Kampf-Seite im Startguide").** In Full VR
the comfort page's Continue (or Esc) opens a second settings page, "Combat (OBVR)", before the
last page: Kick, Enemies take turns, Enemy attack speed, Dismemberment, and while the cuts are on
Blood and Gory wounds (`SettingsView::Combat`, `kCombatOrder` in ui/NativeSettings.cpp; the host
opens it the way it opens the comfort page, `ui::CombatPageFollows` in FinishSettings). Vanilla
VR has no such page - every row on it is Full VR's. The comfort page gained the hints over the
hand ([Onboarding] Hints) in Full VR. native_settings_test `TestCombatView` covers which rows
show, the order, the cuts' gore following the cuts, which page brings it, and that the page asked
for is taken when it opens or is skipped (`TakeGuideView`: a later comfort page is never a combat
page left over). newgame-after-guide.txt (the cuts pinned on) closes both pages with Esc and
expects the combat page's rows: PASS on Linux without SteamVR (2026-11-20), the log in order -
comfort page, combat page, "You're ready", the main menu. Not tried in the headset yet.

**No settings page for Vanilla VR (2026-11-20).** A Vanilla VR comfort page after the seated page
was built (be69280: Real height, Snap turning, Soft mouse turns, Controllers in menus, Animations
in first person, No knockdown) and taken back the same day: Nadi, the rows make no sense in Vanilla
VR, and no page at all rather than other rows. Kept seated still ends the guide. Found on the way:
Snap turning is offered in Vanilla VR's settings menu, but its only caller is the hands' controls
(`StepSnapTurn` in camera/hands/ControlsOut.cpp) - derived from the code, not tried in the game.

### 3.3 Hints in the world, the first few times

**Rebuilt 2026-11-14** (Nadi: "bau das" - the things OBVR adds that the game does not
have, the shield bash first, are found by no one but through the controls card).

- **A hint goes once it was done.** The shield bash's hint ends for good at the first
  shield bash; three showings are only the cap for one ignored. Learned and shown counts
  live in `OBVR_Hints.ini` beside the plugin (`[Hints] <Name>Shown`, `<Name>Learned`),
  not in the save (a new character does not get them again) and not in OBVR.ini (no
  write to the player's settings). Delete the file to see them all again. A harness run
  neither reads nor writes it (`HarnessRunActive`), so every run starts with all new.
- **Calm:** at most one showing every 90 s, any hint; 6 s up; the moment must hold 1.5 s
  first; nothing in a menu, a load or out of the world; the sign goes when the moment is
  gone or the thing is done.
- **Over the hand that does it:** a small sign (the holster sign's painter, 0.16 m) 12 cm
  over the controller - the shield bash over the other hand, the kick over the weapon
  hand, by role (left-handed swaps them). The words name no side.
- **Switch:** `[Onboarding] Hints=1`, the settings' Start & help "Hints in play" (Full VR's
  menu only).
- **Code:** `vr/HintLogic.h` (pure `StepHints`, `hint_test`: the wait for the moment, a
  broken moment, the show time, the gap, the cap, learned, learned while up, learned from
  the file, off, quiet, the moment gone, the order, bad frame times, the words),
  `camera/hands/Hints.{h,cpp}` (the file, the sign, its warm-up), CameraHook
  `StepHintsInPlay` (the moments read only while a hint is still open).
  Harness: `hint-shield-bash.txt`.

| Hint | Moment | Learned at | Over | Harness |
| --- | --- | --- | --- | --- |
| Yield - "Both palms to them, rock them" | in combat, `[Hands] Yield` on, and a guard or a lawful NPC among those fighting the player (vr::HintLawfulFoe: an NPC whose class has the guard flag, or responsibility 50 or more; game/HintReads.h, read from the combat ring's members - so only with `CombatRing` on) | the yield's gesture beginning its drive (GesturesState.yields) - the game may still refuse the yield | weapon hand | hint-yield.txt, hint-yield-bandit.txt (PASS on Linux, 20261114-195830 and 20261114-150006) |
| Draw your weapon - "Grip it where it rests on your body" | in combat, a weapon equipped and not drawn, holsters on | a reach that drew (HolsterState.draws) | weapon hand | seen in hint-shield-bash's first run (07:41): "Draw your weapon" came first |
| Shield bash - "Drive the shield into them" | in combat, a shield worn, `[Hands] ShieldBash` on | a shield bash (StepShieldBash) | other hand | hint-shield-bash.txt |
| Parry - "Meet their blade with yours" | in combat, a weapon drawn, `[Hands] WeaponParries` on | a parry (NoteParry, HintsState.parries) | weapon hand | hint-parry.txt PASS 2026-11-14 |
| Kick - "Flick this stick up" | in combat, kick on (the default binding is the weapon hand's stick flicked up) | a kick (KickState serial) | weapon hand | none yet |
| Stow it - "Let go of it at your body" | an item held, not food or drink, `[Hands] StowAtBody` on | a stow taken (HeldState.stows) | the holding hand | learned in hint-eat.txt; the sign unrun (stow.txt fails on Linux before the hint) |
| Eat or drink - "Let go of it at your mouth" | food or drink held (not a poison), `[Hands] EatAtMouth` on | a meal at the mouth (HeldState.meals) | the holding hand | hint-eat.txt |
| Potion belt - "Keep potions at a weapon place, grip them there" | `[Hands] PotionBelt` on and a potion held (not a poison), or hurt in a fight (health 40 % of its base or less, vr::HintLowHealth) with a potion kept at a place | a potion drawn from a place (PotionBeltState.draws); keeping one is not the deed | the holding hand, or the other hand when reaching | hint-potion-belt.txt (PASS on Linux, 20261114-150141) |
| Throw it - "Swing and let go" | anything held | a weapon or torch thrown, or an object let go at 2 m/s or more (HintThrowCounts; a thing set down does not count) | the holding hand | hint-throw.txt PASS 2026-11-14 |
| Open the door - "Grip the handle, pull or push" | a free hand at a door's handle (DoorsByGrip) | a door opened by the grip (OpenByHandState.opens) | the hand at the handle | hint-door.txt |
| Throw your weapon - "Thrown weapons return to your hand" | in combat, a weapon drawn, `[Hands] BounceBack` and `ThrowHits` on | a thrown weapon or torch sent back to the hand after its hit (HintsState.bounces, camera/hands/ThrowHits.cpp) | weapon hand | the moment by hint_test; learned in throw-bounce-back.txt; the sign itself unrun (the parry's hint takes the same moment first, and 90 s pass before another) |

The bounce back's hint added 2026-11-18 (Nadi: bounce-back "default an. teil des
onboardings"), last in the order: the fight's other hints come first.

The first six built 2026-11-14 (Nadi: "und das onboarding"), the parry and the throw the
same day (Nadi: "ja go", from docs/features-vs-vanilla.md); the order above is the order
two moments of the same frame are taken in. The moments are vr::HintWantsFor (hint_test);
hint-eat, hint-shield-bash and hint-door PASS again with all eight. The yield and the potion belt
followed on 2026-11-14 (Nadi: the yield "nur wenn man gegen good guys kämpft also
responsibility eher hoch oder guards"): the yield first in order (against a guard,
giving up is the way out), the belt after eating and before throwing.

- **Show the hints again:** Start & help's button (Full VR's menu, `SettingAction::ShowHintsAgain`,
  `hands::RequestHintsReset`): every hint new at the next frame, the file rewritten.
  The older ideas for this section (inventory, teleport,
  recenter) wait behind them.
- **Open:** the headset - whether the sign over the hand is seen in a fight, and whether
  the pace annoys.

### 3.4 Vanilla's prompts in Full VR

- **Decided 2026-10-16 (the tester: "wir werden versuchen die tutorial cards
  im new game dungeon deren texte zu ersetzen ... mit unseren vr controls").**
  The tutorial cards of the new game's dungeon - the prison intro's messages
  that name the keyboard, mouse and gamepad controls - are to say OBVR's VR
  controls instead. Two routes, neither researched yet:
  - **The DLL alone (try first):** catch the message's text where the engine
    builds it or where the MessageMenu (0x3E9) is given it, and swap it from
    a table by role - no plugin in the load order, one zip as now. Cost:
    small to medium once the place is found; the research is the unknown
    (REA over the message path from the quest script's Message command to
    the menu's text tile), and a new game in Full VR is the test, in the
    harness through a save at the dungeon's first message.
  - **An ESP:** override the tutorial quest's scripts with the texts changed
    (the messages are compiled script text in Oblivion.esm). Cost: medium -
    the Construction Set or a script compiler, a plugin to ship and to keep
    in the load order, conflicts with mods that touch the same quest; sure
    to work, since it is what the game reads anyway. Taken if the DLL route
    cannot catch the text.
  - Either way the words come from one table that the controls card
    (ui/NativeControlsCard.h) and the README's table are kept to.

- **Researched 2026-10-16 (REA over Oblivion.exe, UESP, Oblivion.esm read
  directly):**
  - The cards are the Tutorial quest's stage entries (MQ01, stages 10 to 96;
    UESP "Oblivion:Tutorial", Journal Entries). The esm carries the raw text,
    e.g. stage 10's CNAM: `To move forward, &sUActnForward;, &sUActnBack; to
    move back. To move left, &sUActnSldleft;. To move right, &sUActnSldright;.
    The mouse turns you. To run, &sUActnRun;.` The console and PC variants are
    separate entries chosen by their conditions.
  - The tokens are resolved at text layout, not in the quest: the font's
    layout routine `0x00575B40` scans a tile string for `&name;`, looks the
    name up in the settings map (`0xB35574`/`0xB3557C`) and, when the setting
    is one of the 29 control-name settings (`sUActnForward` ... `sUActnGrab`,
    engine defaults "Forward", "Back", "SlideLeft" ..., the array at
    `0xB399D0` in control-index order 0..28 = xOBSE's `kControl_Forward` ...
    `kControl_Grab`), calls **`0x0057C240(control, char out[1024])`**, which
    reads the control map (`OSInputGlobals` `0xB33398`+0x20: keyboard +0x1B7E,
    mouse +0x1B9B, joystick +0x1BB8), takes the button's name from the key
    (`0xB39578`), mouse (`0xB39554`) or joystick (`0xB39930`) name table and
    formats `"%s %s"` with the "press" string at `0xB38F18` - "press W" - or
    the unbound string at `0xB38F20`. The layout splices that into the text.
  - The card is the quest update's Generic menu (0x3F3: "Quest updated",
    the quest's icon, name and date, the entry's text, "Continue"), seen in
    the harness 2026-10-16 - not a MessageMenu, so `ShowMessageBox`
    `0x00579C10` is not its door. The window is built where the
    `sQuestUpdatedText` setting (its object at `0xB382D8`) is read:
    `0x006697A0` (`LIKELY` the quest-update window's builder; it also calls
    `QueueUIMessage_2`), to be read for step 2's hook. The engine's own
    word is "push", not "press": "push F", "push L-Button", "push Tab".
- **The DLL route, two steps:**
  1. **Detour `0x0057C240`** (cost small): with the mode on, answer OBVR's
     phrase for the control index from a pure table (`NameForControl(control,
     mode)`, every index covered by a test, the words from docs/controls-spec
     section 3 and the controls card), and let the engine answer otherwise.
     Every `&sUActn…;` in every text follows - the cards, any prompt built
     the same way - and the token names are engine strings, so this holds in
     every language. Test: the harness with `setstage MQ01 10`, the detour's
     log line and the card's picture. Risk: the Controls options menu, if it
     builds its key column through the same call, shows the VR phrase there
     too - harmless, to be looked at in the picture.
  2. **Whole cards** (cost small to medium): a detour where the quest
     update's window takes the entry's text (in `0x006697A0`, to be found)
     that matches it against the known MQ01 entries (the English esm text
     up to the first token) and hands the window OBVR's card instead -
     for the sentences no token reaches ("The mouse turns you", "Click on the
     fist icon", "Shift-click to drop"). The texts are the work; one table
     shared with the controls card. A non-English esm matches nothing and
     keeps step 1's result. Test: one harness scenario per card by
     `setstage`.
  The ESP stays the fallback for a text the window never takes that way.
- **Built 2026-10-16, step 1:** `game/ControlWords.h` detours
  `0x0057C240` (seven entry bytes, cdecl); `ui/ControlPhrase.h` is the
  table - 29 controls, either hand's words from `[Hands] LeftHanded`; the
  attack's words follow how the bow is shot (`BowWords` from `BowByHand` and
  `ArrowWithGrip`: "hold the right trigger to grab an arrow and release to
  shoot", "... grip ...", or "hold the right trigger to draw and let go to
  shoot" with the trigger draw; the tester, 2026-10-16); always
  run, auto move, change view, quick save and quick load stay the engine's -
  and `control_phrase_test` covers every index with both hands, the five left
  to the engine and the buffer's edges. Wanted each frame while the mode runs
  (`SetControlWordsWanted` from `UpdateHandMode`); no setting of its own, the
  words follow the mode. Harness `tools/hand-scripts/tutorial-cards.txt`
  PASS on the Altmer prison save (OBVRHarnessTall): stage 28's card read "To
  ready your weapon or fists, reach to your left hip and squeeze the right
  grip, or make a fist. To attack, swing the weapon - with a bow, hold the
  right trigger. To block, raise your left hand with the shield on its arm,
  or hold the blade across the blow.", stage 34 "To cast your selected
  spell, pull the left trigger. If you want to change spells, press B on the
  left controller ...", stage 80 "... flick the right stick down, or crouch."
  Found on the way: a card up holds the next quest update back; Escape over
  it opens the Pause menu on top of it; xOBSE's `CloseAllMenus` puts it away;
  a stage the save has passed (10, the walk) raises no card. Open: the
  headset; whether the Controls options menu shows the phrases in its key
  column (not looked at).
- **Built 2026-10-16, step 2:** `game/TutorialCards.h` detours the stage
  entry reader `0x0052AF40` (six entry bytes, thiscall on the stage item
  with the quest as its argument; it reads the entry's CNAM from the quest's
  record on demand and answers the text at `0xB36300`) - the quest update's
  notifier `0x006697A0` hands that text to the card (the Generic menu from
  `quest_added.xml`), so with the mode on an entry that is one of
  `ui/TutorialCards.h`'s nine (stages 10, 15, 22, 26, 36, 56, 76, 78, 94 of
  the English esm, matched whole) is answered with OBVR's words: the mouse
  turn becomes the weapon hand's stick with the teleport, "Shift-click to
  drop" the other hand's A held, the hotkeys the ring, the bow by how it is
  shot (`BowWords`: the arrow taken over the weapon hand's shoulder by the
  trigger or the grip, nocked behind the bow, drawn and released; or the
  trigger draw), the containers "or just reach into it". The cards
  keep vanilla's tokens where a control is named and name no side
  themselves, so step 1 gives either hand its words; another language or a
  mod's text matches nothing and keeps its own. Left as vanilla: the
  lockpick card (70; whether the pick takes A or the trigger in Full VR is
  not known), the map and quest-list cards (94's "click" works with the
  laser as it is), the bars "at the bottom left" (72-74; the HUD hangs on
  the wrist). `tutorial_cards_test` (every card whole, one character off
  matches nothing, every token a control with words, no side named).
  Harness `tutorial-cards.txt` PASS with two more cards: stage 26 read
  "Click an item with the laser to equip it. Click again to unequip it. To
  drop an item, hold A on your other hand over it.", stage 76 "... with the
  laser on the item, click the right stick to open the hotkey ring, move
  your hand to a slot and click again ..." (the token's "push F1" became
  "click the right stick"). Open: the headset; the journal's quest pages,
  if they read entries the same way, show the same words (not looked at).
- The in-menu prompts ("Press E to take") are not researched: whether the HUD
  builds them through the same tokens is open.

### 3.5 A way into the settings

- A **"Basics"** view as the first page: Recenter, Full VR mode, Snap turning, Snap angle, Eye separation, Head movement, HUD on the wrist, Left-handed, Adjust hands, Fit weapon places, Show the controls, Introduction at start. Twelve rows, two pages.
- The **Sections** view gets a category page first: one row per category, click jumps to it, Escape comes back. The categories already exist (the first string of every definition).
- **Test:** `native_settings_test` for the view's rows and the jump.

### 3.6 Failures told in the headset

- Reuse the update notice's window (`OBVR_Update.xml`, one line and OK) for: SteamVR not running or `openvr_api.dll` missing ("OBVR: no headset found - the game runs flat. Start SteamVR and restart."), MenuQue missing ("OBVR: MenuQue v16b not found - simpler menus"), Enhanced Camera loaded ("OBVR: Enhanced Camera is loaded, OBVR stays off").
- These show on the monitor too, which is where a player without a working headset is looking.
- The texts are the log's reasons, already written at those places (OpenVRBackend's fallback, the camera site check at `0x0066BE6E`).
- **Test:** `update_notice_test`'s shape for the new lines.

## 4. Open

- **Fixed 2026-10-15 (3.1, "one truth"):** the overlay walkthrough accepts Full VR as the native tile does (`kStandingUnderConstruction` and `StandingUnderConstruction()` are gone, onboarding_test follows); its texts say "beta" and name the hip draw, the block and the stick chord; "snap turning is planned" is gone. README, docs/nexus-description.bbcode, the INI's [Hands] header, the native tile's words, docs/native-menus.md, the test-report form and docs/hand-tracked-mode.md (the body, the swish) say the same thing: **Beta**, playable, tested on one setup, rough edges - and the beta build is handed out on Discord only (see 5).
- **Verified 2026-10-16:** the vanilla tutorial cards named the keys in Full VR ("push F", "push L-Button"); since 3.4's step 1 they say the mode's words.
- **Not verified:** docs/next-up.md "New games": the walkthrough and the hand guides around the intro and the character creation have not been tested in a headset.
- **Decision for the tester:** the status word ("Experimental" proposed), and whether the first-start chain (3.2) may run before any game is loaded - it asks for the controllers in the main menu, which `ControllerMenus=1` already gives.
- **Asked 2026-10-21 (Nadi, after the beta 5 round), built the same evening:**
  1. *The turn speed on the comfort page when snap turning is off* ("im onboarding auch eine option
     anbieten beim smooth turn (also snap turn off) die geschwindigkeit zu setzen"). In Full VR the
     comfort view offers "Stick turn speed" (`[Hands] TurnSpeed`, the stick's smooth turn, Hands:
     moving) while snap turning is off - one rule in `SettingShownIn` (ui/NativeSettings.cpp); VR
     View's eased mouse turn (`[Look] TurnSpeed`) is not the one offered. native_settings_test: every
     combination of snap, instant, vignette and Full VR (sixteen), the row's place between
     Left-handed and the start guide, and gone again with snap on. The comfort page logs its rows
     ("Native onboarding: comfort page open - rows: ..."); `first-start.txt` PASS with
     "Snap turning, Left-handed, Stick turn speed, Start guide".
  2. *The controls card on more pages* ("die controls cards gerne mehrere statt alles auf einem haufen
     gepresst"): five pages, five lines each, every line at most 56 characters - the harness picture
     showed the old twelve lines of up to 72 characters wrapping into each other at the card's width.
     The weapon hand's buttons, the weapons' places, the other hand's buttons, holding and opening,
     both hands; Next walks them, Start over on the last, Close on all (`ControlsCardFor`,
     `ControlsCardTurned`, native_settings_test). The first start's chain shows the same card.
  3. *Drawings instead of text where they make sense* ("kann man in menuque zeichnungen anzeigen?",
     "zeichnung der controller mappings statt text wo immer das sinn macht"): every page has one, as
     an `<image>` tile of the card's own XML - MenuQue only hosts the window, the tile is the
     engine's. Our own line drawings in the parchment's ink, drawn by `tools/make-controls-drawings.py`
     (Pillow, Noto Serif Bold labels, DXT5 DDS by ImageMagick): a generic controller (stick, A/B -
     "A / X", "B / Y" for Quest's left -, trigger, grip) for each hand's buttons, the body from behind
     with the weapons' places, the chest's gold ring with an item brought to it and a chest reached
     into, both controllers with the two rings' stick clicks. Each drawn for a right-hander and
     mirrored for a left-hander: ten DDS under `assets/textures/Menus/OBVR/`, deployed by
     `tools/build-linux.sh --assets` to `Data/Textures/Menus/OBVR/` (the case of the game's own
     folder) and packed by both release scripts. The page's drawing is shown by its tile's alpha
     (`ControlsCardDrawingTrait`). The places drawn are the defaults; a side chosen in the settings is
     not drawn. Harness `controls-card.txt` and `controls-card-left-handed.txt` PASS: every page's
     title with its drawing (0, 2, 4, 6, 8 right-handed; 1, 3, 5, 7, 9 left-handed), the pictures
     show each drawing and every line on one line.

- **Headset 2026-10-22 (Nadi): "onboarding ok"** - the five-page card with its drawings and the
  comfort page's stick turn speed accepted.
- **Open bug, 2026-10-22 (Nadi): "im onboarding sind texte oft größer als buttons"** - button labels
  larger than their buttons in the onboarding windows. Not measured. What to read first: the
  first-start and controls-card harness pictures at every page (`first-start.txt`,
  `controls-card.txt` keep one per page), against the labels' lengths - the buttons are 200 wide with
  a wrapwidth of 176 and 45 tall in OBVR_Controls.xml, OBVR_AdjustHands.xml and OBVR_Onboarding.xml,
  so a label that does not fit one line at font 3 wraps into a second line below the button's
  bottom ("Adjust again", "Start over", "Decide later"-sized words are the candidates). The fix is
  either shorter words (a unit flow that holds every button label to the length that fits, as the
  card's lines are held to 56 characters) or wider buttons in the XML - the words first.
  **Fixed 2026-10-22.** Measured in the first-start pictures: the comfort page's (the settings
  window's) "Reset to default" broke into two lines in its 170-wide button and "Continue" into
  "Contin-ue" in its 103-wide one; every OBVR button label else fits. Font 3 read off the pictures:
  a 62-character line wraps at 688, "Continue" at 85, "Previous" fits 102 - at most 12.5 units a
  character. The settings window's bottom row re-laid within its 828: Reset and Undo 226 wide,
  Close/Continue 121. NativeMenuAssetsTest.py now holds every button label of every OBVR window -
  the words set at run time listed per button - to one line at 12.5 a character, and its wrap
  inside its button. The harness picture of the comfort page: all three on one line.
- **Built 2026-10-22: the T-pose (Nadi: "T Pose wir machen A"), and it replaces the recenter page**
  ("sobald wir t pose haben können wir natürlich dann den recenter prompt im onboarding entfernen
  ... die t pose macht das ja dann auch"). The chain is now Measure, Which hand, the controls card,
  Comfort. The measure page: stand straight, look ahead, arms out like a T, both triggers held half
  a second (`vr::StepTPose`: each hand at least 0.45 m out to its side, level within 0.20 m,
  between 0.55 m under and 0.10 m over the eyes, at most 0.35 m ahead; again only after both
  triggers let go). Then: the recenter (that way becomes forward and level, the flat picture and
  the menu's quad brought in front), the eyes' height over the floor from OpenVR's standing
  universe (y = 0 the floor, openvr.h), the span between the controllers, and `vr::MeasureBody`:
  the weapons' places, the quiver's and the stow spot's scaled from their defaults by eye height /
  1.6 (held to 0.8 to 1.25; the defaults stand for eyes 1.6 m up), saved to the INI and replacing
  places fitted before (the page says so), the sides kept; and `[Body] ArmStretch` = 1.05 x the arm
  measured / the body's arm (36.63 units), at least the default 1.15, at most 1.5 - the arm being
  0.395 x the span (derived: span fingertip to fingertip about the stature, the controllers 0.846 of
  it apart, shoulder to wrist 0.334 of it after Contini, Drillis and Bluestein; not measured on
  people). "I'm seated" opened an arms-only measurement at first; since the same day (Nadi: "wenn
  man i'm seated drückt dann lass machen dass hands off ist also full vr aus und wir zum seated +
  gamepad + keyboard mode zurückkehren ... mit back button falls user sich nicht entscheiden
  können") it opens the **seated page** instead: "Seated with a gamepad" says Full VR is for
  standing with motion controllers and seated play is gamepad, mouse and keyboard with the head
  looking; **Keep seated** turns Full VR off (`[Hands] Enabled=0`, as the walkthrough's
  Keyboard/Gamepad + VR tile does) and ends the guide, **Back** returns to the measure page with Full
  VR untouched. Later the same day the seated page went and came back (Nadi: "erklärseite muss weg",
  then "die erklärseite beim im seated kann bleiben. ich meinte die nach dem t pose"): what went is
  the **page after the pose** - a measurement goes on to Which hand by itself
  (`ui::ChooseFirstStartMeasured`), the numbers in the log only; a refusal stays on the measure page
  as "Measure again" with its reason. The walkthrough's tiles say it too: "Seated: gamepad, mouse or keyboard ..." and
  "Beta. Standing with motion controllers ...". Out of range nothing is
  set and the page says why (hands under 1.0 or over 2.2 m apart; eyes outside 1.2 to 2.1 m keep
  the places only). The Body settings, "Arm stretch" among them, were in VR View's menu only and
  are in Full VR's now too (Nadi: "ich bin mir sicher das wir armlength oder sowas in den settings
  haben" - it was there, out of Full VR's reach). Unit flows: body_measure_test (the pose's every
  refusal, the hold, the scale and its limits, the arms and theirs, seated, no floor, refusals),
  native_settings_test (the chain without the recenter page, the measure page's buttons and words,
  the Body section in both modes). Harness `tpose.txt` PASS headless: the synthetic head 1.8 m up,
  hands 1.5 m apart - "recentered ... T-pose", "the places scaled 1.125", "the arm 0.59 m,
  ArmStretch 1.19"; since the page after the pose went: "I'm seated" and Back first, then the pose -
  "measured, on to the next page", "Which hand holds the weapon?", no "Measure again".
  **The T-pose drawn on the measure page (2026-10-22**, Nadi: "beim t pose im onboarding füge noch
  ein bild hinzu welches dieses anschaulicher macht"): a body standing straight, arms out level, a
  controller in each hand with the trigger in gold, the eyes' height over the floor marked beside it
  (tools/make-controls-drawings.py, `Menus\OBVR\tpose.dds`, 512 x 512 shown at 256). The adjust-hands
  window carries it hidden with a narrower copy of the text (`messagenarrow`, 380 wide); the measure
  page alone shows both and hides the wide text (`FirstStartText::picture`). Harness `tpose.txt`
  PASS headless with no UI command failing; its picture shows the drawing beside the text and the
  seated page without it.
  `first-start-seated.txt` PASS: "I'm seated" (the seated page), Back (the measure page again),
  "I'm seated" and Keep seated: "kept seated: Full VR off and saved, the guide ends", the player's
  OBVR.ini still Enabled=1. `first-start.txt` PASS on the new chain. Saved to the run's
  OBVR-test.ini; the player's OBVR.ini unchanged (checked). Not covered by the harness: a real
  floor height from SteamVR (the synthetic head answers its own height), and how the scaled places
  feel. A settings row to measure again outside the guide was not built; the Start guide row brings
  the guide back.
- **Open (Nadi, 2026-10-24): show all the tutorial cards again and rework
  them** ("tutorial help cards nochmal alle anzeigen lassen und
  überarbeiten").
  - Show again: every card of the prison intro, one after another, without
    playing the intro again - for reviewing them in the headset, and (to be
    decided) as a row for players, next to "Show the controls" in Help.
    Proposed: OBVR hands each stage's text (vanilla's or 3.4's words) to the
    same card the quest update uses (notifier `0x006697A0`, the Generic menu
    from `quest_added.xml`), next on a click. Whether the notifier can be
    called outside a quest update is not looked at.
  - Rework: all of them, not only 3.4's nine (stages 10, 15, 22, 26, 36,
    56, 76, 78, 94) - also the ones left as vanilla: the lockpick (70), the
    bars "at the bottom left" (72-74; the HUD hangs on the wrist), the map
    and the quest list (94). What is wrong in each is to be seen once they
    can be shown; the words in `ui/TutorialCards.h`, `tutorial_cards_test`
    and `tutorial-cards.txt` follow.

## 5. The tester's decisions of 2026-10-15, towards the Discord beta

Asked what still had to be built for a beta release to the Discord testers, and
answered: "teste machen wir nach der beta runde lol. also ein vermerk in nexus
comment section vll zum discord channel für die hand tracking beta. für quest
hotkey button wird der rechte stick button long pressed. quest button gibts
nicht. das controller schema muss irgendwo dokumentiert sein. am besten im obvr
setting menü und in docs und readme. [...] wichtig ist mir nur das auf nexus
und gothub kein download möglich ist der beta nur in discord. joa gehen wir die
defaults durch. ja den discord post brauchen wir. stimmt wir brauchen die
erststart kette, bei jedem start whl und pflicht beim new game dungeon."

- **Headset tests after the beta round:** what is built now goes to the
  testers on unit tests and the harness alone.
- **The beta is Discord-only.** No GitHub release, no tag, nothing on Nexus;
  the public texts say so (4). Note for him: the CI keeps the DLL of every
  push as a workflow artifact (build.yml, "Keep the DLL"), which any
  signed-in GitHub user can download from the Actions page - if that counts
  as a download, the upload step goes (small).
- **A note in the Nexus comments** pointing to the Discord channel for the
  hand-tracking beta: the text is in docs/discord-hand-tracking-beta.md.
- **Quest: the hotkey ring by the right stick's button held** - built, see
  docs/controls-spec.md 4.4. There is no journal key on the Touch.
- **The controls documented** in the settings menu, the docs and the README:
  the README's table (Controls in Full VR), docs/controls-spec.md section 1,
  and the settings menu's card - Help, "Show the controls"
  (ui/NativeControlsCard.h, OBVR_Controls.xml: two pages by role, Next/Back,
  Close; the hand script's `action controls-card` is the harness's door, `controls-card.txt`).
  Built 2026-10-15. The card's words and the README's table have to be kept the
  same by hand.
- **The first-start chain (3.2)** - built 2026-10-15 as far as the main menu
  allows (see 3.2's "Built"): recenter, which hand, the controls card, comfort,
  at every start, skippable, `[Onboarding] StartGuide`. "Mandatory in the new
  game's dungeon" is open: the engine has to tell a new game from a load - the
  first cell's form ID, or the game's own new-game path - which is not read
  yet. Cost once that is known: small for the trigger, medium if the pages are
  to refuse Skip and Esc there.
- **The defaults, decided the same evening (the tester: "snap turning 1,
  snap instant 0, speed 12, vignette 1, grab reach 0,1, hands 1, contacttrace
  0"):** OBVR.ini ships SnapTurning=1, SnapTurnInstant=0, SnapTurnSpeed=12,
  SnapTurnVignette=1, GrabReachMetres=0.1, [Hands] Enabled=1 (the first start
  asks anyway), ContactTrace=0. The code's own defaults are unchanged; an INI
  without those keys keeps the old values.
- **The CI artifact is gone** ("ja kannste machen"): build.yml no longer
  keeps the DLL of a push, so nothing of a push can be downloaded from GitHub.
- **The beta zip:** the version is 0.3.0 (CMakeLists.txt; no tag, no GitHub
  release - the update check reads only the latest full release, so nobody is
  told of it), the archive is made on Linux by `tools/package-release.py
  --tag=-beta1` from the clang-cl build the tester plays, `dist/OBVR-0.3.0-beta1.zip`
  with the map beside it; the layout is package-release.ps1's. The post and
  the Nexus comment are in docs/discord-hand-tracking-beta.md.

## Open wish, 2026-10-26: a step for walking by swinging the arms

The toggle for walking by swinging both arms (controls-spec, "Open wish,
2026-10-26: walking by swinging both arms") should be offered in the
onboarding. **Built 2026-10-28** (Nadi: "default off und es kommt ins
onboarding user können dort entscheiden"): the page RunBy after Which hand,
on the adjust-hands window - "How do you run?", Swing arms (saves [Hands]
ArmSwingRun=1), Left stick (=0); ui/FirstStart.h,
native_settings_test. The controls card after it says the way chosen ("Stick:
walk. Swing both arms: run." or "Pushed in: run.").

## No Skip buttons (2026-10-28)

Nadi: "entfernen wir alle skip buttons im onboarding". Which hand and How do
you run? lost their Skip (the adjust-hands window's third button, hidden now);
a page goes on only by its choice. The measure page never had one (Next, I'm
seated), the controls card's Close goes on as before. Esc still ends the
chain - it is no button; ask if that should go too. native_settings_test
(the third button is nothing on both pages, not shown).

## Full VR or Play like Vanilla (2026-10-28)

Nadi: "'I'm seated' heißt nun 'Play like Vanilla' und das andere 'Full VR'".
The measure page's buttons are Full VR (Next before) and Play like Vanilla
(I'm seated before); the page it leads to is titled Play like Vanilla, its
buttons Play like Vanilla (Keep seated before: Full VR off, the guide ends)
and Full VR (Back before: to the measure page). The log line "kept seated"
and the words seated in the code stay. native_settings_test; harness
first-start-seated.txt and tpose.txt expect the new title.


## The choice first, then the T-pose (2026-10-29)

Nadi: "der full vr button führt zur T pose. nicht direkt dort. den anderen non full vr button
kürzen. statt tpose dort dann erklärungstext".
- The chain's first page is now the choice (`FirstStartPage::Mode`, "How do you want to play?"):
  a text that explains Full VR (standing, the motion controllers, a short T-pose next) and
  Vanilla VR (gamepad or mouse and keyboard, the headset looks), and that the settings switch it.
  Buttons: Full VR (on to the T-pose) and Vanilla VR (the shortened "Play like Vanilla but in VR",
  to the seated page).
- The measure page has the T-pose only: the pose goes on by itself as before; its buttons are Back
  (to the choice) and Vanilla VR. Full VR no longer skips the measurement from there - with no
  Next on the measure page, the chain goes on only by the pose (or Esc ends it).
- The seated page: title and keep button Vanilla VR, Back to the choice (`ui::BackPageFrom`).
- Unit flows: native_settings_test (the order, every button of the three pages, Back, the texts).
  Harness 2026-10-29, Windows, `-Direct`: first-start.txt PASS (now with the pose; its comfort-page
  expectation carries the Sneak indicator row, there since 864cb3c), tpose.txt PASS,
  first-start-seated.txt PASS. Headset open.

## The start guide switches itself off (2026-11-06; a tester on Discord)

A tester (3080 Ti, 5900X, hours of play): "the game wanting to recalibrate and ask if I
want to setup full vr mode every start". Not a fault of saving: the guide ran at every
start by the decision of 2026-10-15 (`[Onboarding] StartGuide=1`), its first page asks
Full VR or Vanilla VR and its second is the T-pose, and what it sets is saved already.

Built 2026-11-06 (Nadi: option A): a chain that comes through to the comfort page
writes `StartGuide=0` (`ui::FirstStartSwitchesOff`, the host's
`SwitchGuideOffAfterItsRun`); a chain Esc ended earlier, or one that kept Vanilla VR,
leaves the guide on for the next start. The comfort page shows the switch's row ("Start
guide", now off) and the settings' Help has it, to bring the guide back. Harness
(first-start.txt, 2026-11-06): "it went through, the guide is off from the next start
(Onboarding.StartGuide=0 saved)", the write into the run's own OBVR-test.ini. Not
checked: whether the tester had switched it off and it came back - that would be an
OBVR.ini that cannot be written, and the log line above says so then.

## The guide in the dome (2026-11-09)

Nadi: the start guide before the main menu, in OBVR's dome; the sounds on, the game's
music off, after a while something very soft; later his own track (a lo-fi remix OBVR
does not ship) from 2:28 to 5:43.

**Built** (`ui/GuideStage.h` pure, `render/StageHost.cpp` the host, `game/GuideTrack.cpp`
the engine and DirectShow side; `[Onboarding] GuideDome`, `GuideMusicDelay`,
`GuideMusicFade`, `GuideMusicVolume`, `GuideMusicFile`, `GuideMusicStart`,
`GuideMusicEnd`):

- **When:** the chain (or its comfort page) is up and the main menu is loaded. The
  guide begun in the world (a hand script's door) gets no stage.
- **The dome:** `IVRCompositor::FadeGrid(1 s, in)` with the loading stage already in
  place of the grid. Read: openvr.h's comment on `SetStageOverride_Async` ("Use
  FadeToGrid to reveal"), and Valve's own `SteamVR_LoadLevel.cs` calls `FadeGrid(t, true)`
  while the app still submits ("Explicitly fade to the compositor since loading will
  cause us to stop rendering"). Overlays stay over the grid, and the guide's window is
  on the menu overlay. Out again with `FadeGrid(1 s, false)`.
- **Handing over to the main menu's dome (2026-11-28, `ui/MainMenuDome.h`, not run in the game):**
  with `[Render] MainMenuDome=1` the main menu has its own dome round its unchanged picture. The
  scene is held while the main menu's dome or the guide's stage wants it, and the grid is faded in
  by the first and out by the last, so the guide ending over the main menu keeps the dome up (no
  fade out, no hold released); the guide's log line then says the main menu's dome stays. The guide
  hides the main menu, the title music and its film as before. With `MainMenuDome=0` everything above
  is as it was (guide-stage.txt and guide-stage-default.txt set it so).
- **The main menu hidden:** its TileMenu (`g_TileMenuArray[0x414 - 0x3E9]`) gets
  visible &false; (1) through `Tile::UpdateFloat` (0x0058CEB0); the guide's windows are
  menus of their own. Visible &true; (2) again at the end (`ui::MainMenuVisibleTrait`).
  Until 2026-11-10 it was 0 and then 1 - but 1 is &false;, so the main menu stayed
  hidden after the guide (below).
- **The title music silent:** the game's music player (`*(0x00B33398)+0x24`, read at
  0x005B5B36) keeps playing it; OBVR holds its `IBasicAudio` (+0x74) at -10000 every
  frame while the stage is up (the game's own volume setter may run any frame). At the
  end the game's setter `0x006AA1A0(volume at +0x2F0, false)` puts back the volume the
  game wants - with the title flag (0x00B3C0EC) set that is `fMainMenuMusicVolume`.
  Not touched: the flag, the player's track, its fades.
- **The soft track:** OBVR's own DirectShow graph, as the game builds its own
  (0x006AB160: CoCreateInstance of the FilterGraph, RenderFile, the IIDs read from the
  game's image at 0x00A78F64..0x00A78FC3), kept apart from the game's player so the
  game's music switching never sees it. After `GuideMusicDelay` (0 s since 2026-11-15 - Nadi: "musik muss sofort
  kommen" - 20 s before) it starts silent
  and ramps to `GuideMusicVolume` (0.25 of the file's full level, amplitude as 20 log10
  hundredths of a dB) over `GuideMusicFade` (1 s since 2026-11-15, 10 s before). `GuideMusicFile`
  (`Data\Music\OBVR\onboarding.mp3` since 2026-11-15, `guide.mp3` before: Nadi's own track,
  `assets/music/onboarding.mp3`, 184 s, deployed by `tools/build-linux.sh --assets` when the checkout has it; not in
  Git and not in the package - it is from "Oblivion in Lofi" by Samuel Sheppard, whose page allows live streams with
  a link and says nothing of shipping the file, read 2026-11-15; the README names it) is the guide's file; when it is there,
  `GuideMusicStart`/`GuideMusicEnd` (seconds, IMediaPosition) make a stretch that
  loops; missing, `Data\Music\Explore\atmosphere_04.mp3` plays whole and loops (the
  quietest exploration track, measured with ffmpeg ebur128: -19.9 LUFS, 281 s).
- **Unit flows:** guide_stage_test (up, not up, the delay and the fade, volume 0, the
  stretch and its loop, the fallback ignoring the stretch, a stretch from 0, a whole
  own file at its end, the end of the guide with and without the track, switched off
  while up, the helpers including NaN frame times).

**Harness** (`guide-stage.txt`, `start main-menu`, the guide on, delay 3 s, fade 2 s,
the game's atmosphere_04 named as the own file with a stretch 10 to 14 s, Linux,
headless): the order in the log is the guide begins, the dome faded in, the main menu
hidden, the title music held silent, the track started (282 s), at volume 0, to 10.0 s,
ramped to 0.300 (-1045), round to 10.0 s every 4 s, then Vanilla VR ends the guide:
the dome faded out, the main menu shown, the title music given back, the track closed.
Regressions first-start, tpose and weapon-places PASS.

**Not shown by the harness - the headset decides:**
- **The dome itself.** The compositor's grid alpha read a second after `FadeGrid(in)`
  was 0.00 in the headless run (SteamVR without a headset). Whether that is the
  headless compositor or the fade not taking while OBVR submits is open. If the
  headset shows the main menu's cinema picture instead of the dome, the next step is
  Valve's own: stop submitting the eye frames while the guide is up
  (`SteamVR_Render.pauseRendering`), the compositor then shows the stage by itself
  after ten frames. Cost small to medium (the submit path is OBVR's busiest).
- **Heard:** the title's silence, the track's level and its fade, the stretch's seam.
- **Whether the main menu's picture is all under the hidden tile** (its background
  image).

**Open:**
- **KB5124010:** the soft track is a second `RenderFile` in the process. On a Windows
  with that update and without Sep2026LoadingCrashFix, the second MP3 graph crashes
  (msmpeg2ac3dec) - in vanilla at the first load, with this at the track's start in the
  main menu. No new exposure, an earlier moment.
- **Paths:** ASCII only (widened byte for byte, as the game's own paths).

## The new game on Windows (2026-11-10)

Nadi's Windows run of 2026-11-10 (DLL 5d2352b0, logs `OBVR-2026-11-10_22-17-45.log`, the
new game, and `OBVR.log` of 22:20, the guide):

- **Fixed: the main menu gone after the guide.** The log of 22:20: "the first start's
  guide ends with the comfort page", "Native settings: closed", "Guide stage: the main
  menu shown again", the self-check panel's OK - then three clicks "sent to the none
  menu (on top: Main) ... the tile under it """: the main menu was on top and none of
  its tiles under the beam. The stage wrote visible 1 to show it, which Oblivion's XML
  reads as &false; (&true; is 2, `ui/NativeSettings.h` kXmlBool, UESP Oblivion
  XML/Entities). Now 2 (`ui::MainMenuVisibleTrait`, guide_stage_test). The harness
  `guide-stage.txt` read only the log line, not the menu; `newgame-after-guide.txt`
  (the click on "main_new" after the guide) catches it and is brought to the pages
  since 2026-11-09. Harness after the fix (Linux, headless, 2026-11-10 22:44, DLL a58ce5c5): newgame-after-guide PASS - "Guide stage: the main menu shown again", then "the tile under the cursor main_new", the Yes, the load, the race menu; the monitor picture at the mark "menu" shows the main menu again. guide-stage, first-start and weapon-places PASS.
- **The weapon places' window** (from the settings, and the guide's page): starts with
  "Look down to see the gold circles on your body" (`ui::kWeaponPlacesWindowMessage`,
  native_settings_test). Whether the longer text still fits its window: headset.
- **Open bug: the intro movie (the Emperor) did not show at the new game.** Read in the
  log: "Start new game?" Yes, Loading, the race menu opened, then about a dozen frames
  "Menu trace: cinema+overlay" (the trace is rationed, so how long is not in the log),
  then "Hands: right buttons=...0002" - the right B pressed - and right after it the
  race menu's live frames ("Chargen view: the race menu is up"). In a menu the right B
  is Escape (controls card: "in a menu: back, as Escape"), and Escape skips the movie
  (race-menu-newgame.txt, 2026-10-23: "the first Escape at 40.9 skipped the movie and
  the live menu frames began with Chargen view: the race menu is up"). Derived: the
  movie began and a B press ended it; whether that press was meant is the tester's to
  say. Not settled: whether the movie's frames reached the headset before it. Cheapest
  step: race-menu-newgame.txt with eye pictures at 5 and 15 s after the load, no Escape.
- **The new character had no hands after the race menu's Done - cause found.** Nadi:
  in the prison there were no hands right after Done. The log: "First person hide:
  "Foot:handRM" (NiTriShape@@) ... is hidden", three times, from the Done on; the loads
  of 22:08 and 22:13 (a female character) never hide it. `characters\_male\hand.nif` of
  the testers' body names its shape "Foot:handRM" (read on both installs with strings:
  "Bip01 L Hand", "Bip01 R Hand", "Foot:handRM", the texture footmale.dds). Full VR hides
  the first-person "LowerBody,Foot" (the boots, `ComposeHandsHideList`), and a name with a
  colon matches by what comes before it (`NameInList`, as the engine reads "UpperBody:X")
  - so the male hand was taken for a boot. The race menu's Done is where the race's male
  hand is put on. Fixed by the slot: under a worn item's node ("Hand  (00000907)") the
  item's slot decides - listed, every part goes; an entry naming another body slot does
  not reach it (`game::ShapeHiddenByList`, node_name_list_test); the hide line now names
  the worn node above. **Harness PASS 2026-11-11 10:33** (Linux, headless, DLL 783f1291):
  `newgame-hands.txt` - New, the movie skipped, the name typed, the Done, the race's Yes -
  "Foot:handRM" sits under "Hand  (000BE335)" and is not hidden; both hands in the eye
  pictures in the prison.
- **Fixed 2026-11-11: the intro movie in the headset.** The film never reached the headset -
  not a regression: on 2026-10-23 the eye pictures at the mark "movie" were black too
  (brightness 0.0); the Emperor's face then was the monitor copy (12-movie-game.png,
  7.4), not the eyes. Cause, read in the logs: OblivionIntro.bik is drawn into the back
  buffer while Loading and then the hidden RaceSex menu are up, so DeliverFrame's menu
  rules applied - over a pair held from a game loaded before New (Windows 2026-11-11)
  "held", the 2D pass drawing nothing, the old picture; with none held (Linux)
  "cinema+overlay", black eyes. The Windows log has "big quad MISSING" with three
  trigger pulls in that time: the pull at the black skipped the film. Fix: the movie
  player's record (0x00B33428, its HBINK at +0x00, state +0x20; read with REA at
  0x00410C40 / 0x004105F0 / 0x00410B00, game/EngineMovie.cpp) - a film playing with the
  main menu not in the stack takes the cinema screen on a frame with no world render,
  before every menu rule (camera/MovieScreen.h, DeliverFrame's movieOwnsScreen); the
  main menu's map loop keeps the main menu's delivery. newgame-intro.txt PASS
  2026-11-11 18:48: the eye pictures at 5, 12 and 22 s into the film 1.5, 2.2 and 5.4
  (were 0.0), the Emperor's face in them; newgame-after-guide and first-start PASS (the
  main menu path: no film owned while the main menu is up). Not checked: the credits,
  the outro and the Shivering Isles' dreams (the same path), Windows, the headset.
- **Open: Berelos' Quest 3 report** ("whenever I spawn in a new character, it won't
  stop moving forward and the spell circle hotkey immediately shows up"). Nadi's new
  game on Index controllers shows neither (no stuck stick, the spell ring not opened).
  No shared cause found with the missing hands; the controller-type guess (an Index
  emulation, see docs/self-check-spec.md) stands until Berelos' log comes.
  **2026-11-11, Berelos' logs read** (OBVR.log and .prev of his session, Virtual
  Desktop, "emulate index controllers" off): the Index guess is wrong - SteamVR
  reports "oculus_touch" in both hands, the action manifest is in use, the rings sit
  on the stick clicks. The session is a first start (the guide; he chose running by
  swung arms, Hands.ArmSwingRun=1 written) and a loaded save, with no character
  creation in it, and every sampled left stick reads 0.00,0.00 - so the stuck walk and
  the spell ring are not in these two logs. Open: the session where it happened, from
  his OBVR-logs folder. Also seen: his .prev froze before its first frame (watchdog,
  the render thread in amdvlk32.dll - an AMD card on the amdvlk Vulkan driver).

## The stow spot in the guide (2026-11-13)

Nadi, 2026-11-13: after the weapon places, the stow spot's window as a page of the
guide too, "ähnlich zum anderen: look below and ... dann kann man den kreis am körper
setzen". Built as `FirstStartPage::Stow` between `Places` and `WhichHand`
(`ui/FirstStart.h`), on the same adjust-hands window, with the settings' "Place the
stow spot" placing behind it (`vr/StowPlace.h`, `camera/hands/Held.cpp`
UpdateStowPlacing):

- **Text:** "Your stow spot" - "Look down to see the gold ring on your body. Let go of a
  held item inside it and the item goes into your pack. Close a grip on the ring to move
  it, the right stick sizes it. Done keeps it, Reset puts it back. The settings (Holsters
  & items) show it again later." Buttons Done and Reset, no Skip (the rule of 2026-10-28).
- **The ring** comes up with the page (StowPlaceCommand::Start), a grip closed on it takes
  it, the right stick sizes it - as in the settings' window.
- **Done** keeps it and writes `[Hands] StowRight/StowForward/StowUp/StowRadius`.
  **Reset** puts the default back (0.00 right, 0.17 forward, -0.33 up, 0.16 radius) and
  leaves the ring up to be moved again, like the weapon places' Reset - a new command,
  `StowPlaceCommand::ResetStays` (`vr::StowGuideCommand` maps the guide's Reset to it;
  the settings' window keeps its Reset that saves and ends). **Esc** puts the ring back
  where the page found it and ends the guide, which stays on (`FirstStartSwitchesOff`).
- Which page drives which placing: `ui::FirstStartPlacingOf` (the circles on Places, the
  ring on Stow); the host sends the page's Start, Keep, Reset and Cancel through it
  (`LeaveWeaponPlaces`, game/NativeMenuPrototype.cpp).
- Tests: native_settings_test (the order, the buttons, the text, the window, the
  placing), stow_test (the guide's Reset: the default, still up, taken again, Cancel back
  to where the page found it, Done keeps the default; strays do nothing). Harness:
  `stow-guide.txt` (drag, Reset, drag, Done, eye pictures with the head pitched down),
  `stow-guide-esc.txt` (drag, Esc: cancelled, nothing kept, the guide ends);
  `first-start.txt`, `weapon-places.txt` and `newgame-after-guide.txt` step the new page.
- Headset: confirmed by Nadi on Windows, 2026-11-14 (the page works as built).

## Nadi's Windows headset, 2026-11-13

- **Confirmed in the headset:** the intro movie (the Emperor) plays at the new game, and
  the hands are there in the prison right after the race menu's Done - the two items of
  2026-11-10 above, fixed 2026-11-11, now seen working on Windows.
- **Fixed: the chargen view's jump in front of the face not always right at a new game**
  (Nadi: "der Sprung nach vorne vor das Gesicht stimmt bei New Game nicht immer";
  2026-11-13 on Windows: "zu seitlich, musste nach rechts gucken"). The logs of 2026-11-11
  14:28 and 2026-11-12 22:32 / 22:48 each said "the eyes 63.0 units (0.90 m) in front of the
  face, facing it" - the base was right. **Cause (derived from the code and that log, not
  measured in the headset):** the menu frame puts the head on the base (`PlaceMenuCamera`:
  base * head, base + base * offset), and the head's turn and place are measured from the
  last recentre. A player stood turned from that recentre - in the room, the normal case -
  saw the character that far off to the side. The log of 2026-11-13 10:23 shows it: the
  first walk afterwards went 153 degrees from the recentre with the head the same, and the
  second eye's step at the race menu's first frame lay about 47 degrees off the right axis
  of the view the base alone gives. **Fix:** `[Look] ChargenFacesHead` (default 1, INI
  only): when the menu is first seen (not while the engine's film plays over it) the head's
  heading and its place on the floor are taken, and while the menu stays up the base is
  turned back by that heading and the offset counted from that place - a recentre for this
  menu only (`camera/ChargenView.h` ChargenAlign, `camera/ChargenMenuView.cpp`, out of
  CameraHook.cpp). The log now says the head's turn at the opening and where the face lies
  from the view ("Chargen view: laid before the head as it stands - the head turned ...;
  the face ... degrees from the view", then every 450 frames, six times).
  chargen_view_test: the head 0, +-45, +-90 and 180 degrees turned and tilted 25, stood 14
  units aside - the face at the engine's -20.1 every time; a look round afterwards moves
  the face against it; the menu up keeps the opening's head, closed lets go, reopened
  takes it anew; the film over the menu takes nothing, the first frame after it (played or
  skipped) does; no head or one not a number takes nothing. Harness (Linux, headless,
  2026-11-13, DLL 056fa890): `race-menu-turned-off.txt` (the switch off, the head turned 60
  and 20 cm aside during the load) the face -71.5 degrees from the view, the eye picture
  at "racemenu" without the character - `tools/chargen-face.py` FAIL as meant;
  `race-menu-turned.txt` (on) "the head turned 60.0 degrees ... the face -20.1", -20.1 at
  450, 900 and 1350 frames, the character at the right of the picture - chargen-face.py
  PASS (pictures: OBVR-artifacts/chargen-turned-2026-11-13). Open: the headset.
- **Later, not now: vanilla's intro and tutorial messages in VR.** The new game's messages
  (the prison's tutorial boxes and others) partly no longer match OBVR's controls or
  speak of the mouse ("move the mouse" and the like). Later: a list of every such message
  (the GMSTs and message texts), a VR text for each; how to put them in is open. See also
  3.4, vanilla's prompts in Full VR.
  **Listed 2026-11-13: docs/tutorial-messages-vr.md** - MQ01's PC entries by stage and the
  four loading tips that name a control, read from Oblivion.esm, each with its state in Full
  VR and a proposed text. Stale: the jump token's phrase (still the flick), cards 22 ("press
  A to click" - the laser clicks with the trigger), 26 and 78 ("hold A" - the drop is a
  press), the vanilla lockpick card 70 (left mouse button) and the bars "at the bottom left"
  (72-74). Recommended: extend ui/ControlPhrase.h and ui/TutorialCards.h (small); not built.

## The comfort page grows by three (2026-11-14)

Nadi, 2026-11-14: "komfort ja" - the comfort page (`ui::SettingShownIn`, the guide's last
page) also offers, after the stick turn speed and before the start guide's switch:
**Walk direction** (Full VR), the teleport's **Instant** (Full VR, teleport on) with the
glide's **Vignette** only while it glides, and **No knockdown** (both modes - the game's
throw through the air). The two causes of sickness (moving, and the camera thrown by
the game) and the one steering choice every new player feels at once. No stagger, no hit
blur, analog walk and crouch to sneak stay in the menu: the page would grow a third page.
native_settings_test covers every combination (64: the snap's three, the mode, the
teleport, its fade) and the paging. Open: the headset.

### Hints in play: the harness, 2026-11-14 (DLL 25c223e1, Linux, headless)

- The first run of hint-shield-bash showed nothing: the moment read the player's own
  combat state, which stays "not fighting" for a player under tgm who has not struck
  (as the yield found, 2026-10-12). The fight is now the yield's: the player's state or
  the combat ring counting someone fighting the player.
- hint-shield-bash PASS (the sign over the left hand, picture readable; the bash learns
  it); its run before showed "Draw your weapon" first, as built - the scenario now has
  Holsters=0. hint-eat PASS (the sign over the right hand, the meal learns it, the stow
  learns its own). hint-door PASS after the hand rests at the handle 2.5 s (the sign
  and its picture; the grip learns it). Not run: the kick's sign, the stow's sign.
- A line "Hints: at the mark - wanted .." at every harness mark says which moments held.

## A welcome first, and the tutorial's place last (2026-11-15)

Nadi's picture of the first start: the intro films, then OBVR's dome with the guide in it and
the music at once, clicked through, "(später dann an der Stelle evtl ein VR tutorial). und wenn
fertig klick: und hauptmenü wie erwartet"; "der platzhalter geht jz momentan einfach ins
hauptmenü".

**Built** (`ui/FirstStart.h`, `game/NativeMenuPrototype.cpp`; native_settings_test):
- **Welcome** (`FirstStartPage::Welcome`), the chain's first page in front of the main menu: a
  title, three sentences on what the guide does and how long it takes, one button "Let's go"
  to the choice. Beside the text OBVR's mark (Nadi, 2026-11-15: "bau ein logo";
  `tools/make-controls-drawings.py` logo(), `Menus\OBVR\logo.dds`, `FirstStartText::logo`): a
  headset seen from the front inside a seal's double ring, the sun coming up over hills in its
  visor, "OBVR" and "Oblivion in VR" under it - our own drawing in the card's manner, none of the
  game's artwork (`obvr.png`, the Nexus page's meme with the game's pictures, is not for a
  menu). Seen in the harness' window picture 2026-11-15 14:22 (guide-stage-default, the mark at
  the right of the text, one button); the self-check and the doctor count 41 files with it (42 since the main menu's VR button, 2026-11-16). Esc on it ends the chain with the guide
  left on, as on every page before the comfort page.
- **The tutorial's place** (`FirstStartPage::Tutorial`): when the guide's comfort page is closed
  (`FinishSettings`, `ui::FirstStartAfterComfort`) one more page comes up in the dome - "You are
  set up": a tutorial in VR will start from here in a later version - with one button "Main
  menu". Then the chain is over (`ui::FirstStartEnds`), the dome fades out and the main menu
  shows. The guide has switched itself off at the comfort page already.
- **A hand script's chain in the world** (the scenarios' `action first-start`) has neither page:
  it begins with the choice and ends with the comfort page, as before
  (`ui::FirstStartFirstPage`, `FirstStartAfterComfort`).
- **The music at once** (same day, above): the delay 0, the fade 1 s, the file
  `Data\Music\OBVR\onboarding.mp3`.
- Harness 2026-11-15 (Linux, headless, DLL dd014a4e): newgame-after-guide.txt PASS - the welcome,
  the choice, the pose, the places, the stow spot, the hand, the sprint, the card, the comfort
  page closed by Esc, "first start - You are set up", "the guide is over, the main menu next",
  then the dome faded out, the main menu shown, the track closed, and New clicked.
  guide-stage-default.txt PASS (the welcome, then Vanilla VR), first-start.txt PASS (in the
  world, unchanged). Open: the headset.

**Built 2026-11-15 (Nadi's go the same evening, ahead of his Windows test):** the dome made
from the player's own game files - the sky's stars and nebula, Masser and Secunda, Cyrodiil's
map on the floor - on the player's machine, nothing shipped, with the dark dome at 8192 as the
fallback and `[Render] LoadingStageLook` to pick between them. docs/next-up.md, "Built
2026-11-15: two domes at 8192", has the whole of it and what the headset still has to show.

**Open: the guide's music for everyone.** The track is not shipped (the README names it). Nadi,
2026-11-15: stream it into OBVR from its YouTube page instead (yt-dlp, the stretch 18:44 to
21:45 as mp3). Not built: there is no stream to play - YouTube gives no plain audio address,
yt-dlp resolves and downloads, and the game's DirectShow graph plays a file; so it is a download
by a tool OBVR would have to ship or ask for (yt-dlp and ffmpeg), from a page whose terms do not
allow downloading and whose author frees the track for live streams only. Ways: the author's
written leave to ship the stretch (then it goes into the package); a freely licensed track
shipped; or a script the player runs himself. Nadi's decision.

**Decided 2026-11-15 (Nadi: "M1 done go M4"):** the author has been asked for his leave to ship the
stretch (Nadi's message, the answer open). Until it comes nothing changes: the track plays on
Nadi's own install from his file, other players hear the game's quiet track and find the link
in the README. With the leave in hand the stretch goes into the package (small: the file under
assets/music out of .gitignore, the package's file list and the self-check's count).

## The guide without the main menu's map round it (2026-11-15)

Nadi, on the welcome page's picture: "kriegen wir das auch ohne das hauptmenü im hintergrund
hin?" **Found** (`[Debug] LayerDump`, main-menu-eyes.txt with it, 14:31): the menus' layer -
what the headset shows on the menu's quad in the dome - held the guide's window *and* the main
menu's map with its leather bars; only the menu's buttons and title were gone
(`SetMainMenuShown`). The eyes themselves were black, as CinemaOverlay makes them. This was the
open point "whether the main menu's picture is all under the hidden tile": it is not - the map
is the film "Map loop.bik", and the interface pass 0x0057F170 draws the current film at its
start while the film's record (0x00B33428) has state 2 at +0x20 (read in the pseudocode).

**Built** (`game/EngineMovie.h` MenuFilmState and HoldMenuFilm, `render/StageHost.cpp`;
guide_stage_test): while the guide's stage is up the record's state is held at 0 - "no film"
to the pass, and to the engine's other readers of it (0x005ADBC0, 0x005B5870, 0x006AE860, read)
the same as 2 - asked every frame, since the map loop begins only after the menu's intro film;
when the stage ends, a record OBVR held goes back to 2. A film that is the picture (any other
state) is not touched. The film does not run on meanwhile; it goes on where it stopped.
- Harness 2026-11-15 (Linux, headless, DLL d1127712): the layer at the guide's first page holds
  the window alone (14:35, picture OBVR-Layer-guide.png); guide-stage-default.txt PASS with "the
  main menu's film held off the picture behind the guide" and, after "the main menu shown again",
  "the main menu's film drawn again" - the game window then shows the map and the buttons;
  newgame-after-guide.txt PASS (New clicked after the guide). Open: the headset.

**The mark on the welcome page** is the first version again (Nadi, 2026-11-15: "ok logo wieder
zurück!"): in gold, with the letters, beside the text; the watermark try is reverted.

**The welcome page pared down (Nadi, 2026-11-15, the same afternoon):** "entferne den text auf der
Welcome seite sodass nur: Welcome to Oblivion VR. das Logo darunter mit OBVR ___ Oblivion VR und
Lets go steht." No text any more: the title, the mark under it in the window's middle (272
pixels), its letters "OBVR" over a rule over "Oblivion VR" (not "Oblivion in VR"), and the one
button. Seen in the layer dump of 14:41. Centred as well (Nadi: "und das welcome onboarding menü zentriert im
bild"): the title, the mark, the button and its label round the window's middle through the
window's `user5` (OBVR_AdjustHands.xml; 0 on every other page); the unused second button lies
under the first then and is no target (set by OBVR). The first try moved it out of the window
(x 2260) - and after the guide the main menu took no click, the laser's cursor at 0,0
(newgame-after-guide.txt FAIL 14:45 and 14:48, commit 014864d; PASS again 14:52 with the button
left in the window). The window itself stands in the middle of the menus' picture; the layer's dump
shows it up and to the left only because the dump holds the whole texture, of which the menus
use the upper left quarter. Layer dump of 14:44; newgame-after-guide.txt and first-start.txt PASS
after it.

## The title music before and after the guide (2026-11-15, Nadi's Windows headset)

Nadi, after a short Windows run with DLL fd92a9e0: the guide works; "einziger bug: der hauptmenü
sound kommt kurz mit einem schlag laut (der track ist ja so) und dann auf einmal unser sound und
welcome menu. dann beim abschließen setzt das hauptmenü stück fort und fängt nicht von vorne an."

- **Cause (read in the code):** the title track starts with the main menu; the stage held it
  silent only from the moment the guide's stage went up, some frames later - its first bars were
  heard. And silent is not stopped: the game's player kept playing, so at the end the track was
  wherever it had got to.
- **Built** (`ui::StepGuideAhead`, `game::RestartTitleMusic`, `render/StageHost.cpp`;
  guide_stage_test): while a guide is due this start (the dome, the start guide and Full VR on)
  the title music is held silent every frame from the game's start on, until the stage is up and
  holds it itself; a guide that does not come within six seconds of the main menu gives the
  music back. At the guide's end the title track is put back to its beginning (the graph's
  IMediaPosition, asked of the player's IBasicAudio) before its volume is given back.
- Harness 2026-11-15 (Linux, headless, DLL ebf84ab2): guide-stage-default.txt PASS - "the title
  music held silent ahead of the guide" 100 log lines before "the first start's guide begins",
  and at the end "the title music given back its volume, from its beginning";
  newgame-after-guide.txt PASS. Not heard by the harness: whether a first frame of the track
  still slips through between the game opening it and OBVR's next Present, and how the restart
  sounds. Open: the headset.

## The dome shown by holding the scene (2026-11-15)

Nadi, after the guide on Linux: "fügen wir den dome nun auch hinzu wenn das onboarding kommt".
**Read in the logs:** a second after `FadeGrid(1 s, in)` the compositor's grid alpha was 0.00 with
the Beyond on Linux (16:05), 0.49 with the Dream Air on Windows the same day (1.00 on 2026-11-10
to 11-13), 0.27 to 0.31 in the headless harness: while OBVR keeps submitting the eyes (the guide's
frames are CinemaOverlay, black eyes) the grid - and OBVR's dome in its place - does not come, or
only partly. openvr.h names the reliable way: `ClearLastSubmittedFrame` - "This will cause the
compositor to show the grid until Submit is called again" (read with deepwiki ValveSoftware/openvr).

**Built** (`vr::OpenVRBackend::HoldScene`, `ui::GuideHoldsScene`, `render/StageHost.cpp`;
guide_stage_test): while the guide's stage is up the eyes are not submitted - SubmitEye answers
"none" without calling the compositor - and the moment the hold begins the last frame is cleared.
The overlays (the guide's window on the menu quad, the laser) are submitted as before. FadeGrid
stays as it was. When the stage ends the eyes are submitted again.
- Harness 2026-11-15 (Linux, headless, DLL 196bfe82): the grid's alpha a second after the fade
  1.00 (0.27 before), 0.00 after the fade out; guide-stage-default.txt and newgame-after-guide.txt
  PASS (WaitGetPoses kept answering 0, the laser's clicks on the main menu after the guide).
  Open: the headset - the dome itself, and that the window stays on it.

## The guide's words revised (Nadi, 2026-11-15)

Nadi went through every text of the guide and gave the changes:
- **Welcome:** no title, the mark alone, the button "Start" (was "Welcome to Oblivion VR" and
  "Let's go"). The log names the page "Welcome (the mark alone)".
- **How do you want to play?** shorter, for both ways: "Full VR: stand and play with the motion
  controllers - your own hands, weapons drawn from your body. A short T-pose comes next. Vanilla
  VR: play with a gamepad or mouse and keyboard and look around with the headset. You can change
  this at any time."
- **Vanilla VR:** only about Vanilla VR: "Like vanilla, you play with a gamepad or mouse and
  keyboard and look around with the headset. You can change this at any time."
- **Your weapon places / Your stow spot:** "Done keeps ..., Reset puts ... back." removed.
- **Which hand:** "Right or left. Can be changed at any time."
- **How do you sprint?** "The left stick makes you walk, pushed to the end makes you run. Swing
  arms: swing both arms forward and back like running in real life, and you sprint in the game.
  Left stick: push the stick in to sprint. You can change this later at any point."
- **The controls card, page 1:** "A: open doors, talk, use." ("- it never picks up" removed).
- Unchanged: Measure your body, the controls card's other lines, the comfort page, You are set up.

Tests and the three scenarios that name the welcome changed with it; 269/269, guide-stage,
guide-stage-default, newgame-after-guide and first-start-seated PASS (DLL 3de5433f).


## Settings names and help lines on the comfort page (Nadi, 2026-11-15)

"hilfezeile zu technisch. doppelte namen entfernen." Both pairs were two different settings with
one name: the snap turn's vignette and the teleport glide's. Now: "Instant teleport" and
"Teleport vignette" (were "Instant" and "Vignette"), "Snap vignette radius" and "Snap vignette
darkness" (were "Vignette radius" and "Vignette darkness"); "Stick turn speed" says "How fast the
right stick turns you" (was "... mouse pixels a frame at full deflection"). The rule from here on
is in AGENTS.md, "Words the player reads": no technical words in anything a player reads.
first-start.txt PASS with the rows "... Instant teleport, Teleport vignette".

## Line breaks that read well (Nadi, 2026-11-15)

"ausserdem achte auch anprechendere zeilenumbrüche und so was in allen menüs und onboardings."
The guide's pages left their lines to the window's wrap, which broke them anywhere ("your / own
hands", "Left / stick", "For / now"). Now a page's words carry their own breaks - one thought a
line, a blank line between the parts - as `\n` in the C++ string; NativeTextCommand hands it to
the menu as xOBSE's `%r` (read with deepwiki llde/xOBSE: expanded in SetMenuStringValue's
string before the value is set). The longer pages' words are in `ui::FirstStartMessage`. Checked
in the layer's dump of every page (guide-pages walk, 2026-11-15 17:0x; picture
artifacts/obvr-guide-pages-preview.png); the rule for every menu is in AGENTS.md, "Words the
player reads". Not yet gone through: the settings' help lines, the hints, the notices.

## The guide's words made human (Nadi, 2026-11-15: "go, alles so übernehmen")

Nadi: "man merkt bei obvr generell zusehr das alles AI generated ist". The proposal he took
whole: second person and contractions, shorter, the settings reminder only where it helps, a
touch of Oblivion ("Your pack", "Which is your sword hand?"), no colon-and-dash structure in
running text. New titles: Strike a T-pose (Once more after a refusal), Where your weapons hang,
Your pack, Which is your sword hand?, You're ready; the card's pages Your weapon hand, Drawing
weapons, Your other hand, Grabbing things, Both hands; every card line, the comfort rows' help
lines and the settings' header line ("Tap a name for help. Use - and + to change it.") rewritten
the same way; the measurement's refusal says "Didn't quite catch that. Arms a bit straighter,
then pull both triggers again." (the reason stays in the log). Two layout fixes after the layer's
dump: the choice page's last line no longer touches the buttons, the last page's button says
"Main menu" ("To the main menu" wrapped in the button). Picture of every page:
artifacts/obvr-guide-pages-preview.png. Tests and the scenarios naming the titles changed with
it; 269/269; first-start, first-start-seated, tpose, stow-guide, weapon-places,
newgame-after-guide, controls-card (also left-handed and Index) PASS.

## All menu texts made human (2026-11-15)

The rest of what a player reads, rewritten from the review proposal Nadi approved
(artifacts/text-review/proposal.md) with one change of his: the place on the body where letting
go of an item puts it into the inventory is the **inventory ring** everywhere - no "pack", "pack
ring", "stow ring" or "stow spot" as a name (running text may still say "the gold ring" for what
the player sees). The guide's Stow page is now "Your inventory ring" and says the item goes into
your inventory; the settings rows are Inventory ring, Place the inventory ring, Show the
inventory ring; the hint says "Let go of it in the inventory ring"; the settings' window for it
uses the guide page's words. The rings are the hotkey ring, the spell ring and the inventory ring.

What changed, by area:
- Settings rows: 554 texts of 368 rows (193 labels, 361 help lines; the 33 HUD element rows by
  their shared macro). tests/settings-rows.txt regenerated, only the label and help columns differ.
- The settings window's own words and the MenuQue menus: 22 (mode names Full VR / Vanilla VR,
  Reset, Undo, "Undo <label> (<value>)", the play-style window, the self-check and update windows).
- The fallback walkthrough without MenuQue: 23.
- Hints and holster signs: 13 (hint titles kept, the scenarios count them).
- Tutorial cards: 8; control phrases: 5.
- Placing windows and the adjust-hands window outside the guide: 13, with line breaks.
- Update notice, self-check panel, the controls card's rebound-button words: 32 ("Hint:" became
  "Tip:"; the card's short words live in ui::ControlsCardPhrase so the log's words stay).

The run phrase: the code proved the proposal's finding. With the default analog walk
(vr/AnalogWalkLogic.h: enabled, run from 0.65, sprint 1.4) the push runs and the stick clicked in
is the sprint (vr/HandMode.cpp, the analog walk block), so "hold the left stick clicked in"
named the sprint. ui::ControlPhrase now takes whether the push runs ([Hands] AnalogWalk, passed
by game/ControlWords.h) and says "push the left stick all the way" then, the old words without
it. Not followed by the phrase (unchanged): [Hands] RunToggle and ArmSwingRun.

Left out on purpose: the rows the proposal marks as unsure ("Tooltips above target name" kept,
"Hand panel height / back / tilt" labels kept), choice values (they are OBVR.ini's words; no
display table built), log lines, docs other than this note, README's controls table, and the
approved guide and controls card texts except the inventory ring.

Built and unit-tested (269/269, also under the sanitizers); not yet seen in the game, and the
hand-script scenarios whose expectations changed (first-start, stow-guide, stow-guide-esc,
weapon-places, holster-shield-torch, tutorial-cards) have not been run.

## The breath and the heavy weapon on the comfort page (Nadi, 2026-11-15)

"sollten wir den exhaustion verlangsamung der waffe und das breathing auch im onboarding als
toggle ausschaltbar machen? mittlerweile nervt mich das geatme" - option B taken: the comfort
page offers "Exhaustion sounds" (both modes) and a new switch "Heavy weapon when tired" (Full
VR, `[Look] ExhaustionHeavyWeapon`, default on; off, the weapon keeps its weight while the rest
of the exhaustion runs - `game::StepExhaustion` gives the factor 1), both only while the
exhaustion itself is on (off, the game's collapse comes instead and neither means anything). The
weight's slider stays in the settings for its strength. exhaustion_test (the factor 1 with the
switch off, 2.5 with it on) and native_settings_test (128 combinations of the page) cover it;
269/269. Built, not deployed: Nadi is testing.

**The last page promises nothing (Nadi, 2026-11-15):** "entferne noch den hint das irgendwann mal ein vr
tutorial kommt um keine erwartungen zu wecken" - the line "One day a proper VR tutorial will live here." is
gone; the page is still where a tutorial could be offered later (FirstStartPage::Tutorial).

**The scene hold off (2026-11-15):** with `HoldScene` (c86c4bf) the game vanished on Linux with the Beyond
some ten seconds after the welcome page, twice (19:05, 19:06; no crash report, SteamVR only saw the process
leave). Behind `[Onboarding] GuideHoldsScene`, off, since 91a4864; Nadi: "geht wieder". The cause is open; on
Linux the dome is faint again without it.

**The button moved into the Options menu: "VR Options" and "VR Guide" (built and run 2026-11-16, Linux,
headless; replaces the main menu's "VR" button below).** Nadi: the VR button goes under Options as "VR
Guide", under it "VR Options" which opens OBVR's menu; in the game, under the pause menu's Options, "VR
Options". Built with VR Options first (the same place in both menus; the guide's row falls away in the game
without a gap; Nadi: "go so wie du vorschlägst").
- *One menu for both.* The main menu and the pause menu open the same OptionsMenu (options_menu.xml, type
  1015, its user0 true from the pause menu): Return, Gameplay, Video, Audio, Controls in the frame
  "options_background", and an Xbox "Downloads" row that is hidden on the PC.
- *The buttons.* No Options menu is shipped or replaced: `assets/menus/prefabs/OBVR/options_vr_options.xml`
  ("obvr_vr_options", id 9701) and `options_vr_guide.xml` ("obvr_vr_guide", id 9702), the game's own
  button_floating_2, are put into the frame of each Options menu the game builds by MenuQue's InsertXML (the
  console's `@` for the pipe; `game/OptionsMenuVr.cpp`). VR Guide only when the main menu is loaded under the
  Options menu. `[Onboarding] OptionsButtons` (default 1) switches both; `[Onboarding] MainMenuButton` and
  main_vr_button.xml are gone. A menu without the game's frame and six rows by name (a UI mod's) gets
  nothing, said once in the log; both sticks and Insert open OBVR's menu as ever.
- *The clicks* (`ui/OptionsVrButtons.h`, options_vr_buttons_test): noticed by OBVR as the main menu's were (a
  hand's click with the cursor on the tile, or the mouse going down on the active tile), only with the
  Options menu on top and OBVR's menu shut. VR Options asks for OBVR's menu (`RequestNativeSettingsToggle`),
  which opens over the Options menu and leaves it in front when closed. VR Guide asks for the guide and
  presses the Options menu's own Return (`ClickMenuButton "options_background\options_return_button" 1015`):
  the chain begins once the main menu is in front again.
- *LINK.* LINK.esp (in both installs) makes the Downloads row its "Mods" button (LINKMenu1015Update
  MenuFunctionScript: the row's visible written as a string, the frame's user1 and height +64 each, the six
  rows' y -64). Measured without OBVR (options-vr-sweep 20261116-152405): the frame's user1 424, the rows from
  20, Mods at 340 - 20 under the last row, as the game's own 360 and five rows have it. So OBVR never takes
  the Downloads row and stands under the last row that is shown.
- *The layout is OBVR's own, each frame the menu is up.* The first build let the prefab read its place from
  Controls and Downloads and only made the frame taller (options-vr-sweep 20261116-152150): VR Options lay on
  "Mods" - an `onlyif` on a visible trait that LINK wrote as a string does not count - and all rows stood 64
  too low: the menu's own formula puts the rows' top at (the frame's height - five rows) / 2, so every 64 of
  height push them 32 down, and a y another mod wrote is lost when the formula runs again. Now
  `ui::LayOutOptionsRows` places the rows that are shown, in the game's order, so that they end as far over
  the frame's end as before OBVR's came (`OptionsBottomMargin`, read at the insert: 20 from the main menu, 18
  from the pause menu), OBVR's after them; only values that are off are written (the game's rows' y through
  the engine's Tile::UpdateFloat, as LINK writes them). The frame's user1 is raised by OBVR's rows, added to
  what stands, so LINK's own 64 stay whichever of the two comes first.
- **Harness (Linux, headless, LINK loaded, DLL md5 6da2bcd1):**
  - options-vr-buttons-main/20261116-153001 PASS: both asked in, "the rows put in place (1. time ...)" once,
    "the rows as they stand - the frame's y 172, height 616 (its user1 552); options_return_button y 20 ...,
    options_downloads_button y 340 shown, obvr_vr_options y 404 shown, obvr_vr_guide y 468 shown"; the laser
    on "obvr_vr_options", its click opens OBVR's menu over the Options menu, Insert closes it; the laser on
    "obvr_vr_guide", its click, the Return, "the first start's guide begins".
  - options-vr-buttons-game/20261116-153240 PASS: Esc, the pause menu's Options (ClickMenuButton "#7" 1013),
    VR Options alone ("obvr_vr_guide none"), the frame 464 -> 528, the rows from 62, Mods at 382, VR Options
    at 446; its click opens OBVR's menu, Insert closes it, the top menu Options again.
  - The 2D layers at "options_open" of both runs: the rows one under the other, none on another, Mods
    between Controls and VR Options.
- **Harness without LINK (Linux, headless, LINK.esp out of Plugins.txt for the two runs and back after,
  md5 36e2eaf8 before and after; DLL md5 2ab65b6c, the same source built again):**
  - options-vr-buttons-main/20261116-155158 PASS: "the frame 488 high, the game's rows from 20 (they stood
    from 84), no sixth row, VR Options at 340, VR Guide under it"; "options_downloads_button y 404 hidden";
    both clicks as with LINK.
  - options-vr-buttons-game/20261116-155255 PASS: the frame 400 -> 464, the rows from 62 (they stood from
    94), VR Options at 382.
  - With LINK again at the scenario's one pitch for both cases (46: the row is 45 with LINK and 47 without,
    four degrees high): options-vr-buttons-game/20261116-155408 PASS.
- **Owed:** the headset; Windows; a UI mod's Options menu (DarkUI, NorthernUI: expected "is not laid out as
  the game's" when the names differ, not seen). The live INIs still hold the dead line `MainMenuButton=1`.

**A "VR" button in the main menu, and how the settings open (built 2026-11-16; the button itself replaced the same day, above).** Nadi:
"wir brauchen einen VR button im hauptmenü. dieser startet dann einfach wieder das onboarding. außerdem im
onboarding irgendwo erwähnen wie man die OBVR settings öffnet. Sowohl Insert / Entf als auch Both stick
presses".
- *The button.* No main menu file is shipped or replaced (a UI mod's would lose its own): one tile,
  `assets/menus/prefabs/OBVR/main_vr_button.xml` ("obvr_vr", id 9700; first bottom right with the menu's
  user3 as its alpha, never seen - since the runs below the game's own button in the buttons' row), is put
  into each main menu the game builds by MenuQue's
  InsertXML at the menu's top level (cs.uesp.net/wiki/InsertXML; `game/MainMenuVr.cpp`, one console line
  through `RequestConsoleLine`). The engine hands a click to the main menu, which knows no such id; OBVR
  notices it itself (`ui/MainMenuButton.h`, main_menu_button_test): a hand's click with the cursor on the
  tile (`camera/hands/ControlsOut.cpp`), or the mouse's button going down while the tile is the interface's
  active one and the game is in front. `[Onboarding] MainMenuButton` (default 1) switches it; the self-check
  and the doctor list the file.
- *The guide again.* `ui::FirstStartDue` has an `asked`: with it the chain begins whatever the mode, the
  setting and this start's run say - still only with the main menu in front, no generic menu and nothing
  busy (native_settings_test, all 128 combinations). In Vanilla VR the chain begins at the welcome and its
  mode page as at a first start.
- *How the settings open,* on the last page ("You're ready"), by the way of playing (`ui::FirstStartReadyMessage`,
  the host hands `fullVrMode` to `FirstStartTextFor`; native_settings_test). Nadi, 2026-11-16: "entf + insert
  soll im onboarding nur erwähnt werden wenn vanilla vr an ist, wenn full vr dann soll both stick press
  erwähnt werden", and "entferne auch: Hit New in the main menu to start your adventure".
  Full VR: "That's it, you're good to go. / Want to change something later? / Click both sticks at once to
  open the OBVR settings. / VR in the main menu runs this guide again."
  Vanilla VR: the same with "Press Insert to open the OBVR settings. / Delete recenters your view." in place
  of the sticks' line. Read in the code: Delete (Entf) is the recenter key (`recenterKey` 0x2E), the settings'
  only key is Insert (`[SettingsMenu] Key` 0x2D) - the page names each for what it does, and no key's function
  changed ("functionalität bleibt wie bisher").
- **Run in the game 2026-11-16 (Linux, headless, `--save continue`; MenuQue v16b):**
  - *The insert works as written:* "Console line: "InsertXML "OBVR\main_vr_button.xml" 1044" run (the
    interface answered 1)" in every run of the day, no "script line failed".
  - *The first button was never seen (main-menu-vr-button/20261116-100010, the picture at "menu").* Its
    label took its alpha from the main menu's `user3`. Read in the vanilla `menus\options\main_menu.xml`
    (Oblivion - Misc.bsa): `user3` is "alpha value set by animated code" on the film logo laid over the real
    one, `user4` "how many seconds to fade from bink logo to real logo" - it is 0 once the menu stands, and
    the menu's own buttons do not use it (they are shown by `user1` alone). With the alpha 255 the button
    showed (vr-button-sweep/20261116-100344): a small pale "VR" (font 3) on the dark border under the map,
    and no highlight to be seen under the laser - the highlight is a parchment strip, made for the map.
  - *So the button is one of the menu's own now:* the game's `button_floating.xml` prefab (what Continue,
    New, Load ... are made of - its font, its brown, its highlight), in their row (y 725) at the right
    (screen width - 170: clear of Exit at 4:3 and wider), shown by `user1`. A UI mod that restyles
    `button_floating.xml` restyles it with the rest; a mod whose row is not at y 725 is not looked at.
    Seen in main-menu-vr-button/20261116-100657: "VR" reads as the other buttons do, and under the laser
    the 2D layer shows the game's own highlight behind it (OBVR-Layer-vr_lit.png).
  - *The click:* the laser at pitch 41, yaw -37 from the scenario's hand - "the laser hits the menu quad at
    pixel 2400,1135; the game's menu under the cursor Main, the tile "obvr_vr"", "click sent to the Main
    menu ... the tile under it "obvr_vr"", "Main menu: the VR button clicked (a hand's click) - the start
    guide asked for again", "Native menus: the first start's guide begins", the welcome page in the picture
    at "guide". The main menu did nothing with id 9700 (no crash, no other menu). PASS.
  - Again on the day's last build (DLL d9c1b16a): main-menu-vr-button/20261116-103358 PASS.
  - **Open:** the headset; the mouse's click (unit-tested only - the harness clicks by hand); a UI mod's
    main menu (none installed on the test machine).

**The scene hold on again (2026-11-15, evening):** Nadi: the new dome did not come during the onboarding, and it
is to be seen through the whole of it. Read in guide-stage 20261115-203920 and -204034 (hold off): "a second
after the dome faded in the compositor's grid alpha is 0.00" - the dome not shown; in 20261115-163216 (hold on)
1.00. Of the options (find the Linux crash first, hold on only off Proton, hold on everywhere, OBVR drawing the
dome into the eyes itself) Nadi chose the hold on everywhere: `GuideHoldsScene` defaults to 1 again (ConfigTest
`TestGuideHoldsSceneSetting`). **Still open:** the Linux vanishing of 19:05/19:06 (log `player-OBVR.log.prev`
of 20261115-203920 ends mid laser hover over the welcome page, grid alpha 0.64, no exception line); should it
come back, SteamVR's vrserver/vrcompositor logs of that minute are the next evidence. Windows has never run
the hold in a headset.
- Harness 2026-11-15 21:34 (Windows, guide-stage-default, DLL 13f0f38c, artifacts/hand-script/
  guide-stage-default/20261115-213443): PASS - the sky dome asked for before the guide ("compositor answered
  0"), "the eyes held off the headset" with the fade in, "a second after the dome faded in the compositor's
  grid alpha is 1.00", the eyes submitted again only after "the dome faded out - the guide is over". The
  scenario now expects the hold and rejects an alpha of 0.00 after the fade in. Open: the headset (Windows
  and Linux), and a Linux run of this DLL.
- Harness 2026-11-16 08:27 (Windows, guide-stage-default, the dark dome the default at 12288, DLL c3f4e9bf,
  20261116-082715): PASS - "loading_dome_dark.obj, compositor answered 0", no sky made, the eyes held from the
  fade in to the guide's end, grid alpha 1.00.
- Harness 2026-11-16 10:15 (Linux, headless, guide-stage-default, the dark dome at 12288, the hold on, DLL
  d9c1b16a, guide-stage-default/20261116-101537): PASS - "the eyes held off the headset, the last frame
  cleared - the compositor shows the dome", "a second after the dome faded in the compositor's grid alpha
  is 1.00", "the dome faded out - the guide is over". The game did not vanish, here or in the day's other
  runs that begin the guide (main-menu-vr-button). The vanishing of 2026-11-15 19:05/19:06 stays
  unexplained and unrepeated; that run had the headset on, these did not.
- **The vanishing came back (Linux, the Beyond on, measured 2026-11-16 13:41; the cause open).** Nadi clicked
  the main menu's VR button (DLL md5 3d318c10, the hold on by default), saw the welcome page, and the game was
  gone. `OBVR.log` ends a few frames after "the eyes held off the headset, the last frame cleared" and "the soft
  track started" (the track's volume at 0.021 of 0.25, so about a tenth of a second in), with no quit line;
  `CrashLogger.log` holds its header only (no exception reached it); `coredumpctl` has no dump of the game;
  vrserver and vrcompositor only saw the process leave ("Unable to read message from socket: 17", 13:41:29.274,
  the game's last log write 13:41:29.230) and had sent it nothing. The third time in three starts of the guide
  with the hold and the headset on Linux (2026-11-15 19:05:26 and 19:06:23, there some ten seconds in).
  - Measured beside it: the NVIDIA kernel driver (620.70.06, open module) refused GPU channel allocations to
    one client at 13:41:29.159, .218 and .226 ("KernelChannelGroupApi alloc RPC to vGpu Host failed", "State
    in use [NV_ERR_STATE_IN_USE]", `journalctl -k`). The driver had been in that state since 13:34, when the
    card's memory ran full (LM Studio held 19.6 GB of 24.6 GB; "Out of memory [NV_ERR_NO_MEMORY]", three starts
    that died in "Initializing Renderer..." with CreateDevice refused 0x8876086A), and went on refusing other
    clients after the game had left (13:41:38). Whether the refused client of 13:41:29 was the game is not
    known. The two vanishings of 2026-11-15 have no driver line in their minutes.
  - Not separable from the logs in hand: the hold itself (three of three with the headset on Linux) or the
    driver's state after the full memory (today only). The next evidence: after a reboot, one start with
    `PROTON_LOG=1 %command%` as the launch option and the VR button clicked - `~/steam-22330.log` then says how
    the process ended (Wine's exception and exit lines), and a clean driver takes the second cause out.
- **Open bug (Linux, measured 2026-11-16): SteamVR's compositor reports an error at every load of the 12288
  dome.** `~/.local/share/Steam/logs/vrcompositor.txt`: "[Error] - Vulkan transfer ring buffer overflow;
  transfer size 301989888!" a second or two after each game's connect - 28 times in 28 starts of the day
  (09:14 to 10:16), and 301989888 is 12288 x 6144 x 4 bytes, the dome's picture. Yesterday's starts with the
  8192 domes (134217728 bytes) left no such line (the same file from 20:25 on, vrcompositor.previous.txt
  before). The compositor went on each time ("Loaded render model: ...loading_dome_dark.obj" in
  vrclient_vrcompositor.txt; the Linux compositor writes no "Loaded image" line for any size, so the log
  does not say whether the picture arrived). **Not known:** whether the dome is drawn with its picture on
  Linux, or bare or black - headless runs read only the grid's alpha, and SteamVR on Linux writes no
  compositor pictures. **Related, read in the same logs:** the compositor's ASSERT of the same family ends
  it ("ASSERT: Vulkan transfer ring buffer overflow; transfer size 2315040!", 2026-11-15 21:15:51, restart
  at 21:16:26; "... 71100!" 21:22:25; and 2026-10-24 before there was any dome) - a small upload meeting a
  full ring. Derived, not measured: a 134 MB or 302 MB dome picture in that ring makes such a meeting
  likelier. **Options (Nadi's decision - the 12288 was his):**
  1. *Look first:* one start on Linux with the headset on and a look at the dome while a save loads. Cost:
     cheap (a minute); it settles whether anything has to be done.
  2. *8192 on Linux only:* the dark dome made at 8192 when the game runs under Wine (ntdll's
     `wine_get_version`), 12288 on Windows. Small: the size is one constant (`kDarkAtlasWidth`), the
     asset would ship twice (+3 MB) or be scaled at start; unit flows for the choice, the harness reads
     the compositor's log for the error line. It gives up the sharper dome on Linux (8192 was "shown
     magnified").
  3. *One size under the ring's limit for both:* not possible to choose well yet - the limit is not
     read (somewhere between 134 MB and 302 MB); a 256 MB guess (11264 x 5632) would need a run each
     side to bracket it. Small-medium.
  Recommended: 1, then 2 if the dome is wrong there - 1 costs a minute and decides whether 2's loss of
  sharpness is paid for anything.
- **Decided and built 2026-11-16 (Nadi: "linux 8000"): option 2.** Under Wine `vr::LoadingStageModel(true)`
  names `OBVR_Stage\loading_dome_dark_8192.obj` (StageHost.cpp asks ntdll for `wine_get_version` once),
  on Windows the 12288 dome as before. The 8192 files are the same painting: `make_loading_stage dark
  assets/stage 8192 9 loading_dome_dark_8192`, and the same tool rebuilt the shipped 12288 files byte for
  byte (.png, .obj, .mtl SHA-256 equal), so the generator is the one the shipped dome came from. The
  package ships both (+3.2 MB); the self-check and both doctors expect both. Unit tests: LoadingStageTest.
  **Not run:** the harness (guide-stage-default.txt now expects either name) and a Linux start that shows
  whether the compositor's line is gone with 8192.

## 2026-11-19: the hand-fit guide in plain steps, a refused fit said

A Quest 2 tester did not understand the "Adjust your hands" window ("isn't worded very well"),
pressed every button and tried to move the hands "as you would a virtual object" - nothing
adjusted. The old words never named the grip, nor said that the controller goes into the frozen
hand. A fit made by dragging (the controller far from the frozen hand) is refused
(`game::FitReadsLikeAHand`, over 0.35 m), and only the log said so. Now
(ui/NativeHandAdjust.h): the guide gives the three steps - squeeze and hold the grip button,
bring the controller into the frozen hand like a sword hilt, let go; a refused fit opens the
guide again with what went wrong (`kHandAdjustRefusedMessage`; `game::NoteHandFitRefused` from
CameraHook's `PinAdjustableHand`), and its Cancel ends the adjusting. native_settings_test.
Not seen in the game; the line breaks are to be checked in the layer's dump. Open: the default
fit on Oculus Touch sits low, in the wrist (the defaults come from Index controllers) - to be
measured from a Quest, not guessed (docs/controls-spec.md, "Beta 9 reports", B).

## 2026-11-21: stuck on the T-pose page (a Pico 4 and a Quest 2) - open bug, cause not found

**Reports (Discord, 2026-11-20).** A Pico 4 tester cannot get past "Strike a T-pose", over PICO
Connect and over Virtual Desktop alike, and asks whether an OBSE plugin conflicts. A Quest 2
tester: "I have to t pose and click the sticks in rather than the triggers". Index (Nadi's
setup) passes the page (Nadi's own logs: "Body measure: ... hands 1.79 m apart").

**What the code needs (read).** `UpdateBodyMeasure` (camera/CameraHook.cpp) counts the pose only
with both triggers over 0.5 on the action path (`frame.*.trigger`, the manifest's
`left_trigger`/`right_trigger`) and the hands in the body's frame as a T (`vr::TPoseMissOf`:
each 0.45 m out, at most 0.35 m ahead or behind, 0.55 m below to 0.10 m above the eyes, level
within 0.20 m), held half a second. The page has no Skip (2026-10-28); Back, Vanilla VR and Esc
leave it. Both sticks clicked and let go open OBVR's menu (vr::StepStickChord, read); another
menu over the guide's window ends the guide ("the page was closed from outside, the guide ends",
read). Derived, not checked in the game: the Quest 2 tester's stick click most likely ended the
guide rather than measuring - whether the guide's window lets the chord through was not read.

**Candidates, none confirmed:**
- *No binding for PICO Connect's controllers.* The manifest ships default bindings for
  `knuckles` and `oculus_touch` only (assets/input/actions.json). PICO Connect reports its
  controllers as their own type, `pico_controller` (pikegadge.com PICO Connect review, updated
  2026-11; a Fallout 4 VR guide quoting PICO Connect's binding file); Virtual Desktop reports
  `oculus_touch`. Without an app binding SteamVR falls back to the driver's compatibility type or
  its automatic rebinding, if the driver offers one (openvr wiki, via DeepWiki) - whether PICO
  Connect does is not known. If none applies, every action is inactive and the triggers read 0.
  It does not explain the Virtual Desktop run, which gets the Touch binding.
- *Inside-out tracking at the T.* Both testers' headsets track the controllers with their own
  cameras; arms straight out to the sides may leave the cameras' view and the runtime then
  carries the controller on its IMU (a guess, not measured; full-body-tracking T-poses on Quest
  usually work, which speaks against it).

**The log now says why (this commit).** While the page waits: `Body measure: waiting - the
triggers right N left N (both over 0.50 count); <what keeps it from a T>; the hands (right,
ahead, up from the eyes, m) ...` on a change (at most one each half second), again every 3 s
while both triggers are held without a T, at most 40 lines a page (`vr::StepTPoseWaitLog`).
body_measure_test (the miss's every branch, the log's pacing, cap and bad frame times; red
first against the old header). The DLL builds; not run in the game or the harness.

**Next:** the testers' OBVR.log. Already in the current log: the controller type, "actions in
use, bound mask" (00 or no trigger bit = the binding), "controller not tracked". With the next
beta: the waiting lines with the hands' places. Then the fix at the cause: a `pico_controller`
binding (its trigger, grip and joystick paths are known from public bindings; its face buttons'
paths are not), or a pose the cameras can see.

## The grip toggle on the combat page (2026-11-25)

Nadi: "setting ins onboarding: man muss die hand nicht dauerhaft mehr an der klinge halten um diese zu
halten, einmal grip reicht zum nehmen und waffe bleibt gehalten, wenn gehalten lässt grip die waffe wieder
los ... default aus." The combat page offers "Toggle grip" first (`[Hands] GripToggle`, off; the settings
under Combat: features): "Grip once to keep your weapon in hand, grip again to let go. Off, hold the
grip". Seven rows now, still one page. What it does: docs/free-weapons-spec.md 5.1.1. native_settings_test
covers the page; the headset is open.

## Take only by hand on the comfort page (2026-11-25)

Nadi, on a Quest 2 tester's report of the same day: the A button on loose items is on by default again, the
player decides, and "die option gehört auch ins onboarding". `[Hands] TakeOnlyByHand` is 0 by default
(vr/Stow.h, controls-spec "A tester's wishes and a bug, 2026-11-25") and the comfort page offers its row
after the tablet's, in Full VR while the inventory ring is on (only by hand needs the ring to take anything):
"Take only by hand - A doesn't pick up loose items anymore. Grab them and stow them instead"
(`ui::SettingShownIn`, kComfortOrder). native_settings_test: the row's place in every combination, not in
Vanilla VR, not with the ring off, off by default. first-start.txt PASS with the build (20261125-145911-slot2);
its log names the page's first seven rows, as before - the new row stands on the page's second screen,
which no run turned to. The headset is open.

## The guide comes back once with a new beta (2026-11-25)

Nadi: "neue rule: bei jeder neuen beta das onboarding für leute wieder anschalten einmalig. wenn
wir dem onboarding neue toggles hinzufügen bei releases auch." Why it is needed: a player who
keeps their OBVR.ini across an update (the beta posts say so) never met the guide's new rows nor a
changed default - the same day `[Hands] TakeOnlyByHand` went to 0 by default, and an INI kept from
beta 9 still says 1; only the comfort page's row puts that choice in front of them. One who unzips
over Data gets the shipped INI with `StartGuide=1`, and loses their own settings with it. The DLL
cannot tell one beta from the next by itself: its version is 0.3.0 for every beta, the tag is only
in the zip's name.

- **Built.** `ui::kGuideRevision` (ui/GuideRevision.h), 2 since this day; revision 1 is the guide
  of beta 9 and before. At the plugin's load, after OBVR.ini is read, `game::ReturnGuideForNewRevision`
  (game/GuideReturn.cpp) reads `[Onboarding] GuideRevision` from the player's OBVR.ini (none: 0);
  a lower one switches the guide on (`StartGuide=1`, saved) and writes this revision. Log:
  "Onboarding: this OBVR's start guide is revision 2, OBVR.ini named 0 - the guide comes back once".
  From there the guide runs as on a first start: through to the comfort page it switches itself
  off; ended by Esc it comes again at the next start, the rule of 2026-11-06. Not in a harness
  run (its INI keeps the guide off; "the start guide's revision neither read nor written").
- **Raised by hand, held by two checks.** Every new beta: `tools/package-release.py --tag -betaN`
  refuses a beta at the revision another beta was packed with (`tools/guide-revision-packed.txt`,
  "1 -beta9"; it notes the new one after packing; `--same-guide` packs a beta that does not bring
  the guide back, on purpose). Every change to the rows the guide's pages offer: native_settings_test
  (`TestGuideRevision`) pins the comfort and combat pages' rows beside revision 2 and fails when they
  change and the revision does not. package_release test, native_settings_test.
- **Not done:** the headset, a harness run of the return (the harness keeps the guide off; the
  decision is a pure function, the rest is two INI writes and a log line).
- **On Nadi's own games:** their OBVR.ini names no revision yet, so the guide comes back once at
  the next start after this deploy.
