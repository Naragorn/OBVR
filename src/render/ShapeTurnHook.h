#pragma once

// Which way round the shape being drawn is turned (render/BackfacePass.h
// ShapeTurnOf), for the left-handed mirror's cull. NiDX9Renderer's draw calls
// for one shape - slots 88 to 94 of its vtable 0x00A88EA4, each __thiscall
// with the geometry as the one argument and `ret 4` (read in Oblivion.exe
// 2026-11-23; 88 and 89 hold Do_RenderShape 0x007693E0, see
// render/DrawnShapes.h; 92, 0x00765990, draws a torch's flame) - note the
// shape's world turn for the draws they make. Under the mirror only a
// turned-over shape culls the other way; the torch's flame is upright and went
// out while the cull swapped for every first-person draw (free-weapons-spec
// 5.16).

#include "render/BackfacePass.h"
#include "render/FirstPersonPass.h"

namespace obvr {
struct Config;
}

namespace obvr::render {

void InstallShapeTurnHook();
// The first-person effects' hooks together: the billboards' time
// (game/BillboardTime.h), the particles after the pins
// (game/ParticlesAfterPins.h) and this one.
void InstallFirstPersonEffectHooks(const Config& config);
// CurrentShapeTurn (render/FirstPersonPass.h): the shape being drawn now;
// Unknown outside the draw calls. Called under the mirror in the
// first-person pass: the first draws of each slot are traced.
// Whether a renderer slot holds this hook (render/DrawnShapes.cpp chains it).
bool IsShapeTurnHook(UInt32 function);

}  // namespace obvr::render
