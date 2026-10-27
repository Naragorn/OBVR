---
type: architecture
title: Menus, HUD and settings UI
description: How Oblivion's 2D layer (HUD, menus, dialogues, loading screens) is redirected into an OBVR texture and shown as an OpenVR overlay, how parts of the HUD are lifted onto the hands, and how OBVR's own settings menu is modelled and drawn.
tags: [ui, hud, menus, overlay, settings]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-27T21:08:40.633Z
sources:
  - id: openwiki-source-1826578918d285166d0617b3
    resource: repo://src/game/GameAddresses.h
  - id: openwiki-source-908303129debe8436a6d6683
    resource: repo://src/render/HandHudLayer.h
  - id: openwiki-source-33f83f0ec1cf4fb8eda41939
    resource: repo://src/render/HudLayer.h
  - id: openwiki-source-863255273cb120ba7b7797bb
    resource: repo://src/render/InterfaceRenderHook.h
  - id: openwiki-source-d63e09680ef5a4637ba09676
    resource: repo://src/ui/SettingsMenu.h
  - id: openwiki-source-498aa85a45062306c1a8e1ee
    resource: repo://src/ui/SettingsMenuLayer.h
  - id: openwiki-source-9fc68366e3d57bae7a6d5cb3
    resource: repo://src/vr/HandHud.h
generated: { by: "claude-code", at: "2026-10-27T21:08:40.633Z" }
---

# Menus, HUD and settings UI

Oblivion draws its HUD, menus, dialogues and loading screens as one 2D layer into the back buffer after the world. Once mixed into the back buffer it cannot be separated again, so OBVR intercepts the pass that draws it and points it at a texture of its own, which is then hung in the headset as a compositor overlay. The background and the failed alternatives are in `docs/vr-modding/ui-and-hud.md`.

## Redirecting the 2D pass

- **The pass:** `addr::kRenderInterface` = `0x0057F170` (Oblivion 1.2.0.416), the single function every route to the 2D layer funnels through.
- **Two hooks working as one** (`src/render/InterfaceRenderHook.h`): the pass begins the default render-target group from inside itself, so a wrapper cannot set a target first. The entry detour only raises a flag and asks a callback (`InterfaceRedirect::beginRedirect`) for a surface; the substitution happens in the device's `SetRenderTarget`, which answers index 0 with OBVR's surface while the flag is up and restores the last requested target when the pass returns. The depth-stencil surface is never touched. A null surface from the callback leaves the pass alone (menu frames without a compositor, feature off).
- **Alpha:** the back buffer has no alpha. `render::HudLayer::BeginCapture` creates an A8R8G8B8 texture at the frame size, clears it once per frame (not per call: the pass can run more than once per frame and per-call clears wiped earlier draws), and switches the alpha side of blending to a correct over-operator; `EndCapture` restores the states.
- **Loading screens:** while the loading thread is alive the engine skips the 2D pass, which is what "no 2D pass at all" looks like from outside (see `GameAddresses.h` near `kRenderInterface`).

The texture is the game's frame size because that is the layout space the game draws in, cursor included.

## HUD on the hands

`render::HandHudLayer` lifts selected HUD elements out of the captured interface into one atlas texture, erases them from the main capture so they do not show twice, and places each on its own overlay quad — relative to a controller, or absolute in the room. `Submit` hides them all when there is no world (a menu up). The ten elements are listed in `vr::HudElement` (`src/vr/HandHud.h`): bars, spell, weapon, effects, level-up, compass, region, messages, subtitles and the crosshair info; each is configured by `[HandHud] <Stem>Place`, `<Stem>Opacity` and `<Stem>Size`. Rationale and layout: `docs/hud-on-hands-spec.md`.

## Pointing at menus

In Full VR the laser from one hand walks the game's cursor over menus, and the trigger clicks (`vr::HandMode::PointAtMenu`, `[Hands] LaserCursorDirect`). The cursor hit-test under the believed viewport is patched in `src/render/CursorPickHook.cpp`. Menu-stack ownership for the personal Tab pages is tracked by `game::vrbridge` (`src/game/VRMenuBridge.h`), which reads the interface manager's visible stack rather than cursor observations.

## OBVR's own settings menu

Three layers, split so the logic is testable:

| Layer | File | Owns |
| --- | --- | --- |
| Table | `src/ui/SettingsList.cpp` | which settings exist, their kinds and ranges |
| Model | `src/ui/SettingsMenu.h` / `.cpp` | open/closed, highlight, scroll, the repaint revision; no Direct3D, no Windows |
| Layer | `src/ui/SettingsMenuLayer.cpp` | a DYNAMIC lockable texture repainted only when the revision changes, shown as an overlay |

Rows are rebuilt from the live configuration every time they are asked for, so a value changed by the INI hot reload shows up instead of a stale copy being written back. A change is reported, not written, by the model; the caller writes it with `SaveSetting(section, key, value)` (see [Configuration](../operations/configuration.md)). In Full VR the sticks steer the menu while it is open and nothing else fires.

Other painters in `src/ui/` (quick menu ring, holster signs, stow spots, enemy health, guide panel, onboarding) draw through the same canvas/overlay pieces (`MenuCanvas`, `CanvasOverlay`).

## Tests

Unit tests cover the pure parts: `tests/SettingsMenuTest.cpp`, `SettingsListTest.cpp`, `MenuPainterTest.cpp`, `QuickMenuPainterTest.cpp`, `VRMenuBridgeTest.cpp`. Overlay placement and the redirected pass need the game and are checked by [hand-script harness](../testing/hand-script-harness.md) scenarios.
