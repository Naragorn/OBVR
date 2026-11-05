#pragma once

#include "core/Types.h"

namespace obvr::game {

// How a knockdown falls (Nadi, 2026-11-05: the people sink into themselves
// instead of falling over, also when a thrown thing knocks them down). A
// probe for [Debug] ShoveTrace: from the knockdown on, the pelvis's and the
// head's place against where they stood, logged at 0, 0.5, 1 and 2 seconds -
// a fall over shows the pelvis carried away and the head down beside it, a
// sink the pelvis straight down under the head.

// Starts following `actor` (the last knockdown replaces one followed).
void WatchKnockdown(UInt32 actor, float force);

// One frame: logs the due marks; nothing once the watch is done or the
// reference is gone.
void StepKnockWatch(float dtSeconds, bool enabled);

}  // namespace obvr::game
