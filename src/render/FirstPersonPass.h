#pragma once

#include "render/BackfacePass.h"

namespace obvr::render {

// Whether the engine's first-person pass is drawing right now: its byte on the
// renderer's accumulator (BackfacePass.h kAccumulatorFirstPersonOffset), read
// only inside the scene render (MayReadFirstPersonByte). The closed hands and
// the left-handed mirror ask it on every draw.
bool InFirstPersonPass();

// The shape being drawn now, which way round (render/ShapeTurnHook.cpp; its
// header has the rest).
ShapeTurn CurrentShapeTurn();

}  // namespace obvr::render
