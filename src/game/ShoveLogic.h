#pragma once

// The shove: an open hand driven fast into a living actor, on purpose
// (docs/hand-weapon-collision-spec.md, "Proposal: pushing and pulling
// people on purpose"; the tester, 2026-09-29: "finde ich alles geil. können
// wir so machen"). With PushPeople off the hands pass through the living; a
// shove is the deliberate way to move someone.
//
// - Only with the weapons away (sheathed or none drawn - with a weapon or
//   the fists up a fast hand is a swing), an open hand (not a fist, the grip
//   not closed), and the hand moving towards the actor at ShoveSpeed or
//   faster: a light shove, the actor staggers. From ShoveHardSpeed a hard
//   one: knocked down, pushed away from the hand.
// - Either costs the player fatigue and lowers the actor's disposition
//   towards the player; the same actor is not shoved again for a moment.
//
// Pure, covered by shove_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

struct ShoveSettings {
	bool enabled = true;
	float speed = 2.0f;      // m/s towards the actor: a light shove
	float hardSpeed = 4.0f;  // m/s: a hard one, knocked down
	// The hard shove's force, as the engine's knockback takes it
	// (PushActorAway's: fKnockbackDamageBase 50 is a typical value).
	float hardForce = 3.0f;  // the tester, 2026-09-29: 50 threw them far too hard
	// The light shove's push: this far (game units) back, over this long - the
	// hit's own knockback through the character proxy (0x008907A0).
	float distance = 30.0f;
	float pushSeconds = 0.3f;
	float cooldownSeconds = 1.0f;
	float fatigueLight = 15.0f;
	float fatigueHard = 40.0f;  // PLANCK's shove costs 40 stamina
	float dispositionLight = 5.0f;
	float dispositionHard = 15.0f;
};

enum class ShoveKind : UInt8 { None, Light, Hard };

// What the hand is doing now.
struct ShoveHand {
	bool valid = false;
	bool open = false;         // not a fist
	bool gripHeld = false;
	float towardsSpeed = 0.0f; // m/s along the line to the actor
};

inline ShoveKind ShoveFor(const ShoveSettings& s, bool weaponDrawn, const ShoveHand& hand) {
	if (!s.enabled || weaponDrawn || !hand.valid || !hand.open || hand.gripHeld) {
		return ShoveKind::None;
	}
	if (!(hand.towardsSpeed >= s.speed)) {
		return ShoveKind::None;
	}
	return hand.towardsSpeed >= s.hardSpeed ? ShoveKind::Hard : ShoveKind::Light;
}

// The hand's speed towards a point, across the ground only (a hand pushed
// down onto someone's head is no shove): the velocity's part along the
// horizontal line from the hand to the point. Negative moving away.
inline float SpeedTowards(const NiPoint3& velocity, const NiPoint3& hand, const NiPoint3& target) {
	const float dx = target.x - hand.x;
	const float dy = target.y - hand.y;
	const float length = math::Sqrt(dx * dx + dy * dy);
	if (!(length > 0.0001f)) {
		return 0.0f;
	}
	return (velocity.x * dx + velocity.y * dy) / length;
}

// Whether a point lies in an actor's reach: within its bound's radius times
// `factor` plus `padUnits` of the bound's centre.
inline bool HandAtActor(const NiPoint3& hand, const NiPoint3& centre, float radius, float factor, float padUnits) {
	const float reach = radius * factor + padUnits;
	return (hand - centre).LengthSquared() <= reach * reach;
}

// The light shove's push: `distance` units along the ground, away from the
// hand (from `from` towards the actor's centre). Nothing when the two stand
// in one place.
inline NiPoint3 ShovePush(const NiPoint3& from, const NiPoint3& centre, float distance) {
	const float dx = centre.x - from.x;
	const float dy = centre.y - from.y;
	const float length = math::Sqrt(dx * dx + dy * dy);
	if (!(length > 0.0001f) || !(distance > 0.0f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return NiPoint3{dx / length * distance, dy / length * distance, 0.0f};
}

// One actor shoved not again until the cooldown has run out.
struct ShoveCooldown {
	const void* actor = nullptr;
	float secondsLeft = 0.0f;
};

inline void StepShoveCooldown(ShoveCooldown& c, float dt) {
	if (c.secondsLeft > 0.0f) {
		c.secondsLeft -= dt;
		if (c.secondsLeft <= 0.0f) {
			c.secondsLeft = 0.0f;
			c.actor = nullptr;
		}
	}
}

inline bool ShoveAllowed(const ShoveCooldown& c, const void* actor) {
	return actor != nullptr && !(c.actor == actor && c.secondsLeft > 0.0f);
}

inline void StartShoveCooldown(ShoveCooldown& c, const void* actor, float seconds) {
	c.actor = actor;
	c.secondsLeft = seconds;
}

}  // namespace obvr::game
