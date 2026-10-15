#pragma once

#include "core/Types.h"

namespace obvr::game {

// The engine's knockback on a landed hit, for the player's hits on others
// ([Hands] EnemyKnockback, EnemyKnockbackPowerOnly; the tester, 2026-10-15:
// "problematisch wird auch ob wir dann gegner wegschieben mit schlägen. das
// wäre ein fehler", then "bau mal alle vier optionen mit toggle"). Every hit
// the game lands shoves the one hit through their Havok character proxy
// (PlayerStagger.h: 0x0060008A fetches the proxy, 0x006000AF pushes it by
// fKnockback*) - about 19 units from a light sword hit in the harness, out
// of the next blow's reach. In Blade & Sorcery a body stays where it stands
// and only the part struck takes the impulse (weapon-collision-spec 17.1).
struct HitKnockbackSettings {
	// The share of the game's push the player's hits give the one hit, per
	// cent: 100 the game's own, 0 none - they stay, as in B&S.
	float percent = 0.0f;
	// Only a power attack pushes, by the percent; a light hit pushes nobody.
	bool powerOnly = false;
};

// The share (0..1) of the engine's knockback this hit keeps. Only the
// player's hits on others are the settings' business: a hit on the player
// is [Look] NoPlayerStagger's, and another fighter's hit the game's own -
// both keep the whole push (1). A percent that is not a number is 100; one
// out of range is held to 0..100.
inline float HitKnockbackShare(const HitKnockbackSettings& s, bool attackerIsPlayer, bool targetIsPlayer,
                               bool powerAttack) {
	if (!attackerIsPlayer || targetIsPlayer) {
		return 1.0f;
	}
	if (s.powerOnly && !powerAttack) {
		return 0.0f;
	}
	float percent = s.percent == s.percent ? s.percent : 100.0f;
	percent = percent < 0.0f ? 0.0f : (percent > 100.0f ? 100.0f : percent);
	return percent / 100.0f;
}

}  // namespace obvr::game
