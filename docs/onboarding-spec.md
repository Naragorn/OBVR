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

- **Research first:** where the engine substitutes the key name into a message. Candidates: the "%s" in the tutorial quest messages is filled by the control map's key name (UESP: the tutorial messages name the bound key), so the function that turns a control index into a key name is the place. Not found yet; nothing is known about its address.
- Then: with the mode on, answer the controller's name for the control the mode presses (`game::ApplyHandControls` knows which key stands for which role): "Forward" becomes "left stick", "Activate" "A on the weapon hand", "Ready weapon" "reach to your hip".
- The in-menu prompts ("Press E to take") follow from the same place if it is the one.
- **Test:** the name map as a pure function over control index and mode, every control in the table of docs/controls-spec.md section 3.

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

- **Open bug (2026-10-12):** the overlay walkthrough refuses Full VR while the native onboarding accepts it (Onboarding.cpp:56 against NativeOnboarding.h:40). Without MenuQue a player cannot choose Full VR on the first start at all and is told it cannot be chosen.
- **Open bug (2026-10-12):** README.md:8-11, :40-41 and OBVR.ini:1609 describe Full VR as switched off and never tested in a headset. Both are wrong since 2026-09-27.
- **Open bug (2026-10-12):** the overlay walkthrough says "snap turning is planned" (Onboarding.cpp:97); it exists.
- **Not verified:** whether the vanilla tutorial messages in the prison intro show keyboard names in Full VR. Derived from the absence of any rewriting code; to settle: a new game in Full VR, the first "Press ..." message read.
- **Not verified:** docs/next-up.md "New games": the walkthrough and the hand guides around the intro and the character creation have not been tested in a headset.
- **Decision for the tester:** the status word ("Experimental" proposed), and whether the first-start chain (3.2) may run before any game is loaded - it asks for the controllers in the main menu, which `ControllerMenus=1` already gives.
