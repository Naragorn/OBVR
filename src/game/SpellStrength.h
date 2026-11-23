#pragma once

#include "core/Types.h"

namespace obvr::game {

// A charged spell's strength (vr/SpellCharge.h SpellChargeFactor): its effects' magnitudes scaled by
// the stage it was let go at, wherever the engine scales them by the caster's spell effectiveness
// (read 2026-11-23 in Oblivion.exe 1.2.0.416):
//
// - At the release (0x0069AF30, the self and touch effects): each effect made for the item gets the
//   caster's effectiveness through 0x0068E170 (ActiveEffect, thiscall(float), ret 4: magnitude +0x18
//   or duration +0x1C times it), called at 0x0069B466. While a charged release is made (ArmCharged)
//   that call gets the factor on top.
// - A bolt (target range) is made inside that release, queued at 0x0069F6A7 (0x00673A90 on the
//   manager 0x00B3BD00, thiscall(projectile, 4 more), ret 0x14). While armed, the bolt is noted with
//   the factor; any other bolt queued at the same address clears it (a freed one's address reused).
// - Where a bolt reaches its target the caster applies the item (0x0069BA90, thiscall on the
//   caster, ten words, ret 0x28; called by the bolts at 0x00695617, 0x0069730A, 0x0069C5E9,
//   0x0069CD4A): its ninth word is the effectiveness multiplier, its sixth the bolt. A noted bolt's
//   multiplier gets its factor.
//
// Each call site is taken only when it holds the call as read; the log says which were.
void InstallSpellStrength();

// While the held release is made: its factor (1 = as it is).
void ArmChargedRelease(float factor);
void DisarmChargedRelease();

// What the player pays for `item` before a charge: the item's effect list's own cost (MagicItem +0x0C,
// EffectItemList's virtual 0, thiscall(Actor*) -> float; xOBSE GameForms.h) for the player. -1 when
// it cannot be read.
float PlayerSpellCost(const void* item);

// Takes `amount` more magicka from the player (actor value 9 through the actor's +0x2A4, as the
// attack fatigue is taken - game/GameAddresses.h kActorChangeFatigue).
void TakePlayerMagicka(float amount);

}  // namespace obvr::game
