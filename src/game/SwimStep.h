#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The swimmer's velocity the arms ask of the player's Move (vr/SwimStroke.h,
// docs/swimming-spec.md). Read in Oblivion.exe 1.2.416 (2026-11-27):
//
// - The player's walk is UpdateMovement 0x0063C730 handing a step (x right,
//   y forward, z, the actor's frame, game units a frame) to Actor::Move
//   (vtable 0x1B4, 0x005EFAE0) whatever the character controller's state;
//   OBVR's wrap of it (game/RoomFollow.cpp) adds the room's step and turns
//   the stick's. The strokes' step goes in there too: through the character
//   controller, so the water's surface, the shore and the walls stay the
//   game's.
// - 0x005E3920 is the swim walk speed (and 0x005E3AD0 the swim run's): thiscall,
//   no arguments, the float in ST0, plain `ret`, its first bytes the same
//   `83 EC 10 56 8B F1 8B 06` as the walk and run speed getters the analog walk
//   calls (0x005E3590, 0x005E3750), the body the speed formula over Speed,
//   encumbrance and Athletics. GetCurrentSpeed 0x005E65B0 picks it by the movement
//   flag 0x800.

// Each frame from the hands' step: the velocity wanted, world units a second
// (zero: none). It holds for kSwimRequestSeconds of Move time unless asked
// again, so a hands step that does not run lets it die.
void RequestSwimVelocity(const NiPoint3& worldUnitsPerSecond);

// From the wrap of the player's Move: adds the strokes' step for `dt`
// seconds to `step` (x right, y forward, z; the actor `self`'s frame), cut
// together with what is there to the longer of the two (vr::CombineSwimStep).
// False, step untouched, when no stroke is asked.
bool AddSwimStep(const void* self, float dt, float* step);

// The game's swim walk speed for the player, game units a second; 0 when the
// getter's bytes are not the ones read or there is no player.
float PlayerSwimSpeed();

// The player's heading (rotZ, radians), false without a player.
bool PlayerHeadingRadians(float& rotZ);

}  // namespace obvr::game
