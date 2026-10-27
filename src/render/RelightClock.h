#pragma once

#include "core/Types.h"

namespace obvr::render {

// A clock on the engine's per-geometry relight, ShadowSceneNode's
// 0x007C6100 (__cdecl: the geometry and a node override on the stack, the
// caller cleans - its last instruction is a plain `ret` after `add esp, 0xC`,
// and ecx is not read; read 2026-10-27, OBNG
// knowledge/shadow-light-list-remove.md): for
// a geometry whose world bound changed since its last registration, the
// engine detaches it from every light that lit it - a walk of each light's
// geometry list, the crowd's quadratic cost - and re-attaches it to every
// light in range. Skinned actors change their bound every frame, so the
// relight runs for every actor shape in every frame it is registered.
//
// Two readings: the calls, counted by scene pass on every call (one
// increment) and written on the "Dual draws" heartbeat as "Relight at scene
// call", so a log says whether OBVR's second eye pays the relight again; and
// the ticks inside the calls while a capture runs with the call clock on,
// marked at the pass's end as `relight` beside shape_render
// (SceneRenderHook.cpp, MarkCallTicks).
bool InstallRelightClock();

// The ticks spent inside the relight since the last take and the calls timed;
// both cleared by the take.
void TakeRelightTicks(UInt64& ticks, UInt32& timedCalls);

// The relights since the last take: in the first scene pass, in the second,
// and outside any pass (loading, the flat path); cleared by the take.
void TakeRelightCounts(UInt32& firstPass, UInt32& secondPass, UInt32& outside);

}  // namespace obvr::render
