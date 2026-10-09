#pragma once

#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/ParryLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The living near the blade as capsules on their drawn bones
// (BladeContactLogic.h, "the living"; docs/weapon-collision-spec.md, 4 C):
// the actors of the process manager's high list (the strike by motion's own
// walk, 0x00673A50 on 0x00B3BD00), alive, not the player, whose bound comes
// within `withinUnits` of `around`. Their "Bip01" bones are found by name
// through the root's GetObject (vtable +0x58, as leading by the hand finds a
// hand) once per actor and root and kept, checked each frame by their names;
// a skeleton read with fewer than six of them is the bound's column instead.
// With `foes`, also the blade of each who has a weapon in hand (their
// "Weapon" node, kept with the bones; ParryLogic.h, TheirBladeFromNode),
// the frame before's with it, and their process action. Answers how many
// people were taken.
UInt32 CollectBladeBodies(const NiPoint3& around, float withinUnits, BladeBodies& out, BladeFoes* foes = nullptr);

// Forgets the bones kept (the mode off): a load builds new skeletons.
void ForgetBladeBodies();

}  // namespace obvr::game
