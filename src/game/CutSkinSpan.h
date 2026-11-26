#pragma once

// The engine side of game/CutSkeletonLogic.h's span: whether a skinned shape
// taken whole to a limb's part would stretch between the part and the body.

#include "core/Types.h"
#include "game/CutSkeletonLogic.h"

namespace obvr::game {

// `geometry`'s skin read where the engine keeps it (NiGeometry +0xB4 its data,
// the vertex count a UInt16 at +0x08; +0xB8 its NiSkinInstance, +0x08 the
// NiSkinData: the bone count at +0x40, the bones' data at +0x44, 0x4C each,
// the weights at +0x44 and their count, a UInt16, at +0x48 - game/BodyView.cpp
// has where each was read), nothing copied. `flies` holds one flag a bone in
// the skin's order, `boneCount` of them - the skin's own count. Spans with
// `spanning` the vertices on both sides; None; Unread when any of it does not
// read or the counts disagree.
SpanRead ShapeSpansCut(UInt32 geometry, const bool* flies, UInt32 boneCount, UInt32& spanning);

}  // namespace obvr::game
