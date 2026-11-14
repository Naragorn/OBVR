#pragma once

// What the hints in play read of the engine (vr/HintLogic.h): whether one
// fighting the player is lawful - a guard or an NPC of high responsibility,
// whom a yield is for - and the player's health against its base.

#include "core/Types.h"

namespace obvr::game {

// Whether this actor is one the yield's hint is for (vr::HintLawfulFoe): an
// NPC (its base form NPC_, a creature never accepts a yield) whose class has
// the guard flag (TESNPC +0x104 the class, TESClass +0x60 its flags, 0x2 the
// guard; xOBSE GameForms.h), or whose responsibility (actor value 0x24,
// xOBSE GameActorValues.h, read through vtable +0x288 as ModAV leaves it) is
// kHintLawfulResponsibility or more. False when anything does not read.
bool ActorIsLawful(UInt32 actor);

}  // namespace obvr::game
