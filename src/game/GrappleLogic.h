#pragma once

// The off hand in a fight, as in Blade & Sorcery (docs/physical-combat-spec.md,
// section 8; the tester, 2026-10-09: "man kann mit der off hand gegner packen
// oder hauen wie in blade & sorcery", and asked whether to note it for later:
// "ne starte jetzt"). The off hand is the one without the weapon - the left,
// the right for a left-handed player (vr::AssignHandRoles swaps the roles
// before anything here sees them).
//
// - The strike. With a blade or a blunt weapon drawn, the off hand - its grip
//   loose, not on a two-hander's handle - driven fast into someone is the
//   shove's blow (ShoveLogic.h): from ShoveSpeed they stagger and are pushed,
//   from ShoveHardSpeed they are knocked down. A fist is a punch; an open
//   hand the shove or the slap it is with the weapons away. With the weapons
//   away the hands are the shove's and the fists' already.
// - The grab. The off hand's grip closed at someone fighting - within one of
//   the capsules on their bones (BladeContactLogic.h) and kGrappleReachUnits
//   more - holds them: they stagger, their legs stop (SlowApproach.h,
//   StandStill), the hand drags them across the ground, and held by the
//   head, the neck or the weapon arm their blows do not land (game/Parry).
//   Let go at speed they are thrown - knocked down or staggered by the
//   hand's speed, as a shove; they break free after GrabHoldSeconds, and a
//   hand dragged far from them (they are stuck on something) loses them.
//
// Pure, covered by grapple_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

struct GrappleSettings {
	bool strikes = true;       // [Hands] OffHandStrikes
	bool grabs = true;         // [Hands] OffHandGrabs
	float holdSeconds = 4.0f;  // [Hands] GrabHoldSeconds: then they break free
	float fatigue = 15.0f;     // [Hands] GrabFatigue, spent at the grab (the light shove's)
};

// The hand this far outside a capsule still takes hold (about 9 cm; the
// capsules are thin on the arms).
inline constexpr float kGrappleReachUnits = 6.0f;
// The hand this far across the ground from where it wants them: lost.
inline constexpr float kGrappleLoseUnits = 40.0f;
// The drag: each frame they are placed where the hand wants them (SetPos's
// way, their controller placed too - game::PlaceActorAt), at most 2 m a
// second of the way. The hit's knockback through their character proxy
// was tried first (2026-10-10, hand script grapple.txt): over a single
// frame it did not move them at all, and re-issued over 0.3 s a frame it
// moved them a few units while the stagger carried them off - the hold
// was lost in 0.6 s both times.
inline constexpr float kGrappleDragUnitsPerSecond = 140.0f;

// GrabHoldSeconds as used: 0.5 to 30 s; anything else the default 4.
inline float GrappleHoldSeconds(float setting) {
	return setting >= 0.5f && setting <= 30.0f ? setting : 4.0f;
}

// The off hand's strike with a weapon drawn: the shove's thresholds, a fist
// or an open hand alike. None without a blade or blunt weapon drawn (the
// fists and the weapons away are the hands' own already), with the hand
// busy (its grip held, on a two-hander's handle), or too slow.
inline ShoveKind OffHandStrikeFor(const GrappleSettings& g, const ShoveSettings& shove, bool meleeWeaponDrawn,
                                  bool offHandFree, const ShoveHand& hand) {
	if (!g.strikes || !meleeWeaponDrawn || !offHandFree || !hand.valid || hand.gripHeld) {
		return ShoveKind::None;
	}
	if (!(hand.towardsSpeed >= shove.speed)) {
		return ShoveKind::None;
	}
	return hand.towardsSpeed >= shove.hardSpeed ? ShoveKind::Hard : ShoveKind::Light;
}

// Where the hand takes hold: the nearest capsule whose surface is within
// `reachUnits` of the hand (inside it counts as 0 or less).
struct GrappleTake {
	bool found = false;
	UInt32 actor = 0;
	BodyPart part = BodyPart::Column;
	float gap = 0.0f;  // the hand from the capsule's surface, units
};

inline GrappleTake GrappleTakeAt(const BladeBodies& bodies, const NiPoint3& hand, float reachUnits) {
	GrappleTake best;
	for (UInt32 i = 0; i < bodies.count; ++i) {
		const BladeBodyCapsule& c = bodies.cap[i];
		const float gap = PointSegmentDistance(hand, c.a, c.b) - c.radius;
		if (!(gap <= reachUnits)) {
			continue;
		}
		if (!best.found || gap < best.gap) {
			best.found = true;
			best.actor = c.actor;
			best.part = c.part;
			best.gap = gap;
		}
	}
	return best;
}

// Whether a hold there stops their blows: the head, the neck, or the arm
// that holds the weapon - the right; Oblivion's actors draw it in the right
// hand.
inline bool GrappleStopsBlows(BodyPart part) {
	return part == BodyPart::Head || part == BodyPart::Neck || part == BodyPart::RightArm;
}

enum class GrappleEnd : UInt8 {
	None,
	LetGo,       // the grip opened, the hand slow
	Thrown,      // opened at ShoveSpeed: staggered and pushed along the hand's motion
	ThrownDown,  // opened at ShoveHardSpeed: knocked down along it
	BrokeFree,   // held GrabHoldSeconds
	Lost,        // the hand dragged too far from them
	Gone,        // dead, unreadable, the mode or the setting off, the hand busy
};

inline const char* GrappleEndName(GrappleEnd e) {
	switch (e) {
	case GrappleEnd::LetGo: return "let go";
	case GrappleEnd::Thrown: return "thrown - staggered and pushed";
	case GrappleEnd::ThrownDown: return "thrown down";
	case GrappleEnd::BrokeFree: return "broke free";
	case GrappleEnd::Lost: return "lost - the hand too far from them";
	case GrappleEnd::Gone: return "gone - dead, out of reach, or the hand busy";
	default: return "held";
	}
}

// The grip opened: how they leave the hand, by its speed across the ground
// (m/s) - the shove's thresholds.
inline GrappleEnd GrappleReleaseFor(const ShoveSettings& shove, float speedAcross) {
	if (speedAcross >= shove.hardSpeed) {
		return GrappleEnd::ThrownDown;
	}
	return speedAcross >= shove.speed ? GrappleEnd::Thrown : GrappleEnd::LetGo;
}

// A grip squeezed just before the hand gets there still takes hold: for
// this long after it closed (a lunge squeezes first).
inline constexpr float kGrappleTakeGraceSeconds = 0.3f;

struct GrappleState {
	UInt32 actor = 0;  // held, 0 nobody
	BodyPart part = BodyPart::Column;
	float seconds = 0.0f;
	// Where they stood from the hand at the grab, across the ground: the
	// hand keeps them there.
	NiPoint3 offset{0.0f, 0.0f, 0.0f};
	bool gripWas = false;
	float closedFor = 0.0f;  // seconds the grip has been closed; past the grace after an end
};

struct GrappleInput {
	// [Hands] OffHandGrabs is in the settings; this is the rest: Full VR in
	// the world, the off hand free (not on a handle, no shield on it).
	bool allowed = false;
	bool handValid = false;
	bool grip = false;                         // the off hand's grip down
	NiPoint3 hand{0.0f, 0.0f, 0.0f};           // the off hand, world
	NiPoint3 handVelocity{0.0f, 0.0f, 0.0f};   // m/s, world
	// When the grip closes: what the hand is at, whether they are fighting,
	// and where they stand.
	GrappleTake take;
	bool takeFights = false;
	NiPoint3 takeAt{0.0f, 0.0f, 0.0f};
	// The one held, this frame: alive, standing, near; where they stand.
	bool heldReadable = false;
	NiPoint3 heldAt{0.0f, 0.0f, 0.0f};
	float dtSeconds = 0.0f;
};

struct GrappleVerdict {
	bool started = false;
	bool refused = false;  // the grip closed at someone not fighting
	GrappleEnd end = GrappleEnd::None;
	UInt32 actor = 0;
	BodyPart part = BodyPart::Column;
	NiPoint3 drag{0.0f, 0.0f, 0.0f};       // units across the ground to move them this frame
	float gap = 0.0f;                      // units across the ground from where the hand wants them
	float seconds = 0.0f;                  // how long they have been held, at an end the whole hold
	float speed = 0.0f;                    // the hand across the ground at the end, m/s
	NiPoint3 direction{0.0f, 0.0f, 0.0f};  // the hand's motion across the ground then, a unit
};

inline GrappleVerdict StepGrapple(GrappleState& s, const GrappleSettings& g, const ShoveSettings& shove,
                                  const GrappleInput& in) {
	GrappleVerdict v;
	const bool closed = in.grip && !s.gripWas;
	s.gripWas = in.grip;
	const float dt = in.dtSeconds > 0.0f ? in.dtSeconds : 0.0f;
	s.closedFor = !in.grip ? 0.0f : closed ? 0.0f : s.closedFor + dt;
	// Closing now, or closed within the grace and the hand arriving.
	const bool taking = in.grip && s.closedFor <= kGrappleTakeGraceSeconds;
	if (s.actor != 0) {
		v.actor = s.actor;
		v.part = s.part;
		if (!g.grabs || !in.allowed || !in.handValid || !in.heldReadable) {
			v.end = GrappleEnd::Gone;
		} else if (!in.grip) {
			const float across =
				math::Sqrt(in.handVelocity.x * in.handVelocity.x + in.handVelocity.y * in.handVelocity.y);
			v.speed = across;
			if (across > 1e-4f) {
				v.direction = NiPoint3{in.handVelocity.x / across, in.handVelocity.y / across, 0.0f};
			}
			v.end = GrappleReleaseFor(shove, across);
		} else {
			s.seconds += dt;
			const NiPoint3 d{in.hand.x + s.offset.x - in.heldAt.x, in.hand.y + s.offset.y - in.heldAt.y, 0.0f};
			const float gap = math::Sqrt(d.x * d.x + d.y * d.y);
			v.gap = gap;
			if (s.seconds >= GrappleHoldSeconds(g.holdSeconds)) {
				v.end = GrappleEnd::BrokeFree;
			} else if (gap > kGrappleLoseUnits) {
				v.end = GrappleEnd::Lost;
			} else {
				const float most = kGrappleDragUnitsPerSecond * dt;
				v.drag = gap > most && gap > 0.0f ? d * (most / gap) : d;
			}
		}
		v.seconds = s.seconds;
		if (v.end != GrappleEnd::None) {
			s.actor = 0;
			s.seconds = 0.0f;
			// The next hold wants the grip closed anew.
			s.closedFor = kGrappleTakeGraceSeconds + 1.0f;
		}
		return v;
	}
	if (!taking || !g.grabs || !in.allowed || !in.handValid || !in.take.found) {
		return v;
	}
	v.actor = in.take.actor;
	v.part = in.take.part;
	if (!in.takeFights) {
		v.refused = true;
		return v;
	}
	s.actor = in.take.actor;
	s.part = in.take.part;
	s.seconds = 0.0f;
	s.offset = NiPoint3{in.takeAt.x - in.hand.x, in.takeAt.y - in.hand.y, 0.0f};
	v.started = true;
	return v;
}

// Where a throw pushes from: behind them along the hand's motion, so the
// shove's push and knockback (which go away from that point) carry them
// along it. Their centre itself when the hand did not move.
inline NiPoint3 GrappleThrowFrom(const NiPoint3& centre, const NiPoint3& direction) {
	return NiPoint3{centre.x - direction.x * 30.0f, centre.y - direction.y * 30.0f, centre.z};
}

}  // namespace obvr::game
