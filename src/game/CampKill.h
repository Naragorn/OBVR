#pragma once

// A walker killed by a camp's refugee loses its head (game/CampKillLogic.h).

namespace obvr::game {

// Once a frame: each walker Walkers named in 'WGCT' since the last frame has
// its head cut as at the player's own cut, when `enabled` ([Hands]
// Dismemberment), it is loaded and dead. Logs each.
void StepWalkersCampKills(bool enabled);

}  // namespace obvr::game
