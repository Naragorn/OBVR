#pragma once

// The engine's loaded actors, for the hand script's "HandScript: actors"
// line (game/ActorRosterLogic.h): every Character or Creature on the actor
// process manager's four lists (high to low, GameAddresses.h
// kActorListByLevel) that have a model in the world (kRefNiNodeOffset), the
// player left out, each with its reference and base
// form IDs, whether it is dead (Actor's IsDead virtual, as MeleeHits reads
// it) and its distance from the player.

#include "core/Types.h"
#include "game/ActorRosterLogic.h"

namespace obvr::game {

// Fills `out` with up to `most` actors; returns how many. Nothing without a
// player.
UInt32 CollectLoadedActors(RosterActor* out, UInt32 most);

}  // namespace obvr::game
