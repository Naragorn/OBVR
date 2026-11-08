#pragma once

// The stab that sticks, in the game (game/BladeStickLogic.h decides;
// docs/blade-penetration-spec.md step 5): on a stab that went in, the
// fighter staggered and held where they stand with their blows stopped
// (game::HoldStill, game::SetHeldFoe - the grapple's standing hold), the
// shown blade pinned along its line; a corpse, a fighter killed with the
// blade in or a fast stab into the torso hung on the blade in their
// ragdoll (game/RagdollHold.h's drive). Each start and end said in the log
// ("Blade sticks:").

#include "game/BladeContactLogic.h"
#include "game/BladeEntry.h"

namespace obvr::game {

// After the entry's step (StepBladeEntryContact) on its verdict and the
// contact it judged: starts on a stab that went in, pins the blade (the
// verdict's pose) while it sticks.
void StepStuckBlade(BladeContactVerdict& v, const BladeContactFrame& f, const BladeEntryNow& n);

// Once a frame, before the blade's step: a stuck blade whose step did not
// run since (the weapon put away, the hands off) lets go and frees whom it
// held.
void WatchStuckBlade();

}  // namespace obvr::game
