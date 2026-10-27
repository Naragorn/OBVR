---
type: architecture
title: Rendering and stereo
description: How OBVR renders Oblivion's world once per eye by calling the engine's own world render twice, keeps culling consistent between the passes, borrows DXVK's Vulkan images through the D3D9 interop and submits them to SteamVR, and how the submit policy protects the game's frame rate.
tags: [rendering, stereo, dxvk, openvr, d3d9, compositor]
verified:
  - by: openwiki/0.7.2
    at: 2026-10-27T21:08:40.633Z
sources:
  - id: openwiki-source-2b10fb6468f3695dacf90613
    resource: repo://OBVR.ini
  - id: openwiki-source-a66fb70ef9c23adff057a8e2
    resource: repo://src/core/Config.cpp
  - id: openwiki-source-ecccdc5ce661a2581310ce31
    resource: repo://src/render/CullingHook.h
  - id: openwiki-source-502c8e08f75433c13a102da0
    resource: repo://src/render/HeadsetRenderer.cpp
  - id: openwiki-source-5a3e40e49c5007f34458d83e
    resource: repo://src/render/InteropBracket.h
  - id: openwiki-source-614cb859e559018cf5db94c0
    resource: repo://src/render/PresentHook.h
  - id: openwiki-source-d723e440e82de6eff0076aac
    resource: repo://src/render/SceneRenderHook.cpp
  - id: openwiki-source-78798f3fbf9fadcf9f6ea538
    resource: repo://src/render/SceneRenderHook.h
  - id: openwiki-source-682c64ac8b81a634ecf8da40
    resource: repo://src/render/SubmitPolicy.h
generated: { by: "claude-code", at: "2026-10-27T21:08:40.633Z" }
---

# Rendering and stereo

Oblivion renders with Direct3D 9; OBVR expects that D3D9 to be **DXVK**, because SteamVR's submit takes a Vulkan image and DXVK can hand out the Vulkan image behind a D3D9 surface. The full history, measurements and failed approaches are in `docs/vr-modding/rendering-and-stereo.md` and `docs/vr-modding/failed-approaches.md`; this page describes the mechanism in the current code.

## Stereo modes

`[Render] Stereo` (parsed by `ReadStereoMode` in `src/core/Config.cpp`) selects one of:

| Value | `vr::StereoMode` | What it does |
| --- | --- | --- |
| `dual` / `dualpass` | `DualPass` | the world is rendered twice per game tick, once per eye — the shipped default in `OBVR.ini` |
| `aer` / `alternate` | `AlternateEyes` | alternate-eye rendering: each frame draws one eye; the fallback |
| `none` / `off` | `None` | mono |

An unknown value keeps the previous setting and logs it.

## Dual pass

`render::InstallSceneRenderHook` detours `addr::kRenderScene` (`0x0040C830`), the engine function that runs culling, both scene-graph passes, water and image-space shaders and leaves the finished world in the back buffer without the 2D layer. `HookedRenderScene` (`src/render/SceneRenderHook.cpp`) calls the original once per eye. Between the calls the camera node's local position is stepped by the eye baseline and the world transform is recomputed with the engine's own `UpdateSelectedDownwardPass` (`addr::kUpdateSelectedDownwardPass`, `0x00707370`).

This is safe because rendering does not advance the simulation: physics, animation and particles were advanced in the update step before the render; the second call only costs GPU time. Decisions about whether a frame gets a second pass and what happens between and after the passes are made by the caller through `ScenePassCallbacks`. The pre-render callback is also the latest moment to change bones (the first-person arms), because the engine's animation update overwrites anything set earlier in the camera hook.

**Culling:** `src/render/CullingHook` detours `NiCullingProcess::Process`. In a dual frame the first pass's camera positions are recorded and replayed while the second pass builds its visible sets, so both eyes see the same objects; the renderer itself keeps the second eye's camera.

## From the back buffer to the headset

- **Borrowing the frame** — `render::GameFrame` finds the back buffer, queries `ID3D9VkInteropTexture` for its Vulkan image, and makes a single-sample copy when the back buffer is multisampled (SteamVR refuses multisampled images with error 105).
- **Holding DXVK still** — `render::InteropBracket` is one owner for the six steps around a submit: flush outstanding commands, lock DXVK's submission queue (SteamVR schedules work on the same Vulkan queue), transition to `TRANSFER_SRC_OPTIMAL`, submit, transition back, release the queue. Every step has an undo that must run even on failure; a missing undo freezes the game rather than producing a wrong picture.
- **Submitting** — `render::HeadsetRenderer` submits both eyes with `kTextureTypeVulkan` through `vr::OpenVRBackend::SubmitEye`. The same interop query is used by the overlay layers (`HudLayer`, `HandHudLayer`, `CrosshairLayer`, `VignetteLayer`).
- **Present** — `render::InstallPresentHook` replaces the device's `Present` vtable entry with a thin hook that calls OBVR's frame-end callback and then the original unchanged. Present is the moment the frame exists, unlike the camera hook which runs before rendering.

`render::EyeTextures` is a separate D3D11 device with a generated test pattern, kept to separate "compositor is talked to wrongly" from "pixels come out of D3D9 wrongly".

## Submit policy

`WaitGetPoses` blocks until the compositor wants a frame, which keeps the game on the compositor's clock — but throttles to 10 Hz while another application has focus, which would drag Oblivion to ten frames a second. `render::SubmitPolicy` (`src/render/SubmitPolicy.cpp`, pure) reads the compositor's answers and decides `Continue`, `Skip` or `StopRendering`, giving up the picture rather than the frame rate after a run of consecutive recoverable failures.

## Related

- The 2D layer and overlays: [Menus, HUD and settings UI](menus-and-ui.md).
- Where the hooks are installed and how a frame flows: [Architecture overview](overview.md).
- Performance tools (`src/perf`, render sampler) and measurements: `docs/performance-spec.md`, `docs/performance-profiling.md`.
