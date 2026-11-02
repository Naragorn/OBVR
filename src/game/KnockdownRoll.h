#pragma once

#include "core/Types.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

// Vanilla's chance on a knockdown (game/ImpactLogic.h): with `vanilla` a
// Hard blow on `actor` stands with the engine's own chance - the player's
// fist damage (a power blow) against the actor's Agility, Luck and fatigue,
// through the game's settings as it holds them now (each slot's name checked
// first; vanilla's values when one is not the one read) - and is a Light
// one otherwise. Anything else comes back as it went in. Logs each roll
// (a budget of lines).
ShoveKind RollKnockdown(void* actor, ShoveKind kind, bool vanilla);

}  // namespace obvr::game
