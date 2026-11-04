#pragma once

// A walker's hand on the player's held arm, the engine's side
// (game/WalkerGripLogic.h for the decisions and why).

#include "game/NiMath.h"
#include "game/WalkersLink.h"

namespace obvr::game {

// Each world frame, after the actors' animation and before the render: for
// each of the player's arms Walkers' 'WGRB' names a holder for, that holder's
// nearer arm is laid onto the player's forearm, eased in and out. The holder
// 0xFFFFFFFF ([Debug] TestWalkersHold, no walker) and an unknown reference
// are left alone. `enabled` is [Hands] WalkerGrip. While Walkers listens
// ('WGVR' taken) both forearms go to it as 'WGHP' ten times a second, so it
// grabs only with a holder already in reach.
// The player's hands where the controllers are, in the world: the cyclopean
// camera times each controller's pose - the place the pins put the hands,
// read from the controllers rather than from the first-person bones (at the
// render those read 36 units off the controller, measured 2026-11-04 in
// walker-grip-real). Index 0 the left arm, 1 the right; forward is the
// controller's forward (its local +y) in the world, along the forearm.
struct HeldHands {
	bool valid[2] = {false, false};
	NiPoint3 pos[2]{};
	NiPoint3 forward[2]{};
};

void StepWalkerGrips(const WalkersNow& walkers, float dtSeconds, bool enabled, const HeldHands& hands);

}  // namespace obvr::game
