#pragma once

// The pommel strike (Nadi, 2026-11-01: "bau nun Rückstoß mit Knauf einer
// waffe"; free-weapons-spec 5.6 "man kann Gegner mit dem Waffenknauf hitten,
// Gegner kriegen Recoil", weapon-collision-spec "Foes recoil from a pommel
// strike"). The drawn weapon's pommel - the end of the handle below the hand -
// driven into someone, the pommel leading: they recoil. The engine's hit
// takes no place and no kind of damage (0x005FEBF0, free-weapons-spec 2.4),
// so the damage is the engine's own fist blow (game::KickStrike, as the kick
// and the shield bash), the recoil the shove's stagger, a knockdown when the
// strike is hard.
//
// Pure, covered by pommel_test.

#include "core/MathFns.h"
#include "game/NiMath.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

struct PommelSettings {
	bool enabled = true;      // [Hands] PommelStrikes
	float speed = 3.0f;       // m/s towards them: they recoil (ImpactSettings; vr::ApplyImpact)
	float hardSpeed = 6.0f;   // m/s: they go down
	float units = 8.0f;       // the pommel below the hand along the weapon (a longsword's measured 9.5)
	float fatigue = 10.0f;    // the player's, a strike
	// How much of the hand's motion must go the pommel's way (back along the
	// weapon) for it to lead: a swing whose pommel only trails into someone
	// is no pommel strike.
	float leadShare = 0.6f;
};

// The pommel in the world: from the hand back along the weapon (grip to tip).
inline NiPoint3 PommelPoint(const NiPoint3& hand, const NiPoint3& grip, const NiPoint3& tip, float units) {
	const NiPoint3 d = tip - grip;
	const float len = math::Sqrt(d.LengthSquared());
	if (!(len > 1e-3f)) {
		return hand;
	}
	const float u = units > 0.0f && units < 60.0f ? units : PommelSettings{}.units;
	return hand - d * (u / len);
}

// How much of the motion goes the pommel's way (back along the weapon), -1..1.
inline float PommelLead(const NiPoint3& velocity, const NiPoint3& grip, const NiPoint3& tip) {
	const NiPoint3 d = tip - grip;
	const float dl = math::Sqrt(d.LengthSquared());
	const float vl = math::Sqrt(velocity.LengthSquared());
	if (!(dl > 1e-3f) || !(vl > 1e-4f)) {
		return 0.0f;
	}
	return -(velocity.x * d.x + velocity.y * d.y + velocity.z * d.z) / (dl * vl);
}

struct PommelHand {
	bool meleeDrawn = false;   // a blade or a blunt weapon drawn in the hand
	bool valid = false;        // the hand tracked
	float towardsSpeed = 0.0f; // m/s along the line to them
	float lead = 0.0f;         // PommelLead
};

inline ShoveKind PommelStrikeFor(const PommelSettings& s, const PommelHand& h) {
	if (!s.enabled || !h.meleeDrawn || !h.valid || !(h.towardsSpeed >= s.speed) || !(h.lead >= s.leadShare)) {
		return ShoveKind::None;
	}
	return h.towardsSpeed >= s.hardSpeed ? ShoveKind::Hard : ShoveKind::Light;
}

}  // namespace obvr::game
