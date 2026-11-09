#pragma once

// Blood on the player's own hands while disguised in a walker's blood
// (vr/WalkersDisguise.h): the engine's blood decal (game/Blood.h
// BloodDecalTexture, game/CutMarks.h LayCutMarkOn) laid on the first-person
// arms' hand shapes - the hands seen in Full VR - at each hand's middle
// (between its hand bone and its middle finger's), projected from one side
// at a time (vr::HandStainDirection: the eye's, then the hand's six). On the
// third-person body the shapes round the hands were the sword and its
// scabbard (walkers-disguise 20261109-153555), so it is not asked. The
// engine fades its decals after fDecalLifetime, so the frame lays them again
// while the disguise lasts (camera/hands/WalkersDisguise.h).

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// One stain at each wanted hand (0 left, 1 right) from `side`
// (vr::HandStainDirection), the eye at `eye` (world units). Returns the
// decals the arms gained.
UInt32 StainPlayerHands(const NiPoint3& eye, const bool want[2], UInt32 side);

}  // namespace obvr::game
