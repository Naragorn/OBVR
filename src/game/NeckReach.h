#pragma once

// CollectNecks' rule (game/NeckReachLogic.h) with its lines in the log: what
// it left out, and whether the checking calls ever found it wrong.

#include "core/Types.h"
#include "game/NeckReachLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

NeckRule& TheNeckRule();

// An actor taken - its head is in reach; `leftOutByBound` as NoteNeckTaken's.
void NeckTaken(NeckRule& rule, UInt32 actor, const NiBound& bound, const NiPoint3& head, bool leftOutByBound);

// The rule's line when one is due.
void SayNeckRule(NeckRule& rule);

}  // namespace obvr::game
