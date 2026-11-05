#pragma once

// The weapons and torches thrown from the hand, handed to the throw noise
// (game/ThrowNoiseLogic.h) and the throw's hits on people (game/ThrowLogic.h)
// - thrown objects come to both from GrabPhysics' TakeJustThrown.

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// A dropped weapon or torch just sent off with `velocityUnits`: kept for the
// noise when that is a throw's speed (IsThrowSpeed).
void NoteSentOff(UInt32 ref, const NiPoint3& velocityUnits);

// The last one kept, once: its reference, then 0 until the next.
UInt32 TakeSentOff();

}  // namespace obvr::game
