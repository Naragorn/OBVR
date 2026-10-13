# HUD on the hands: feasibility and plan (2026-09-29, not built)

Goal (tester's words, paraphrased):

- **Left hand:** health, magicka, fatigue and the equipped spell.
- **Right hand:** everything else from the HUD.
- **The compass moves into the sky.** It sits above the normal field of view when the head looks straight ahead. When the player looks up ("towards the sun"), it fades in slowly.
- **Every element has its own opacity** and is configurable.
- **Mod compatibility stays:** UI mods such as DarNified UI and NorthernUI must keep working.
- **Dialogue stays as it is**, with two changes:
  - the dialogue panel recentres once on the NPC when a conversation opens;
  - the panel's size is adjustable, default 80 %.

This file is a feasibility analysis. Nothing in it is built.

## 1. What exists today (read from the code)

**The whole interface is one texture.**
- "Route B" redirects Oblivion's interface pass into an OBVR-owned render target (`src/render/HudLayer.cpp:92-98`, `src/render/InterfaceRenderHook.cpp:1932-1968`, bracketed in `CameraHook.cpp` by `BeginCapture` and `HudEndRedirect`).
- HUDMainMenu, HUDInfo, HUDReticle, the subtitles and the menus all land in that one texture. It is shown as a single OpenVR overlay, `obvr.hud`.
- Its placement modes are head, room, or a wrist (`HudLayer.cpp:285-428`).

**The wrist placement already exists.**
- `WristHud` and `WristMenu` in `[Hands]` hang the whole HUD picture on one wrist (`HudLayer::SetWristPlacement`; `docs/hand-tracked-mode.md:127-139`).
- Both default to off, because a headset run found them awkward.
- This moves the whole picture, not single elements.

**Taking one element out of the texture is already proven, by the crosshair.**
- `CrosshairLayer::TakeFromHud` (`src/render/CrosshairLayer.cpp:81-137`) works in two steps:
  1. it StretchRects a rectangle out of the HUD capture into its own texture;
  2. it ColorFills that rectangle transparent in the HUD, so the crosshair is not drawn twice.
- The crosshair is then hung ahead of the right controller (`CrosshairLayer::SetHandPlacement`).
- This is exactly the operation every hand element needs.

**The rest of the pieces:**
- **Per-overlay opacity:** `OpenVRBackend::SetOverlayAlpha` (`OpenVRBackend.h:297`) and `SetOverlayTextureBounds` (`:306`).
- **Tiles:**
  - OBVR knows the tile name, parent and render-node offsets (`GameAddresses.h:651-657`) and the HUD menu ids (`MenuType.h:46-53`).
  - It can write a tile's visible and alpha traits (`SetHudReticleEnabled`, `CrosshairTarget.cpp:136-201`).
  - It does **not** yet walk HUDMainMenu's child tiles or read their positions.
- **Game values:** fatigue is read through the actor-value getter (`PlayerTeleport.cpp:212-224`), and the equipped weapon form is read (`MeleeHits.cpp:251`). Health, magicka and the equipped spell are not read yet.
- **Dialogue:** the dialogue panel uses the shared `HudLayer` room anchor. It is taken when the layer appears and retaken on recenter or after a drift of more than 2 m. Nothing aims it at the NPC. `MenuScale` only affects the flat cinema path (`EyeMirror.cpp:322`).

**OpenVR limits:**
- `k_unMaxOverlayCount = 128` (openvr_capi.h, https://github.com/ValveSoftware/openvr/blob/09240643/headers/openvr_capi.h).
- About 8 extra overlays for the hand elements is well inside that limit.

## 2. The three ways to get single elements

| Route | How | UI mods | Look | Effort |
|---|---|---|---|---|
| A. Fixed crop rectangles | Cut fixed pixel rectangles out of the HUD capture, the way the crosshair does | Break: DarNified UI and others move the elements | The game's own, mods' own | Small |
| **B. Crop rectangles found from the tiles** | Walk HUDMainMenu's tile tree each time the layout changes. Read each element's x, y, width and height traits, and crop exactly that rectangle | Keep working **if** the mod keeps the vanilla tile names (to verify) | The game's own, mods' own | Medium |
| C. Draw our own widgets | Read health, magicka, fatigue, spell and so on from the game, and draw OBVR's own bars and icons | Independent of every UI mod | OBVR's look, not the mod's | Large (every element redrawn, icons loaded) |

**Recommendation: B, with A as the fallback.**
- Every element is cut from the picture the game (or the UI mod) already drew, so a UI mod's look carries over to the hands.
- The rectangles come from the tile tree, so a moved element is still found.
- Where a mod renamed a tile, an INI rectangle (A) can be set for that element.
- An element that is found neither way stays in the main HUD panel, where it is today. Nothing gets lost.
- C stays open for single elements where cropping looks poor, for example bars that are too thin to read on a hand.

**What B stands on (to verify, step 1 below):**
- that the vanilla `hud_main_menu.xml` tiles have stable names for the bars, the compass, the icons and the active effects;
- that DarNified UI and NorthernUI keep those names;
- that the tile's x, y, width and height traits give the rectangle on screen in capture pixels.

The vanilla file is packed in `Oblivion - Misc.bsa` (no loose `Data\menus` on this install). I have not read its tile names; step 1 logs them from the running game instead.

NorthernUI rewrites parts of HUDMainMenu, for example the compass icon alpha (https://github.com/DavidJCobb/NorthernUI/commit/aaaa8c10e8704ab04d1a8e07b27e824dcd820cb1). Its names have to be checked in the same step.

## 3. The layout

**Left hand, above the back of the hand, like a watch face tilted towards the eyes:**
- health, magicka and fatigue bars;
- the equipped spell icon.

**Right hand, same kind of panel:**
- the equipped weapon icon, arrows, and weapon condition;
- the active effects;
- the enemy health bar;
- the sneak eye.

**Subtitles and messages** do not go on a hand. The tester decided this on 2026-09-29:

- **Placement:**
  - Messages (top left in vanilla) show at the top centre of the view.
  - Subtitles show at the bottom centre of the view.
- **They always follow the current view.** Each one is placed in the middle of the headset's view at the moment it appears, not at the room anchor. Once the player has turned away from the recenter point, a room anchor would put them somewhere else entirely.
- **Proposal:** a message stays where it appeared while it is shown, rather than being locked to the head. A head-locked text makes some people sick. `HudTextFollow` could offer "where it appeared" or "locked to the head".

**The crosshair and tooltips** already have their own layers and do not change.

**The compass in the sky:**
- It is placed in the room, at the head's heading (yaw only). It sits at a set elevation above the horizon: default 40°, a setting.
- Its opacity follows the head's pitch:
  - invisible while looking straight;
  - fades in between `CompassFadeStartDegrees` (default 20° up) and `CompassFadeFullDegrees` (default 35° up);
  - reaches at most its own opacity setting.
- It keeps showing the game's compass picture, turned with the **view's** heading, not the body's (`game/CompassHeading.h`): HUDMainMenu's update (0x005A6DE0) reads the player's heading virtual (+0x1E0) at 0x005A6F05; that read is rerouted to the camera's heading as last drawn, levelled even looking straight up (`game::ViewHeadingOf`, compass_heading_test). The markers take their bearing from positions and sit against the strip, so they turn with it; the interior north rotation is still added by the game. So "north" stays correct. Before, the head turned only the camera and the strip stood still (the tester, 2026-10-01: "der kompass wenn man hochschaut dreht sich nicht mit der headset sicht"). Harness `walk-direction` 2026-10-01: the head turned 90 degrees, the body still at 0 - the compass read 270 (W), the engine's heading 0.

**Every element** gets its own settings:
- on or off;
- hand (left, right or main panel);
- opacity 0–100 %;
- size;
- offset from the hand.

**Global settings:**
- `HudOnHands` (on or off);
- when the hand HUD shows: always, when the palm is turned towards the face, or when the hand is raised.
- The "palm towards the face" check reuses the hand gesture code. It is a proposal; the default is always.

**`WristHud`** is replaced by `HudOnHands`, because it becomes one choice among the per-element hands. **`WristMenu`** stays (menus on the wrist is a separate feature).

## 4. Dialogue

**Recentre once on the NPC.**
- When DialogMenu opens, the panel's room anchor is set once. It goes on the line from the head to the NPC's head, at `HudDistanceMetres`, turned towards the player.
- It then stays put, as now. The recenter key still works.
- The NPC comes from the dialogue target. Which engine field holds it is to find; the HUDInfo crosshair reference (`CrosshairTarget.cpp:117-131`) is the fallback, because a conversation normally starts by activating the NPC under the crosshair.
- **Beside the NPC** (agreed 2026-09-29): the panel can sit beside the NPC instead of in front of the face, for example 30 cm to the right and slightly lower, so the NPC stays visible. The setting `DialogPanelSide` offers centre, right or left.

**Size:** `DialogPanelScale`, default 80 %. It scales the panel while DialogMenu is open (and the persuasion minigame with it).

## 5. Mod compatibility

- **Cropping from the capture (B/A)** keeps every UI mod's graphics, because OBVR shows what the mod drew.
- **Nothing is written into the menu XML** and no tile is moved. The only tile writes would be alpha or visible on the elements the hands show, and only if cropping and erasing is not enough (step 2 decides).
- **Erasing** a cropped element from the main panel is a ColorFill on OBVR's own texture, the same as the crosshair. The game's tiles are not touched.
- **NorthernUI** already works with OBVR. Its HUD names are checked in step 1.
- **An element OBVR cannot find** stays in the main panel. A UI mod can never make an element disappear.

## 6. Risks and open questions

- **Thin bars:** vanilla bars are a few pixels wide, so cropping them onto a small hand panel may be blurry. Step 2 shows it.
  - Every hand element gets its own size (zoom) setting (agreed 2026-09-29).
  - If the bars are still too blurry at a readable size, OBVR draws them itself (route C) from the actor values.
- **Layout changes:** a UI mod's scale settings, or a resolution change, move the rectangles. Re-walking the tiles when the HUD's size changes handles that. How often the walk is cheap enough is unknown until measured.
- **Tile coordinates:** they are in the game's UI space. They need the same scaling the crosshair's `CrosshairSourceShare` uses to become capture pixels. This is unverified.
- **Ammo and weapon icons** in vanilla sit right next to the bars (bottom left). That is a guess from memory until step 1 logs it. A single crop of that corner would put the bars and the weapon on the same hand, so they need separate rectangles.
- **Reading the dialogue target field:** still to find in the engine.

## 7. Build steps

1. **Tile probe.**
   - Walk HUDMainMenu's tile tree and log each tile's name, x, y, width, height and visible state.
   - Run it with vanilla. If Nadi has them installed, also run it with DarNified UI and NorthernUI.
   - Result: the list of elements, their names and rectangles. This decides B against A per element.
   - Harness: a scenario that loads the save and logs the tree.
2. **Crop and erase per element.**
   - Generalise `CrosshairLayer::TakeFromHud` to a list of elements, each with its own texture region.
   - Use one shared atlas texture and one overlay per element with `SetOverlayTextureBounds`.
   - Harness screenshots of the atlas.
3. **Hand placement.** Place each overlay relative to its controller, with offsets, opacity and size from the settings. Pure placement maths go in a logic header with tests.
4. **Compass in the sky.** Room placement plus the pitch fade as a pure function (`CompassOpacity(pitch, start, full, max)`) with tests for every flow.
5. **Dialogue recentre and scale.**
   - Find the dialogue target and anchor the panel once per dialogue episode.
   - `DialogPanelScale` defaults to 0.8.
   - Tests for the anchor decision: new episode, same episode, target missing, recenter key.
6. **Settings.**
   - INI rows and menu rows.
   - `WristHud` migration: a set `WristHud=1` means `HudOnHands=1`.
   - Onboarding text.

Step 1 decides how much of B is possible. The rest does not depend on its outcome, only on where the rectangles come from.

## 8. Step 1 results (2026-09-29)

**How it was run.** Built as `[Debug] HudTileProbe` (`src/game/HudTiles.cpp`). It was run with the harness scenario `tools/hand-scripts/hud-tiles.txt`:
- vanilla UI;
- a capture of 3916x3480, with the game believing 3916x2203;
- run folder `artifacts/hand-script/hud-tiles/20260929-184919`.

It PASSED: HUDMainMenu has 34 tiles and HUDInfoMenu has 21.

**The tile layout from xOBSE holds.** The value list is at +0x14 and the child list at +0x30. Every child points back at its parent.

**Units** (measured):
- The HUD is laid out in a space 960 units high and 1706.5 wide (`player_grab_zone`, w 1706.5, h 960). 1706.5 is 960 times the aspect.
- A tile unit is `believedHeight / 960` capture pixels, with the origin at the top left: 2203 / 960 = 2.2948, and 3916 / 1706.5 = 2.2948.
- Check against the dumped capture:
  - the health bar is computed at (98, 864) units, i.e. (225, 1983) pixels;
  - `OBVR-HudTiles-hud.bmp` shows it at about (220, 1985).
- The capture is taller than the believed frame. Only its top `believedHeight` rows hold the HUD.

**Absolute position** is the sum of the `x`/`y` traits up the parent chain. The root is at (0, 0).
- This matches the render nodes' world translation: screen x = world.x + 853.25, screen y = 480 − world.z.
- Tiles without `locus` are the exception. Their offset goes into their geometry, not their node, so their node sits at the parent's position. The trait sum still gives their place.

**Vanilla elements** (absolute rectangles in tile units; all children of `hudmain_background` at (87, 850)):

| Element | Tile | Rectangle (x, y, w, h) |
|---|---|---|
| Health / magicka / fatigue bars | `hudmain_statusbars` → `hudmain_health_empty`, `hudmain_magic_empty`, `hudmain_fatigue_empty` | (87, 862, 189, 52); each bar 189 × 16 at y 862 / 880 / 898 |
| Equipped weapon, ammo count, condition | `hudmain_Weapon_Icon` (+ `hudmain_weapon_ammo`, `hudmain_weapon_status`) | (316, 855, 63, 63), ammo and condition inside to x + 81 |
| Equipped spell | `hudmain_Magic_Icon` | (411, 855, 63, 63) |
| Compass | `hudmain_compass_layout` → `hudmain_compass_window` | (494, 855, 213, 87), frame (480, 845, 220, 84) |
| Region name on discovery | `hudmain_region` | (87, 805, 360, 45) |
| Active effects | `magic_icons` | from (1642.5, 58), grows by its children |
| Level up icon | `hudmain_Levelup_Icon` | (711, 856, 63, 63) |
| Crosshair info (name, action, icons) | HUDInfoMenu `hudinfo_*` | around (1578–1714, 827–965) |

**Menus not loaded at the time:**
- HUDReticle (the enemy health bar and the sneak eye) and HUDSubtitleMenu. The existing persistent root pointers (`kHudReticleRootPointer`) are the way to HUDReticle.
- The top-left messages showed in neither tree. They come and go with a message, so a run that shows one still has to find them.

**Decision:** route B holds for vanilla. Every element the hands need has a stable, named tile with a rectangle that matches the picture.

**The notices and the subtitles** (a later run on 2026-09-29, `tools/hand-scripts/hud-messages.txt`):
- They live in HUDSubtitleMenu, as `hudsubtitle_notice` (with `hudsubtitle_icon`) and `hudsubtitle_text`.
- The tile menu array does not carry that menu. It hangs under `InterfaceManager::menuRoot` (+0x68, xOBSE's GameAPI.h) once the game first shows one.
- A notice read: "Your Blade skill increased.", x 48, y 40, w 554, h 45, visible 2.
- The subtitle text is centre-justified: its x is its middle.
- The enemy health bar and the sneak eye are the persistent HUDReticle roots, drawn at the crosshair. The crosshair layer already carries them. They are not hand elements.

## 9. Built (2026-09-29)

Commits `b4e33e3` and the following.

**`src/vr/HandHud.h`** holds the pure logic (`hand_hud_test`):
- the places;
- the tile rectangles, including justify, and alpha for the region's name;
- the capture pixels and the atlas;
- the rows on the hands;
- the compass pose and its fade;
- the top and bottom of the view, with their anchor.

**`src/render/HandHudLayer.*`** lifts the elements into one 2048² atlas, erases them from the panel, and shows one overlay per element.

**`src/vr/DialogPanel.h`** covers the dialogue panel on the speaker and its size (`dialog_panel_test`).

**Default layout:**
- left hand: bars, spell;
- right hand: weapon, active effects, level-up icon;
- sky: compass;
- top of the view: region name and notices;
- bottom of the view: subtitles.

Every element has its own place, opacity and size. The settings are in `[HandHud]` and the settings menu's HUD page. The dialogue settings are `[Look] DialogRecentre`, `DialogPanelSide` (default right), `DialogPanelSideDegrees` and `DialogPanelScale` (default 0.8).

**Harness runs (a sleeping headset, so no tracked hands and no quads):**
- `hand-hud.txt` PASS: bars, spell, weapon and compass lifted; the atlas and the emptied panel written.
- `hand-hud-text.txt`: the notice lifted at 4–6 s after the console line (atlas picture). The final run with the corrected mark was cut off: the tester had started the game.
- `hud-messages.txt` PASS.

**Not tested:**
- The quads on the hands and in the sky, which need tracked controllers and a head pose.
- The dialogue panel on the speaker, which needs a conversation. Its decision and anchor have unit tests.
- How readable the bars are at 12 cm.

**Headset (2026-09-29):** "ja HUD geht alles habe es getestet." The settings are not tried yet.

**Shown on a look (2026-09-29, the tester: "man muss die hand mit vr anschauen und die hand zum gesicht drehen quasi mit offener hand").** A hand's HUD fades in over 0.15 s only while three things hold:
- the hand is within `LookGazeDegrees` (35) of the head's forward;
- its palm is within `LookPalmDegrees` (55) of facing the eyes;
- the hand is not a fist (finger curl < 0.6, or the grip's squeeze without curls).

While shown, the HUD hangs `PalmLiftMetres` (0.06) off the palm, facing the eyes, and is hung on the controller. `ShowOnLook=0` brings the watch on the back of the hand back.

The palm's side is **assumed** to be the controller's +x on the left hand and −x on the right, from OpenVR's controller axes. The headset has not confirmed it. If the HUD shows with the back of the hand instead, that is the sign to flip.

`tools/hand-scripts/hand-hud-look.txt` covers it, but it has not run yet: the tester was playing.

**Reworked (2026-10-06; headset verification pending):** the crosshair cache has been removed. `DialogCameraShim` copies the actual actor position from `SetDialogCamera` before either the POV switch or vanilla zoom runs. This also supplies a target for NPC-initiated greetings. The panel places once per conversation, retrying if the first frame has no valid head/camera pose. Side and size settings remain unchanged. `DialogFocus` clears the copied position on explicit end, menu exit, or cancelled approach. The camera observer stays installed with zoom on; a zoom setting change takes effect for the next conversation so its original exit cleanup is retained. `dialog_panel` and `dialog_camera_hook` exercise the placement policy and engine-facing wiring without running Oblivion.

**Headset (2026-09-29, second look):** "hud an den händen auch zu weit hoch. dachte da eher an das dass hud dann in der handfläche ist". Since then the HUD lies in the palm:
- `PalmDownMetres` 0.05 below the controller's tracked origin, along its −y;
- `PalmLiftMetres` 0.02 off the palm, facing the eyes.

The dialogue panel's side now defaults to centre, because right was "zu weit rechts".

Menus open in front: the room anchor is dropped as a menu opens, so a menu or a book is placed where the head looks. Before, the anchor was kept from the first placement, and with the body and the walking direction turned away from the view a menu could open behind the player.

**The action icon twice (2026-10-07).** The tester saw double tooltips of
two sizes in Full VR: the crosshair quad carries HUDReticle's context icon
(at the laser, or in the reach ring, `render::ReachIconPose`), and the Info
row under the thing carried HUDInfoMenu's `hudinfo_action_icon` as well,
scaled with the distance. The Info element now leaves that tile out
(`vr::kHudInfoActionIcon`, hand_hud_test): the icon is the ring's, the text
the row's. Not seen in the headset.

**The enemy health bars over the fighters (asked 2026-10-07, built
2026-10-13; the tester: "Gegner-Lebensbalken über dem NPC in der Welt. nicht
mehr am crosshair. default an").** Not the plan above (HUDReticle's tile
lifted as a hand-HUD element over the crosshair target): a lifted tile is
one bar for the one fighter the pick is on, and in the headset the pick is
the laser - a fight is not fought by pointing. Instead OBVR's own bars,
`[HandHud] EnemyHealthBars=1` (HUD, "Enemy health over them"),
`EnemyHealthMetres` (25, "Enemy health range (m)") and
`EnemyHealthWidthMetres` (0.35, "Enemy health bar width (m)"), both modes:

- **Who:** every Character or Creature in the process manager's high list
  whose combat target is the player (the combat ring's own test,
  `game::ActorFightsPlayer`: Actor vtable +0x334 in combat, +0x338 the
  target), alive, within the range of the eyes - up to eight, the nearest
  first (`game/EnemyHealthLogic.h`, enemy_health_test; `game/EnemyHealth.h`
  reads them).
- **What:** their health now over their base (GetActorValue_F, vtable
  +0x288, actor value 8; GetBaseActorValue 0x005F1910 - the fatigue's
  readers), as a fraction; painted anew when it moved a 128th
  (`ui/EnemyHealthPainter.h`, 256 by 64): since 2026-10-13 afternoon the
  game's own frame for it (the golden arc, below), and only without the
  game's frames OBVR's own bar - a dark bed, the quick menu's gold frame, a
  red fill as wide as the health.
- **Where:** 14 units above the head bone ("Bip01 Head", through the bones
  BladeBodies keeps, read on demand - `game::ActorBoneNode`), or 136 above
  the feet without it, both times the skeleton's scale; one CanvasOverlay a
  bar (the stow spot's kind), turned to the eyes and upright
  (`StowSpotPose`), the set width or wider so it keeps about 3.4 degrees far
  off (`EnemyHealthBarWidth`).
- **When:** with the HUD - hidden in menus, while a game loads, when dead.
  A bar lingers two seconds after its fighter dies (empty) or stops
  fighting, then goes; a corpse never gets one.
- **The game's own bar:** while the setting is on, HUDReticle's third
  persistent root (`kHudAuxRootPointer`, the enemy health) has its visible
  trait written false after every HUDReticle update the crosshair tooltip's
  hook wraps (`game::SetEnemyHealthTileHidden`, game/WorldPickHook.cpp) -
  so it never rides the crosshair quad. Each time the engine has it up (the
  root sized before the write) a line says so, with its tiles: the open
  bug below asked whether the game shows it in VR at all.
- Logged "Enemy health: a bar over <ref> - n% health, n m away, above the
  head bone (bar 1 of 8)", "<ref>'s bar went (dead or no longer fighting, 2
  s after)", and at a hand script's marks the state of every bar ("at the
  mark - bar 1 over <ref>: n% (painted n times), n m away, n m wide, the
  overlay up, shown n s").

**The game's own bar, frame for frame (the tester, 2026-10-13 noon: "ist
es möglich statt unserem eigenen balken den von vanilla zu rendern über
den gegnern" - "mach A"; built the same afternoon).** The game's enemy
health is a model, not an image: hud_reticle.xml's `<nif>`
hudreticle_enemy_health loads `meshes/menus/enemy health bar/
health_bar01.nif` (read with pyffi): one quad, 128 by 32, with a
NiFlipController on its texturing property holding 40 textures,
`textures/menus/misc/sequence_dds/000001..000040.dds` (128 by 32, DXT5, no
mipmaps) - a golden arc, the widest in the first frame (79 of 128 columns)
and a stub in the last (11): full health to none; the HUD runs them by its
sequence "Health_Bar" from the tile's user traits. Now
(`game/EnemyHealthModel.h`) the geometry is found under HUDReticle's enemy
health tile (`kHudAuxRootPointer`, its render node at +0x24, the first
geometry with a NiFlipController on its NiTexturingProperty - the classes
told by their vtables, 0x00A7DB5C and 0x00A7A33C), the controller's
textures (its NiTArray at +0x44, the count at +0x4A) are the frames, and
each frame's pixels are read off the game's own Direct3D texture the first
time a bar wants it (NiTexture's renderer data at +0x24, the
IDirect3DTexture9 among its first dwords - at +0x50 - told by the vtable a
texture OBVR makes shares; level 0 locked read-only, the DXT5 blocks
decoded by `ui/DxtDecode.h`, enemy_health_test) and kept; the painter
scales the frame for the fraction (`EnemyHealthFrameIndex`: 0 full, 39
empty, the nearest) onto the overlay with the frame's own transparency. A
UI mod's bar comes along: whatever model and textures the HUD loaded are
what is read. Without the game's frames (none found, the texture not
lockable, a format OBVR does not decode) the bar is OBVR's own, said once.

- **Why not the model itself in the world (route A as first tried).** The
  geometry was cloned under each fighter's scene root (NiObject::Clone, the
  bow's way), placed through its local transform, given render states from
  the HUD's own geometry and a fresh BSShaderPPLightingProperty the
  engine's way (0x004ED1B0's recipe: 0xF0 bytes from the game's heap, the
  constructor 0x007D94B0, NiAVObject::AttachProperty 0x00405680, the
  set-up vtable +0x58 = 0x007DA220) - six harness rounds, and the world's
  renderer drew the quad every time but never with its texture: a black box
  round the arc without a shader property (the texture used, its alpha
  not), a white one with it, a grey one lit. The set-up logs it: "TEXTURE
  ERROR : texture does not contain mipmaps & will not be used" (the format
  at 0x00A8F4A0, the check of the texture's mip count at 0x007DA8B8) - the
  world's lighting shader refuses a texture without mipmaps, and the
  frames have none. The HUD draws them through its own path, which does
  not mind. So the frames are read and painted instead; the clone work is
  in the history (commit 57f2f36's follow-ups), not in the tree. Read on
  the way, for later: NiGeometry's property state at +0xAC and effect state
  at +0xB0, built by vtable +0x6C (UpdatePropertiesDownward, 0x00722B30)
  and +0x70 (UpdateEffectsDownward, 0x00722C00) - a geometry made outside
  the loader has none and the renderer crashes on it at 0x00780848;
  NiPropertyState's ten slots from +0x08; the shader property's flags at
  +0x1C (0x100 set by the set-up when the state's alpha property blends,
  0x2 skinned, 0x3000 for the engine's own quads), its texture arrays at
  +0xBC (diffuse) and +0xC0 (normal maps, looked up as "<name>_n.dds").
- Hand script `enemy-health.txt` PASS 2026-10-13 15:46 and 15:53 (the
  synthetic head faces the nearest fighter, pitched up; the first bar's
  painted picture saved at the marks as OBVR-EnemyHealth-<mark>.bmp beside
  Oblivion.exe, the runner takes it): "the game's own bar model found -
  geometry 18324EE8 "health:0" (NiTriStrips) ... its flip controller
  carries 40 textures, the frames", "frame 1 of 40 read off the game's
  texture 1601D870 ("textures\menus\misc\sequence_dds\000001.dds") - 128x32
  pixels, the picture opaque from column 26 to 104" (the offline decode's
  26..104 exactly), frame 40 "opaque from column 59 to 69", both bars "the
  overlay up (the game's own frame 1 of 40)" at the marks, "bar went" for
  both; the saved picture is the golden arc on nothing. SteamVR's dumps do
  not carry overlays, so the picture file is the proof of the painting;
  the overlay's place and turn are the bars' as before.

Hand script `enemy-health.txt` (two bandits at an untouchable player, the
state at marks, then `killall` for the linger): PASS 2026-10-13 on its first
whole run, with OBVR's own bars then (the one before was stopped as
SteamVR started, at the tester's word): "a bar over 1B9EC538 - 100% health,
1.7 m away, above the head bone (bar 1 of 8)", the second at 2.2 m; at the
three marks of the fight both "the overlay up", 0.35 m wide at 1.8 to 3.1
m, shown 2.5 to 10.6 s; after the kill "1B9EC538's bar went (dead or no
longer fighting, 2 s after)" for both. Not seen in the run: a bar sinking
(the untouchable player never struck, so the health stayed at 100% and
each bar was painted once - the repaint is enemy_health_test's), the empty
bar at a mark (the kill and the two seconds fell between the marks), and
the game's own bar: no "the game's own bar ... came up" line, so the
engine never had HUDReticle's root sized through the fight - with the
laser ahead and no pick on the bandits, which is also how the tester
fights.

**Open bug (2026-09-29): the enemy health bar is not shown - closed
2026-10-13 by the bars above.** The tester: "wir müssen den healthbar von gegnern rendern ... das ist afaik ein vanilla feature".
- It is vanilla: HUDReticle's `hudreticle_enemy_health`, drawn under the crosshair while a fight target is set.
- The step 1 probe found it as the third persistent HUD root, `kHudAuxRootPointer` 0x00B3B358 (x −1, y −20, visible 1 = hidden when idle, file "Enemy He…").
- Why it was missing in VR was never settled; the candidates (the crosshair
  square's lift and erase, `SetHudReticleEnabled` on the title screen, the
  wrapped HUDReticle update) are moot now: the game's bar is hidden on
  purpose and the fighters carry their own. Whether the engine ever has the
  root up in VR: not in the harness run of 2026-10-13 (no pick on the
  fighters); the hide's own line will say so the first time it does.

**The crosshair quad off the laser (2026-10-07).** The tester: "die
tooltips vom laserpointer ausschneiden und stattdessen wie beim heben von
objekten in die welt legen über das objekt wo die hand hinzeigt oder
mittig der vr view (wechselbar in settings). denn so muss man bischen
schielen mit den augen". The quad - the game's reticle with the context
icon it carries - hung ahead of the pointing hand at the aim's depth: off
the line of sight, so the eyes had to cross for it. Now `[Hands]
CrosshairPlace` (Hands, "Crosshair place"; `vr::CrosshairPlace`,
`CrosshairQuadPlace`, hand_hud_test):
- `target` (the default): over the thing the pick has settled on, facing
  the eyes, the mirror of the Info row under it (`TargetHoverPoint` from
  the row's anchor: above a small thing's bound, above the hit of a large
  one by as much as the row hangs below), its apparent size kept over the
  distance; with nothing under the hand the plain reticle rides the laser
  as before, so the dot and the reticle do not come apart. It takes the
  Info row's anchor, so it needs the hand HUD on;
- `laser`: as before;
- `view`: straight ahead of the head at the aim's depth - the crosshair of
  the seated mode.
The reach ring's icon and an arrow on the string still win. The Info row
(the name) is unchanged: "die texte ... finde ich noch ok so". Logged
"Crosshair: the quad over <ref> at <m> (CrosshairPlace=target)". Not seen
in the headset.
