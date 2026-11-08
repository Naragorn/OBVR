#pragma once

// Lays a cut mark (game/CutMarkLogic.h) on an actor with the engine's own
// decal call: on its 3D, in its cell, the turn computed in the frame of the
// actor's root node (the engine builds its frame from the direction carried
// into the geometry's own space; the body's skinned shapes hang under that
// root). Main thread only, as every engine caller. False, with the reason
// in `why`, when nothing was laid.

#include "game/CutMarkLogic.h"

namespace obvr::game {

bool LayCutMark(UInt32 actor, const CutMark& mark, const NiPoint3& cutWorld, char* why, UInt32 whySize);

// How many decal shapes ("Decal:") hang under the actor's 3D now - a blood
// stain or a cut mark that landed on the body adds one; 0 with no 3D.
UInt32 DecalsUnder(UInt32 actor);

}  // namespace obvr::game
