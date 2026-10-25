#pragma once

#include "core/Types.h"
#include "game/EnemyHealthLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The fighters' health, read for the bars (EnemyHealthLogic.h): the process
// manager's high list (the combat ring's walk) for every Character or
// Creature, not the player, alive and fighting the player (game::
// ActorFightsPlayer) and within `rangeMetres` of `eyes` - and for every
// actor in `keep` (the bars' own, kept for the linger) whatever they do:
// their health now against their base (GetActorValue_F, vtable +0x288, actor
// value 8, and GetBaseActorValue 0x005F1910, as the fatigue is read), and
// the bar's point `aboveUnits` over their own top (EnemyHealthAnchorOf): the
// head bone ("Bip01 Head" through the bones BladeBodies keeps for them),
// else the highest of those other bones, else their model's bound, else
// their feet - and the point the eyes' ray looks at. Answers how many were
// read; stops at `capacity`.
UInt32 ReadFightersHealth(const NiPoint3& eyes, float unitsPerMetre, float rangeMetres, float aboveUnits,
                          const UInt32* keep, UInt32 keepCount, EnemyHealthSeen* out, UInt32 capacity);

}  // namespace obvr::game
