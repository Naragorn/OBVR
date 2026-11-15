#pragma once

// The drawn weapon's shape read from its model ([Hands] FittedWeaponShapes,
// game/WeaponShapeLogic.h): every vertex under its node in the node's own
// space, through the local transforms under it (game/NearbyItems.h,
// ForEachVertexUnder), the shape fitted there. Once per
// weapon drawn - a few hundred vertices, on the game's thread.

#include "core/Types.h"
#include "game/WeaponShapeLogic.h"

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

// False, with the reason in `why`, when no shape reads or the fit refuses.
bool ReadWeaponShape(const NiAVObject* node, WeaponShape& out, const char** why);

}  // namespace obvr::game
