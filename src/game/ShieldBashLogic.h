#pragma once

// The shield bash (Nadi, 2026-10-31: "Schildstoß ... wenn nicht bauen wir
// den"; combat-comfort-spec proposal C, physical-combat-spec "Bashing"). Not
// vanilla: Oblivion's shield only blocks (UESP, Oblivion:Block). The off hand
// with a shield on its arm driven fast into someone - the shield's middle at
// their body, the hand's speed towards them - bashes instead of the punch or
// the slap an empty off hand gives: the engine's own fist blow for the damage
// (game::KickStrike, as the kick), then the stagger, or with a hard bash the
// knockdown, through the shove's push (game::ShoveActor); the shield's sound,
// a pulse, its fatigue. The stagger breaks off the attack they were in.
//
// Pure, covered by shield_bash_test.

#include "game/ShoveLogic.h"

namespace obvr::game {

struct ShieldBashSettings {
	bool enabled = true;     // [Hands] ShieldBash
	float speed = 3.0f;      // m/s towards them: a bash, they stagger (ImpactSettings; vr::ApplyImpact)
	float hardSpeed = 6.0f;  // m/s: a hard bash, they go down
	float fatigue = 15.0f;   // the player's, a bash
	float hardFatigue = 35.0f;
};

struct ShieldBashHand {
	bool shieldWorn = false;   // a shield on the off hand's arm
	bool valid = false;        // the hand tracked
	bool holdsSomething = false;  // the off hand on a handle, holding someone or an object
	float towardsSpeed = 0.0f; // m/s along the line to them
};

inline ShoveKind ShieldBashFor(const ShieldBashSettings& s, const ShieldBashHand& h) {
	if (!s.enabled || !h.shieldWorn || !h.valid || h.holdsSomething || !(h.towardsSpeed >= s.speed)) {
		return ShoveKind::None;
	}
	return h.towardsSpeed >= s.hardSpeed ? ShoveKind::Hard : ShoveKind::Light;
}

// The shove's rules as the bash takes them: always told as a hit (a bash is
// an attack), unless the engine's own blow landed first and told them itself;
// no fatigue of the shove's - the bash pays its own (ShieldBashFatigue).
inline ShoveSettings BashShoveSettings(const ShoveSettings& shove, bool engineHit) {
	ShoveSettings b = shove;
	b.countsAsHit = !engineHit;
	b.lightCountsAsHit = !engineHit;
	b.fatigueLight = 0.0f;
	b.fatigueHard = 0.0f;
	return b;
}

inline float ShieldBashFatigue(const ShieldBashSettings& s, ShoveKind kind) {
	if (kind == ShoveKind::Hard) {
		return s.hardFatigue > 0.0f ? s.hardFatigue : 0.0f;
	}
	if (kind == ShoveKind::Light) {
		return s.fatigue > 0.0f ? s.fatigue : 0.0f;
	}
	return 0.0f;
}

}  // namespace obvr::game
