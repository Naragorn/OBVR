#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The dead player's body drawn ahead of the held death view ([Look]
// DeathBodyAheadMetres, camera::DeathBodyAhead): the view stays where the
// living eyes were and the body is seen falling in front of it, instead of
// the view jolting back to show it (2026-09-26).
//
// Drawn there, not moved there: the body is a ragdoll, its bones placed by
// Havok, and running a node's own update puts a Havok-driven node back
// where its rigid body is (the held-object lesson, game::HeldObject). So for
// the draw only, every node of the player's skeleton (Bip01 down) has the
// offset added to its world translation and its world bound, and after the world
// render each gets back exactly what it had. Nothing the engine or Havok
// reads between frames ever sees the shift, and it cannot build up.
//
// Only the translation moves, the same offset for every node, so the pose is
// kept exactly; a skinned mesh follows its bones' world transforms.

// Before the world render, while the death view is held: shifts the tree by
// `offset` (game units, world). Nothing when `wanted` is false or the offset
// is zero. At most one shift is outstanding; a second call first restores.
void ShiftDeadPlayerBody(bool wanted, const NiPoint3& offset);

// After the world render: puts back what ShiftDeadPlayerBody changed. Safe
// with nothing shifted.
void RestoreDeadPlayerBody();

}  // namespace obvr::game
