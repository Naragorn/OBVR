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
// `worldPaused` (game::WorldPausedForMenus): nothing steps and nothing is
// placed, the turns' clocks stand, the legs keep what they were given.
void StepCombatRing(const CombatRingSettings& settings, float dtSeconds, bool logState, bool worldPaused);

// How many fought the player at the ring's last step (0 with the ring off).
UInt32 CombatRingFighting();

// Whether this actor fights the player: in combat (Actor vtable +0x334, as
// the slow approach asks) with the player as the combat target (+0x338,
// 0x005E0AF0). False when either slot does not read. The ring's own test,
// for the health bars too (game/EnemyHealth.h).
bool ActorFightsPlayer(UInt32 actor, UInt32 player);

}  // namespace obvr::game
