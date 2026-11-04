#pragma once

// A walker's hand on the player's held arm, the engine's side
// (game/WalkerGripLogic.h for the decisions and why).

#include "game/WalkersLink.h"

namespace obvr::game {

// Each world frame, after the actors' animation and before the render: for
// each of the player's arms Walkers' 'WGRB' names a holder for, that holder's
// nearer arm is laid onto the player's forearm, eased in and out. The holder
// 0xFFFFFFFF ([Debug] TestWalkersHold, no walker) and an unknown reference
// are left alone. `enabled` is [Hands] WalkerGrip. While Walkers listens
// ('WGVR' taken) both forearms go to it as 'WGHP' ten times a second, so it
// grabs only with a holder already in reach.
void StepWalkerGrips(const WalkersNow& walkers, float dtSeconds, bool enabled);

}  // namespace obvr::game
