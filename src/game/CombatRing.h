#pragma once

#include "game/CombatRingLogic.h"

namespace obvr::game {

// The fighters' ring and N at a time, the engine side (CombatRingLogic.h has
// the decisions): once a frame the process manager's high list is read for
// those fighting the player - a Character or Creature in combat whose combat
// target (Actor vtable +0x338, 0x005E0AF0: the combat controller's target;
// xOBSE GameObjects.h GetCombatTarget) is the player - their distance and
// whether they are attacking (the process action, as the parry reads it);
// the turns are stepped, and the waiting ones' legs handed to the movement
// setters (game/SlowApproach.h SetRingLegs). Off: nobody's legs touched.
void StepCombatRing(const CombatRingSettings& settings, float dtSeconds, bool logState);

}  // namespace obvr::game
