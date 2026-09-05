# UI, HUD and menus

How Oblivion's flat 2D layer reaches a headset, and what it cost to get it there.

## The shape of the problem

`CONFIRMED`, `Scope: Gamebryo` / `Scope: Legacy games`

Oblivion draws its HUD, menus, dialogues and loading screens as one 2D layer into the same
back buffer as the world, after it, through an orthographic projection. By the time a
mod sees the back buffer the pixels are mixed and alpha blending is not reversible, so
"snapshot before and after, take the difference" (route A) cannot recover the layer. The
route that works (route B) is to intercept the pass that draws the layer and point it at a
texture of the mod's own, complete with alpha, and hang that texture as a compositor
overlay.

## The 2D pass

`CONFIRMED`, `Scope: Oblivion`

- **One function draws the whole layer, everywhere**: `0x0057F170`, `__thiscall` on the
  InterfaceManager singleton, one argument (a rendered texture, null on the ordinary
  path). Four call sites in the binary; all wrappers deciding *when*. Found by walking the
  binary from Oblivion Reloaded's `kRenderInterface` call site and verified through
  addresses already proven (`0x00582160` singleton getter, `kSceneGraphCameraOffset`,
  `SetCameraViewProj`).
- **It begins the default render-target group from inside itself** (clear flags 6: depth
  and stencil, not colour - which is why menus sit on the world), so a wrapper cannot set
  a target first. The redirect happens at the *device*: while the pass runs,
  `SetRenderTarget(0, ...)` is answered with OBVR's surface (`InterfaceRenderHook.cpp`,
  `HookedSetRenderTarget`), and the last target the pass asked for is put back before it
  returns.
- **The pass is skipped by the engine while the loading thread lives**
  (`0x00B33434`, `GetExitCodeThread == STILL_ACTIVE`), which is what "no 2D pass at all"
  looks like from outside.

## Alpha

`CONFIRMED`, `Scope: DirectX 9`

The game's back buffer is X8R8G8B8 (no alpha), so the engine may run with alpha writes
off and blends `SRCALPHA/INVSRCALPHA` without separate alpha blending. Into an A8R8G8B8
texture that gives destination alpha of alpha squared - close, monotone, too transparent -
or alpha zero if the write mask lacks the bit. `HudLayer::BeginCapture` saves five states,
sets `D3DRS_SEPARATEALPHABLENDENABLE` with `ONE/INVSRCALPHA` on the alpha side and the
colour write mask including alpha, clears the texture to transparent black **once per
frame** (not per call: the pass runs more than once per frame and clearing per call wiped
the earlier draws), and `EndCapture` restores. The `SetRenderState` hook keeps the alpha
bit in any mask the pass sets midway.

## The 2D pass and the dual pass

`CONFIRMED` as behaviour, cause `UNKNOWN`

With the world rendered twice, the game's own 2D pass is entered once per frame on the
same thread with the loading handle null, walks past every open gate, and **draws
nothing** (measured with the `DualPassProbe` sweep: 21-22 primitives with one render,
zero with two; cutting the second render back off restores it within the run, so the
state is per frame). The disassembly of the interface manager's gates did not explain it.

Worked around, not explained: `HudBetweenPasses=1` runs the pass **between the two world
renders**, redirected, through the same detour path (`RunHudPassBetweenScenes`), where it
demonstrably draws; the game's later pass is left alone and the redirect declines it so
it cannot clear the texture. This is also the moment the overlay wants the layer: after
the first eye is captured (so the layer never lands in an eye picture), before the camera
moves (so it is drawn from the viewpoint the game computed).

Open question: why the second render leaves the pass unable to draw. See
[open-questions-and-known-issues.md](open-questions-and-known-issues.md).

## Where the layer hangs

`CONFIRMED`

- The overlay `obvr.hud` (`IVROverlay_028`, fetched on first use; `SetOverlayTexture`
  takes the same `Texture_t` with `TextureType_Vulkan` as the eyes) shows the layer
  texture through the same `InteropBracket` as the eyes.
- `HudAnchor=head`: head-relative, `HudDistanceMetres` ahead, `HudWidthMetres` wide - a
  cockpit HUD. `HudAnchor=world` (shipped): placed in the seated tracking space from a
  levelled head pose taken when the layer first appears and again on the recenter key, so
  the wearer can look away from it; the stick does not move it (the head has not moved in
  the room), and an anchor further away than a person leans is retaken (self-heal).
- `SetOverlayTextureBounds` crops the overlay to the 2D's believed size when the texture is
  bigger than the picture (eye-sized frame), which also makes the quad 16:9 again.
- The overlay is submitted only when this frame captured something; submitting an empty
  capture hides the overlay, and on held menu frames that blinked the menu.

## Menu delivery: cinema, world, held, live

`CONFIRMED` (cinema, world, held), `EXPERIMENTAL` (live)

`camera::DeliverFrame` (`FrameLogic.h`) decides per frame from four booleans and a streak:

| Frame | `Menus=cinema` | `Menus=world` |
| --- | --- | --- |
| World drawn, no menu | Stereo | Stereo |
| No world drawn, no menu (film, loading, main menu) | Cinema | Cinema (a quad in an empty room is worse than a screen) |
| Menu over a world the engine is still drawing (dialogue, persuasion) | Cinema | Stereo |
| Menu, world not redrawn this frame | Cinema | HeldStereo, or Cinema with nothing held yet |

- **Cinema**: the whole back buffer, menu included, letterboxed into both eye textures at
  `MenuScale` and `MenuAspect` (16:9 as shipped), hung on a held pose levelled to its
  heading (pitch and roll removed, `vr::LevelPose`), so it hangs at eye level like a
  screen on a wall. A flat picture is legible in a way a quad at a fixed distance is not; reading an inventory
  is what menus are for. `MenuAspect` and `MenuScale` were the answer to menus squeezed
  into a square (the height was once derived from the world's angular rectangle).
- **HeldStereo**: the last captured pair submitted again with the pose it was drawn from,
  so the compositor reprojects it. Not stale: the world is paused. The old menu flicker
  was exactly the alternative - stereo and cinema alternating at frame rate because "is a
  menu up" was guessed from whether the camera hook ran (some frames redrew the world,
  some did not, was the belief; the measurement later said the engine does not redraw at
  all behind a pause menu). `IsMenuMode` (`0x00578F60`) answers the question now.
- **The closing seam**: the frame that closes a menu has no camera pass and no menu and
  is Cinema by every rule; it flashed the flat picture and, worse, filled the eye copies
  letterboxed so the *next* menu held black bars. `worldlessStreak` bridges up to three
  such frames with the held pair; a longer streak is a real flat presentation (a film).
- **Dressing**: pause menus get vanilla's sepia (the `MenuShade` shader) and a single
  common border (each eye trimmed to the window both show, so the frozen pair's edges
  fuse); a dialogue's exit fade, minutes after the DialogMenu opened, must not be dressed
  (it read as a half-second washed-grey picture), hence the ten-frame age gate and the
  dialogue-episode latch (`MenuDressingWanted`, `DialogMenuEpisode`).
- **Live** (`LiveMenuBackground=1`, shipped, `EXPERIMENTAL`): the engine's own static
  menu background is switched off through the process-local copy at `0x00B33396`, the
  engine renders the world behind every pause menu itself, and `MenuStandIn` supplies the
  VR camera from inside the scene render on those frames. Fresh stereo pair per frame,
  sepia, no border trim. The earlier shelved attempt (`498a1e7`) rendered from Present and
  drew nothing; see [failed-approaches.md](failed-approaches.md).
- **Videos and loading screens are excluded by measurement** ("did a camera pass run"),
  not by a list, so a mod that adds a menu needs no maintenance. Loading screens in
  cinema mode get the sepia shade too, because the live background disabled the engine's.
- **Mirror to monitor** (`MirrorMenusToMonitor=1`): the captured layer is blended back
  onto the back buffer in the Present hook, after the eyes were copied, so the desk stays
  usable when the headset misbehaves; the headset never sees it.
- **Unpaused menus** (`UnpausedMenus=0`): seven of the update step's fourteen `IsMenuMode`
  calls are redirected to `WorldPauseForMenu`, which keeps the world running only behind
  the player's own menus (F1-F4, container, book, quantity, magic popup) and never behind
  anything opened from a conversation (Real Time Menus needed two more hooks to stop the
  speaker walking off). `StablePauseMenuId` holds the menu id across the blank readings
  `GetTopVisibleMenuID` returns mid-stack. One existing plugin's import thunk to
  `IsMenuMode` is recognised and only that call redirected (`IsAbsoluteJumpTo`).

## Menu identification

`CONFIRMED`, `Scope: Oblivion`

- `IsMenuMode` at `0x00578F60`: nullary, returns a byte, reaches the interface manager
  three times. The main menu, loading, inventory, Esc, dialogue all count.
- Which menu: `ActiveMenuId` reads `InterfaceManager+0x9C -> Menu+0x20`; **null means the
  mouse is not over a menu, not "no menu"** (xOBSE's own note). Ids are xOBSE's enum from
  `0x3E9` (`MenuType.h`, pinned by static asserts against two measured points: SleepWait
  `0x3F4`, HUDInfo `0x3ED`). The F1-F4 menus stand in the active-menu stack as `1`.
- `GetTopVisibleMenuID` at `0x0057CF60` for the pause policy.
- The loading thread handle at `0x00B33434` is the reliable half of loading-screen
  detection (LoadingMenu is not always the active menu: no cursor to hover it).
- The tile menu array at `0x00B13970` (xOBSE's `g_TileMenuArray`) is indexed by menu type
  and is the second route to any menu by id; the crosshair uses it as a check on the
  direct `HUDInfoMenu` pointer.

## The crosshair

`CONFIRMED`, `Scope: Oblivion`

What hangs on the depth quad is **the game's own crosshair**, lifted out of the captured
layer and erased where it came from - never a drawn substitute (a custom third-person
fallback was built and removed, `b0cd3d9`).

- A square of `CrosshairSourceShare` percent of the believed height (a share, not pixels:
  the interface scales with the frame, and 96 pixels covered less and less) is copied out
  of the centre of the layer texture into the crosshair layer's texture and cleared in the
  layer (`CrosshairCaptureWanted` runs whenever the feature is on, so the original never
  shows through at HUD distance).
- The quad is placed at `CrosshairDepth` metres with a width of
  `CrosshairSizeAtOneMetre * distance` so its apparent size is constant; limits 0.3..100 m.
- **Third person**: vanilla draws no crosshair there (Bethesda's support page says so) but
  does draw the context icons and the sneak eye. OBVR makes only its isolated HUD pass see
  first person (`RunHudPassWithCrosshairView`, the reticle update wrapped to skip its
  third-person fade-out), lifts the genuine pixels, and keeps a clean first-person
  capture in `OBVR-crosshair.cache` (header + checksum) as the fallback across starts.
  The borrowed copy stands down whenever the game draws something of its own (a target
  icon, the sneak eye): `BorrowedCrosshairWanted`.
- **Only when needed** (both views, shipped on): shown while something activatable is
  under it or a weapon/spell is readied. Nothing is aimed while a menu is up, and a
  dialogue under `Menus=world` is a menu over a live world - the crosshair stayed up
  through every conversation once (`CrosshairWanted` asks `menuIsUp` separately).
- Tooltips (the action icon) can be kept on the quad or moved above the target name in
  third person (`CrosshairTooltipsAboveName`, off).
- The depth: [camera-tracking-and-aiming.md](camera-tracking-and-aiming.md#the-crosshairs-depth).

## The believed screen size, the cursor and the click

`CONFIRMED`, `Scope: Oblivion`; the shape `Scope: Legacy games`

The 2D lays out against a screen size the engine copied once from its `iSize` settings
at window creation (`0x00B06C4C/50`), normalising to a height of 960 (UESP's description,
found at `0x57D7A0/F0` by the unique 960.0f/1280.0f constants). The mouse is clamped
against the same copy. The pick that decides the hover highlight (`0x00581390` ->
`0x70D300` -> `0x00701540`) normalises the cursor pixels by the **renderer's** width and
height getters, not by the copy - so with the copy raised to the frame and the renderer
believing the frame, the two agreed; with them apart the hover sat half the height
difference below the sprite (the "cursor ladder" measurement). The two cursor detours make
the search run under the believed viewport and divide by the believed size.

A dead end here worth knowing: the cursor-movement function's renderer getter
(`0x403190`) returns zero for all three axes in vanilla; redirecting it to the copy
armed an inert term and walked the cursor off screen (`4582c12`).

## OBVR's own menus

`CONFIRMED` (settings menu in a headset), `EXPERIMENTAL` (walkthrough)

- Drawn by OBVR: an own bitmap font (`ui/MenuFont`), a canvas (`ui/MenuCanvas`), a painter
  and a model; no engine UI involved, so they work at the main menu and during films.
- The settings menu (`Insert`, `[SettingsMenu]`) rebuilds its rows from the live config
  every time, applies a change to the config and reports the setting; the caller writes
  it to the INI through the Windows INI API (`SaveChangedSetting` in `CameraHook.cpp`),
  which the hot reload then picks up like any edit.
- The first-start walkthrough (`ui/Onboarding`) is pages of the same rows: the choice
  between seated and standing (standing shown, refused), the comfort basics, "do not show
  again" writes `[Onboarding] ShowAtStart=0`.
- Keyboard arrows drive both; with `ControllerMenus` the sticks and buttons do.
- Shown on their own overlay layer (`ui/SettingsMenuLayer`), anchored like the HUD.

## Screen-space assumptions that break in VR, listed

`Scope: General VR`

- A crosshair at HUD depth is seen double against anything at another depth.
- A layer in the back buffer cannot be separated afterwards; intercept its pass.
- "Is a menu up" must be asked of the game, not inferred from what rendered.
- A pause menu may stop the world render entirely (snapshot) or keep it running
  (dialogue); both need handling, and a held reprojected pair is the cheap answer to the
  first.
- The 2D coordinate space (layout, mouse clamp, hit-test normalisation) can be three
  different numbers once the frame is not the size the game chose.
- A levelled anchor for flat pictures needs a decision about pitch, and OBVR changed
  its mind twice (`HANDOFF.md`, "Levelling an anchor took the pitch out with the roll" and
  "The anchor keeps the heading and nothing else"). Current state: every anchor (flat
  picture, HUD room anchor, settings menu) uses `vr::LevelPose`, heading only, so the
  picture hangs at eye level however the head was tilted; `LevelRollOnly` (keeps pitch)
  exists in `OpenVRBackend.h` and is not called anywhere. The reason recorded for yaw-only:
  the same key recenters the world camera without changing how high the wearer looks, and
  one key should not mean two things depending on whether a menu is open. The cost: a
  picture above the line of sight when the key is pressed looking down.
