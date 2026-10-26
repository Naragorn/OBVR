#pragma once

// The actors left out of the fighters' ring and of the enemy attack tempo
// ([Hands] CombatRingExceptions, AttackTempoExceptions;
// game/ActorExceptionsLogic.h has the matching).

#include "core/Types.h"

namespace obvr::game {

enum class ExceptionList : UInt8 { CombatRing, AttackTempo };

// The setting's text, read again only when it changed.
void SetActorExceptions(ExceptionList which, const char* setting);

// Whether the actor (a Character or Creature reference) is on the list: its
// base's name, or a person's race's name. Remembered per actor and base form
// until the list changes.
bool ActorOnExceptionList(ExceptionList which, UInt32 actor);

}  // namespace obvr::game
