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

**Subtitles and messages** (top left in vanilla) stay in the main panel ahead of the player, not on a hand. Text on a moving hand is hard to read. This is a proposal; it can become a setting.

**The crosshair and tooltips** already have their own layers and do not change.

**The compass in the sky:**
- It is placed in the room, at the head's heading (yaw only). It sits at a set elevation above the horizon: default 40°, a setting.
- Its opacity follows the head's pitch:
  - invisible while looking straight;
  - fades in between `CompassFadeStartDegrees` (default 20° up) and `CompassFadeFullDegrees` (default 35° up);
  - reaches at most its own opacity setting.
- It keeps showing the game's compass picture, and the game turns it with the player's heading as it does now. So "north" stays correct.

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
- **Better alternative (proposal):** put the panel beside the NPC, not in front of the face, for example 30 cm to the right and slightly lower. Then the NPC is not hidden. A setting `DialogPanelSide` could offer centre, right or left.

**Size:** `DialogPanelScale`, default 80 %. It scales the panel while DialogMenu is open (and the persuasion minigame with it).

## 5. Mod compatibility

- **Cropping from the capture (B/A)** keeps every UI mod's graphics, because OBVR shows what the mod drew.
- **Nothing is written into the menu XML** and no tile is moved. The only tile writes would be alpha or visible on the elements the hands show, and only if cropping and erasing is not enough (step 2 decides).
- **Erasing** a cropped element from the main panel is a ColorFill on OBVR's own texture, the same as the crosshair. The game's tiles are not touched.
- **NorthernUI** already works with OBVR. Its HUD names are checked in step 1.
- **An element OBVR cannot find** stays in the main panel. A UI mod can never make an element disappear.

## 6. Risks and open questions

- **Thin bars:** vanilla bars are a few pixels wide, so cropping them onto a small hand panel may be blurry. Step 2 shows it. If so, those bars can be drawn by OBVR (route C) from the actor values.
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
