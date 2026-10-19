#pragma once

#include "core/Types.h"

namespace obvr::render {

// A clock on the engine's per-shape renders (the two functions the shape
// dispatcher at 0x007693E0 calls: 0x00767520 from 0x007694BD and
// 0x007672F0 from 0x007694D7, `this` in ecx): everything the renderer does for one shape on the
// CPU - the shader's transformations and skinning, its constant maps with
// their uploads, the stream and index setup, the draw. Summed per scene pass
// while a capture runs and marked at the pass's end as `shape_render`, beside
// the D3D9 call buckets (InterfaceRenderHook.cpp, CallClock): the pass minus
// shape_render is the scene graph outside the shapes; shape_render minus the
// call buckets is the engine's own work per shape.
//
// Found 2026-10-19 by the bone probe: the bone rows are uploaded from
// 0x0078321D inside the constant map (0x009A5B35, 0x009A900A), called from
// the shader's pass setup (0x0077A2D3) inside this function (0x007676E6).
// The first calls of a run also log the shader's vtable slots the function
// calls (+0x34, +0x38, +0x3C), so those can be timed on their own next.
bool InstallShapeRenderClock();

// The ticks spent inside the per-shape render since the last take, and the
// number of calls; both cleared by the take.
void TakeShapeRenderTicks(UInt64& ticks, UInt32& calls);

}  // namespace obvr::render
