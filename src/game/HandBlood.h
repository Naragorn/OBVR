#pragma once

// Blood on the player's own hands while disguised in a walker's blood
// (vr/WalkersDisguise.h): the engine's blood decal (game/Blood.h
// BloodDecalTexture, game/CutMarks.h LayCutMarkOn) laid at each hand on the
// player's third-person body and on the first-person arms, looked down on
// from above the hand. The engine's decals fade after fDecalLifetime, so the
// frame lays them again while the disguise lasts (camera/hands/WalkersDisguise.h).

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// One stain at each valid hand (0 left, 1 right; world units) on both of the
// player's roots. Returns the decals the roots gained.
UInt32 StainPlayerHands(const NiPoint3 hands[2], const bool valid[2]);

}  // namespace obvr::game
