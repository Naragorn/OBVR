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
// One actor read directly, without the roster's cap (64 - a camp's attack
// loads 142, 2026-11-05): true when `actor` (a live pointer, from the form
// lookup) is a Character or Creature with its 3D in the world; `dead` its
// IsDead.
bool ActorInWorld(UInt32 actor, bool& dead);

UInt32 CollectLoadedActors(RosterActor* out, UInt32 most);

}  // namespace obvr::game
