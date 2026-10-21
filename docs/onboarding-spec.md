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
4. **Weapon places.** The existing "Fit weapon places" guide (`RequestHolsterFit`). The text already says what to do.
5. **The controls card.** One page, the table of docs/controls-spec.md section 1 reduced to twelve lines, by role ("weapon hand", "other hand") so it reads right for a left-hander. Reopenable from the settings (a Help row "Show the controls").
6. **Comfort.** The view that opens today.

- The chain lives where `g_comfortPending` lives (NativeMenuPrototype.cpp:32, :374-388): one pending step instead of one flag, stepped the same way (`StepComfortPage` becomes a step over an enum).
- The guides in 3 and 4 already hand their finish back (`TakeHandAdjustFinishRequest`); the chain continues on it.
- Nothing in the chain should start a game. All of it runs in the main menu, where the native menus already work; the hand guides need the hands tracked, which they are in the main menu with `ControllerMenus` on.
- **Test:** the step logic as a pure function (`StepFirstStart(pending, genericRoot, mainMenuOnTop, guideFinished)`), every path: skip, finish, the main menu going away mid-chain, a failed save.
- **Built 2026-10-15 (ui/FirstStart.h, the host in NativeMenuPrototype.cpp; the tester: "wir brauchen die erststart kette, bei jedem start whl und pflicht beim new game dungeon").** Four of the six steps, in this order: **Recenter** (the adjust-hands window with its own words; the page sees the recenter - `game::NoteRecentred` from `DoRecenter`, taken by the page - and says "Recentred"; Next or Skip), **Which hand** (Right, Left or Skip; Right and Left write `[Hands] LeftHanded`), **the controls card** (its own window, two pages by role, Next/Back turn, Close goes on) and **Comfort** (the existing view; the "Start guide" switch is on it too, so the chain can be turned off where it ends). At every start with Full VR and `[Onboarding] StartGuide` (default on) in the main menu, once per start, nothing else of OBVR's on the way; choosing Full VR in the walkthrough starts it the same way (the comfort page alone follows only with the guide off). Esc ends the chain; a page whose XML is missing is skipped. Steps 3 and 4 of the proposal (the hands' fit, the weapon places) are not in the chain: the hands' fit offers itself the first time the player stands in the world (`FirstHandFitDue`, as before, now held back while the chain or the card is up), the weapon places stay a settings row. The pure parts (`FirstStartDue`, `FirstStartNext`, `ChooseFirstStart`, `FirstStartTextFor`, `FirstStartWindow`) are covered in native_settings_test; the harness drives the pages with the script actions `menu-primary`, `menu-secondary` and `menu-reset` (`first-start.txt`, begun in the world by the script's `action first-start`), and the runner writes `[Onboarding] StartGuide=0` into every run's OBVR-test.ini unless the scenario says otherwise, so the chain never stands in front of the main menu's console. Harness `first-start.txt` PASS and `controls-card.txt` PASS (Linux, 2026-10-15; five runs to get there: a window of OBVR's must not open before the script runs, Escape over a generic menu opens the pause menu on top instead of closing it, and the chain's place has to be kept across its pages). **Not built: "mandatory in the new game's dungeon"** - it needs the engine to tell a new game from a load (the first cell's form ID, or the game's own new-game path), not read yet; the chain can be skipped everywhere for now.

### 3.3 Hints in the world, the first few times

A hint is one `GuidePanelText` on a `CanvasOverlay` (the holster-fit layer's shape, CameraHook.cpp:1440), hung where the HUD hangs, shown for about four seconds, counted in the INI so it stops after N showings (`[Hints] <name>=<shown>`, N = 3 by default, `[Hints] Enabled=0` turns all off).

| Moment | Hint | Trigger that already exists |
| --- | --- | --- |
| First time a weapon is equipped and sheathed | "Grip at your left hip to draw. Two-handers and staffs: over your right shoulder. The bow: the other hand over your left shoulder." | the holster reach's "ready" state (`vr::StepHolster`) with the weapon sheathed |
| First hostile within reach | "Raise your shield hand to block. Both hands up, open and still: yield." | `CombatRing` / the target marker's hostile |
| First inventory in Full VR | "Trigger clicks. Left A drops what the cursor is on. Hold the right trackpad: hotkeys." | the inventory menu opening with the mode on |
| First teleport aim | "Let go to go. A flick jumps." | `TeleportAim` starting |
| First time the view is off by more than 45 degrees from the body for 10 s | "Both sticks held for three seconds recenter." | the walk-direction heading against the head |
| First death | nothing new; the death view is built | - |

- Hints never pause the game and never take input; they only show.
- The texts are by role, as the controls card.
- **Test:** `hint_test` over a pure `StepHint(count, trigger, dt)`: shows at most N times, never while a menu is up, the counter persists, `Enabled=0` shows nothing.

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
- **Asked 2026-10-21 (Nadi, after the beta 5 round), not built:**
  1. *The turn speed on the comfort page when snap turning is off* ("im onboarding auch eine option
     anbieten beim smooth turn (also snap turn off) die geschwindigkeit zu setzen"). Today the comfort
     view shows Left-handed, Snap turning, the start guide's switch and the snap rows while snap is on
     (`SettingShownIn`, ui/NativeSettings.cpp) and nothing for the smooth turn. The Full VR turn is
     "Stick turn speed" (`[Hands] TurnSpeed`, mouse pixels a frame at full deflection, Hands: moving);
     `[Look] TurnSpeed` is the eased mouse turn of VR View. Cheap: one rule in `SettingShownIn` (the
     Hands row while snap is off, in Full VR), a native_settings_test flow, the first-start chain
     scenario's comfort picture.
  2. *The controls card on more pages* ("die controls cards gerne mehrere statt alles auf einem haufen
     gepresst"): twelve lines on each of two pages today (`ControlsCardFor`, kControlsCardLines). Cheap:
     more pages of fewer lines in `ControlsCardPage` and `ControlsCardFor`, the XML unchanged (its twelve
     line tiles stay, the spare ones blank), the chain's Turn choice walks the pages, native_settings_test
     and the first-start scenario's pictures.
  3. *Drawings on the cards* ("kann man in menuque zeichnungen anzeigen?"): yes. The menu XML's `<image>`
     tile with a `<filename>` (a DDS under Data\Textures, named the way the prefab names vanilla's:
     Menus\...) draws a picture, and OBVR's own windows use one already (menus/prefabs/OBVR/button_highlight.xml, the
     vanilla dialog_selection textures, included by OBVR_Controls.xml and OBVR_Onboarding.xml). MenuQue
     only hosts the window; the tile is the engine's. The cost is the pictures, not the code: our own
     drawings of a Quest and an Index controller with the roles marked (no vendor images - the zip ships
     them), one per card page. Medium, almost all of it art;
     the XML and the asset list are cheap.

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
