#pragma once

#include "core/Types.h"

namespace obvr::game {

// How a knockdown falls (Nadi, 2026-11-05: the people sink into themselves
// instead of falling over, also when a thrown thing knocks them down). A
// probe for [Debug] ShoveTrace: from the knockdown on, the pelvis's and the
// head's place against where they stood, logged at 0, 0.5, 1, 2 and 4 seconds -
// a fall over shows the pelvis carried away and the head down beside it, a
// sink the pelvis straight down under the head. A creature without those
// two bones (a rat) is followed by its root node instead: a tester,
// 2026-11-06, "shield bashing sometimes leads to small enemies like rats
// clipping through the floor" - its root going down past where it stood.

// Starts following `actor` (the last knockdown replaces one followed).
// `sourceOverMiddle` is how far the knock's source stood over their middle,
// in units, for the line.
void WatchKnockdown(UInt32 actor, float force, float sourceOverMiddle);

// One frame: logs the due marks; nothing once the watch is done or the
// reference is gone.
void StepKnockWatch(float dtSeconds, bool enabled);

}  // namespace obvr::game
