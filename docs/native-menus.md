# Native OBVR menus

## Restored to main — 2026-09-20

The native MenuQue onboarding and settings implementation was restored from
`dlss5` commit `ecaedb4` onto the current water/foliage main line. The port is
deliberately limited to the native menu host, shared settings model, MenuQue
v16b signature check, XML assets, fallback policy, tests, and release packaging.
The divergent SteamVR Actions, Full VR perspective, water, and rendering code
from that branch was not imported.

On current main, Insert and the two-stick settings toggle reach the native menu.
Mouse/keyboard interaction and the existing controller-to-game-menu path remain
the input owners; the later `dlss5` menu mailbox is not part of this port. If the
xOBSE task/console interfaces, verified MenuQue binary, or XML assets are absent,
the existing OBVR overlay menus remain active.

Measured for this restoration: the Win32 Release DLL builds, all 63 CTest targets
pass, and a package inspection contains the DLL, INI, licence and all three menu
assets. A fresh in-game run of this exact restored build is still required before
release acceptance.

## Controller regression correction — 2026-09-11

The preceding partial SteamVR migration was wrong: it called SetActionManifestPath
but still read legacy GetControllerStateWithPose for trigger/stick/button data.
Valve confirms that the old API no longer functions once an app uses the action
manifest: [Valve issue 1102](https://github.com/ValveSoftware/openvr/issues/1102).
This supersedes earlier claims that these input paths could coexist. The previous
replay bypassed the input reader and therefore could not detect this regression.

All controls now use IVRInput_011: five digital actions and trigger/vector2-stick
analog actions per hand. Pose reads use GetDeviceToAbsoluteTrackingPose independently.
Inactive/failed actions are neutral without discarding a valid hand pose. Both Index
and Touch bindings include all required controls; Touch trackpad is optional/unbound.
Binding version 2 rejects stale click-only bindings. Defaults were built from the
installed SteamVR Index/Touch legacy profile component paths, not guessed layouts.
Analog ABI is taken from Valve's openvr_capi.h (48 bytes on Windows).

Laser hover uses the game's existing hit test with direct pixel deltas, without the
previous smoothing lag. The cursor render node is culled while laser pointing;
only OBVR's owned cull bit is restored when pointing ends, including disconnect.
The normal Oblivion hover background remains intact. Captured onboarding and Load
hover images were inspected: no cursor sprite. A separate laser-trigger test now
opens Load after stick navigation, rather than accepting a keyboard-only test as
proof of laser click delivery.

Schema 7 replays buttons through the production action reader using a fake action
table, AND separately requires all mandatory live bindings on both controllers.
The first runs correctly failed because controllers were disconnected. Once both
connected, `20260911-083343-288` passed: all mandatory bindings active, startup cursor,
onboarding choice and laser click, main selection advance/return, direct Load laser
click, and ten world hand/render cases. Both INIs restored byte-for-byte. Unit tests
exercise 32,768 combinations of active/pressed/error actions, plus unavailable API,
invalid axes, pose preservation and cursor ownership. Physical trigger actuation
and an equipped sword are not simulated by binding activation; world fixtures use
the existing fist/hand cases. Manual headset verification remains distinct.


Final replay `20260910-231751-526`: startup cursor, onboarding stick selection,
onboarding laser click, main-menu advance/return and trigger opening Load all pass;
ten world cases pass too. Both INIs restored. This uses synthetic controller input
in the real engine; physical headset interaction remains unverified for this build.

## Menu input update — 2026-09-10

The settings row **Wrist menus (off: floating)** switches `Hands.WristMenu`.
With `Render.Menus=world` and the HUD overlay enabled, off uses the existing large
floating panel. Main menu/onboarding remain on the cinematic presentation.

The Index legacy binding maps both physical thumbstick clicks and 80% stick
position to Axis0 press. OBVR now reads dedicated SteamVR physical-click actions
from `Data/OBSE/Plugins/OBVR_Input/actions.json`, with Index and Touch bindings.
Only these clicks can produce the two-stick OBVR shortcut. Missing/inactive action
bindings fail closed; Insert remains available. Actual Index action activation was
logged on both hands; physical button presses and Touch hardware were not tested.
Source: installed SteamVR `drivers/indexcontroller/resources/input/legacy_bindings_index_controller.json`;
[Valve action manifests](https://github.com/ValveSoftware/openvr/wiki/Action-manifest).

The old cursor reader used scene X/depth at InterfaceManager+0x20/+0x24. The
engine's pixel cursor is +0x2C/+0x34 (Oblivion.exe writes at 0x57E9C4/0x57EA03,
hit testing at 0x581390). The corrected reader and main-thread xOBSE MoveMouseX/Y
bridge make laser input converge to the engine cursor. Clicks use TapKey 256.
Queued packets reject changed menu IDs/GenericMenu roots. The render thread never
executes menu scripts. [xOBSE input commands](https://github.com/llde/xOBSE/blob/master/obse/obse/Commands_Input.cpp).

Both sticks now navigate entries, without also scrolling the mouse wheel. Moving
the laser resumes pointing; otherwise the trigger confirms the keyboard selection.
Owned onboarding explicitly selects its two XML buttons. Other menus use the
engine XML navigation handler at 0x580BA0 on the main thread; its direction values
are left=1, right=2, down=3, up=4. Left/right map to up/down in the vertical main menu.
Buffered MenuTapKey and MenuHoldKey did not advance selection in this installed
NorthernUI setup; the reason for that interception is not established. Direct XML
navigation is verified; no NorthernUI-specific tile names are hardcoded.


The MenuQue onboarding and Insert settings menu are enabled by default through the startup-only compatibility key `[Onboarding] NativePrototype=1`. Setting it to 0 restores the legacy overlay menus. Missing xOBSE task/console interfaces, missing XML assets or an unrecognized MenuQue poll routine also retain the legacy menus. A failed settings XML open restores the legacy entry point.

The onboarding has two choices, Keyboard/Gamepad + VR and Full VR. Both choices are enabled. Full VR is labelled experimental. Each choice saves Hands.Enabled (0 for Keyboard/Gamepad + VR, 1 for Full VR) before applying it; failed writes offer a retry. There is no Decide later button. Both onboarding and settings use the game's dialog_selection_full.dds and dialog_selection_cut.dds as a background hover effect; text RGB values remain constant.

Insert opens/closes the native settings menu; its Close button closes it too. Previous/Next buttons or Page Up/Page Down navigate nine pages of seven rows, including all 58 existing SettingDefinitions. Clicking a label selects its help text; minus/plus adjust it. Changes are saved before application, failed saves preserve the current value, and restart-only settings carry a marker. Recenter is handed back to the render callback. In the stereo HUD path, the existing menu width/distance/anchor settings are used while this menu is open. The native menu otherwise uses the game's existing UI presentation, including its cinematic main-menu presentation and tracked wrist placement.

Only the owned GenericMenu is handled/closed. Foreign generic menus block opening. Covered menus ignore our keyboard input. The old bitmap menu remains as a compatibility fallback, not a second menu shown alongside MenuQue. This change does not implement the unfinished full motion VR gameplay.

## Implementation evidence

- xOBSE [PluginAPI.h](https://github.com/llde/xOBSE/blob/master/obse/obse/PluginAPI.h): Console interface v2, RunScriptLine2, Tasks interface. [Commands_Console.cpp](https://github.com/llde/xOBSE/blob/master/obse/obse/Commands_Console.cpp) constructs a temporary script and calls CompileAndRun with mode 1. [Script.cpp](https://github.com/llde/xOBSE/blob/master/obse/obse/Script.cpp) locates that engine function at 0x004FBF00.
- Local Oblivion.exe disassembly at 0x004FBF23-0x004FBF48: mode 1 sets byte 0x00B361AC before execution and clears it afterwards. [GameAPI.cpp](https://github.com/llde/xOBSE/blob/master/obse/obse/GameAPI.cpp) identifies that byte as g_bConsoleMode. [Utilities.cpp](https://github.com/llde/xOBSE/blob/master/obse/obse/Utilities.cpp) chooses @ for console temporary scripts. This confirms the separator even when the visible console is closed. Evidence capture: build-ci-tests/menuque-evidence/console-mode.txt. DeepWiki could not settle the engine-internal step; the actual binary did.
- [Commands_Menu.cpp](https://github.com/llde/xOBSE/blob/master/obse/obse/Commands_Menu.cpp) implements SetMenuStringValue/SetMenuFloatValue/ClickMenuButton. Empty string operands are dropped by its tokenizer, so the builder refuses them; blank UI slots explicitly contain one space. Quotes and percent signs are escaped; command separators/control characters and invalid trait names are refused.
- MenuQue v16b's bundled docs, extracted locally to build-ci-tests/menuque-evidence/commands.html: ShowGenericMenu takes a path relative to Data/Menus/Generic and an optional closingID; only that ID closes the menu when positive. Settings use 9299.
- MenuQue's script-facing GetGenericButtonPressed requires a script context. The host instead calls its verified internal bool(int*) poll routine at submodule RVA 0xEA30. All 38 bytes, including relocated operands, must match. This is a dependency on the verified v16b binary, not a public stable MenuQue C++ API. Verified Submodule.Game.dll SHA256: B1F904137A1CDF888F1E4A2F24BD293CB17DD9EFFB1502021256C25FD5CFBC91. The installer copies dependencies only from the user's existing MenuQue installation; release packages do not redistribute it.
- Highlight texture names/layout were read from the locally installed DarNified UI menus/prefabs/darn/button_floating.xml. OBVR supplies its own XML and references the existing game textures.

## Validation, 2026-09-09

The user confirmed the earlier native onboarding prototype in the headset. The user subsequently confirmed the native menu update and reported that the lower hover highlight was clipped. The new clipping correction has not yet been visually verified in the running game. The OBSE loader produced no game window; a subsequent direct launch displayed Steam Application load error 5:0000065434 before reaching the game UI. No cause for that launch error has been established here.

Release DLL built successfully. All 53 tests passed before the final refinements; affected native menu/config tests were rebuilt and rerun afterwards. NativeSettingsTest covers all 58 definitions, pagination/selection, bounds, save failure/success, actions, buffer/encoding refusal paths, atomic mailbox semantics, and all 512 lifecycle input combinations. NativeOnboardingTest covers onboarding transitions and each byte of the MenuQue signature. NativeMenuAssetsTest.py validates XML structure, IDs/row bindings, local includes, constant text colors and background hover textures. These tests do not execute Oblivion's XML renderer, mouse hit testing, engine console commands, or headset presentation; those runtime checks remain outstanding.

The original `dlss5` prototype used a machine-specific installer that is not
carried into current main. `tools/package-release.ps1` now includes the XML
assets alongside the DLL and INI, which is the supported installation path.

## Hover clipping correction and dependency handoff

The previous prefab reduced each image tile from the native reference's 64px to 48px
without adjusting zoom. Oblivion image tiles crop the texture to their bounds;
changing height alone does not resize the full image. The correction sets both pieces
to 42px with 65.625% zoom, and the 104px end cap to 68.25px width. At y=3 this fits the
smallest 45px button. Both pieces share y relative to the button locus; text colors
remain constant. This fixes the XML sizing cause; in-game visual confirmation is pending.
Sources: [Property Element](https://cs.uesp.net/wiki/Property_Element),
[XML Traits](https://en.uesp.net/wiki/Oblivion_Mod:Oblivion_XML/Traits), and the local
DarNified UI button_floating.xml reference. The asset regression test checks full-height
scaling, cap alignment, and containment for every button using the prefab.

MenuQue [v16b download](https://www.nexusmods.com/oblivion/mods/32200) is now listed in
README and the Nexus publishing draft as a requirement for native menus. The MenuQue
DLL is not redistributed. The existing legacy fallback remains intentional.
User documentation, INI comments, UI architecture notes and HANDOFF are synchronized.

## Controller follow-up, 2026-09-11

Native menu input now forwards Tab as well as Escape/accept. The pointing stick
scrolls a hit world-menu list; the other stick navigates. Startup menus retain stick
navigation. Ray and beam share SteamVR `/pose/tip`. `[Hands] TouchMenus=1` is an
experimental controller-tip contact proxy, with release-to-click and drag-to-scroll;
visible character-finger alignment remains unfinished. See
[controller details](hand-tracked-mode.md).

## Input correction, 2026-09-11

Native input preserves queued B/navigation edges. Tab/Escape use poll-consumed
xOBSE TapKey; Return keeps buffered menu input. Full VR reticle/activation direction
now uses the Index tip pose, including first-person picking. Schema 8 checks both
menu open/close roundtrips and weapon sheath/draw in the real engine.

## Perspective routing, 2026-09-11

Full VR no longer forces first person. Left B switches gameplay perspectives; in
menus it still closes/backtracks. Third person uses classic gamepad/gaze behavior,
including head reticle and floating menus; first person retains motion behavior.
The selected Hands.Enabled flag is distinct from effective MotionPerspective.
