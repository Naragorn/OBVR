#pragma once

// The torch's blow where the torch meets a body (the tester, Windows
// 2026-11-26: "gegner mit fackel links schlagen ging manchmal nicht").
//
// Until now the torch's blow was found and measured at the HAND, as the
// shove's (CameraHook StepShoves, game::LivingActorAt): the hand inside an
// upright column round the actor's bound - half its radius wide, its radius
// high - and the hand's speed across the ground towards the bound's middle at
// the stagger's speed. A torch strikes with its far end, some 27 units (40
// cm) past the hand. On a rat on the floor (its bound 96-117 units round, the
// column a metre and a half across round a rat) that missed both ways:
// - a chop down onto it met it (the contact step: "a fast torch is held by
//   2B137C60 at 5.6 m/s", "rests on 2B137C60 ... 9.12 m/s into it") with the
//   hand mostly going down: the harness's chop at 4.8 m/s had 2.25 m/s of it
//   across the ground towards the middle, under the 3 - no blow;
// - a sweep or a jab landed while the torch was still short of the rat, the
//   hand already inside the column and going its way.
//
// Now the torch's own shaft (game/TorchShaftLogic.h, carried by the
// controller's pose this frame and the frame before) is tested against the
// capsules of the living the hands' contact step collected - the blade's hit
// by touch (game::CapsulesTouched), swept between the two poses so a fast
// blow does not pass through a rat in a frame. It is a blow when the torch
// goes INTO them there - its own speed at that point (the hand's and its turn
// carried out, as the half-sword's hilt) a quarter or more against the way
// out of their capsule: a chop onto a back, a sweep into a flank, a jab; not a
// slide along someone or a torch drawn back. And as the sword's power, the
// HAND's speed says how hard (the harness's chop met the rat with the hand at
// 4.4 m/s, 2.9 of it into the rat - a speed into them alone missed it again):
// a stagger from StaggerSpeed (3 m/s), a knockdown from KnockdownSpeed (6) -
// the fist's speeds at the contact (physical-combat-spec, "One impact
// speed"). Not the head's: it runs well ahead of the hand in a swing (the
// harness's chop: 7.5 m/s against the hand's 4.4), and a flick of the wrist
// is no knockdown (Nadi, 2026-10-26: "umhauen erst mit ordentlich schwung").
//
// A blow is the meeting: a torch that lay on them the frame before too
// (resting, pressed on) is no new blow.
//
// Pure; covered by torch_touch_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/BladeSweepLogic.h"
#include "game/BodyShapes.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

// How near the shaft has to come to a capsule: the blade's hit by touch.
inline constexpr float kTorchTouchUnits = 4.0f;
// How much of the torch's speed where it met them has to go into them: a
// quarter (some 15 degrees off their surface) - a slide along them does not.
inline constexpr float kTorchIntoShare = 0.25f;

// The shaft in the world: its two ends, and where along a-b the hand holds
// it (0 at a, 1 at b; TorchGripShare).
struct TorchPose {
	bool valid = false;
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	float grip = 0.0f;
};

// The hand that holds it, in the world's axes: SteamVR's velocity (m/s) and
// turn (radians a second), the player's own walking left out.
struct TorchHand {
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
	NiPoint3 spin{0.0f, 0.0f, 0.0f};
	float unitsPerMetre = 70.0f;
};

// Whom the torch met this frame, and how.
struct TorchTouch {
	UInt32 actor = 0;  // 0: nobody
	bool swept = false;  // met on the way between the two frames' poses
	NiPoint3 point{0.0f, 0.0f, 0.0f};   // the shaft's point nearest them there
	NiPoint3 normal{0.0f, 0.0f, 0.0f};  // the way out of them there
	BodyPart part = BodyPart::Column;   // the capsule met
	float speedInto = 0.0f;   // the torch's speed into them there, m/s (below 0: away)
	float speedThere = 0.0f;  // the torch's whole speed there, m/s
	float handSpeed = 0.0f;   // the hand's, m/s
};

// Where along a shaft a-b (in the hand's frame, the hand at its origin) the
// hand holds it: the origin's foot on the line, 0..1; 0 for a shaft of no
// length.
inline float TorchGripShare(const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 way = b - a;
	const float length = way.LengthSquared();
	if (!(length > 1e-6f)) {
		return 0.0f;
	}
	const float t = -(a.x * way.x + a.y * way.y + a.z * way.z) / length;
	return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

// The torch's velocity at `point` of the pose `p` (m/s): the hand's, and its
// turn carried out from where it holds the shaft.
inline NiPoint3 TorchPointVelocity(const TorchPose& p, const NiPoint3& point, const TorchHand& hand) {
	const NiPoint3 grip = p.a + (p.b - p.a) * p.grip;
	const float perMetre = hand.unitsPerMetre > 0.0f ? hand.unitsPerMetre : 70.0f;
	const NiPoint3 r = (point - grip) * (1.0f / perMetre);
	const NiPoint3& w = hand.spin;
	return hand.velocity + NiPoint3{w.y * r.z - w.z * r.y, w.z * r.x - w.x * r.z, w.x * r.y - w.y * r.x};
}

// The steps between the two poses, as the blade's sweep takes them
// (kBladeSweepStepUnits, at most kBladeSweepMostSteps); 1 - no sweep - with
// no pose before or a jump past kBladeSweepMostJumpUnits (a load, a teleport).
inline UInt32 TorchSweepSteps(const TorchPose& before, const TorchPose& now) {
	if (!before.valid || !now.valid) {
		return 1;
	}
	const float aWay = math::Sqrt((now.a - before.a).LengthSquared());
	const float bWay = math::Sqrt((now.b - before.b).LengthSquared());
	const float way = aWay > bWay ? aWay : bWay;
	if (!(way > kBladeSweepStepUnits) || !(way <= kBladeSweepMostJumpUnits)) {
		return 1;
	}
	const float steps = way / kBladeSweepStepUnits;
	if (!(steps < static_cast<float>(kBladeSweepMostSteps))) {
		return kBladeSweepMostSteps;
	}
	const UInt32 whole = static_cast<UInt32>(steps);
	return static_cast<float>(whole) < steps ? whole + 1u : whole;
}

// The box the shaft swept this frame (both poses), grown by `growUnits`: a
// capsule wholly outside it cannot be met, so a body near the player but away
// from the torch costs a few compares a frame, not its capsules' tests.
struct TorchBox {
	NiPoint3 low{0.0f, 0.0f, 0.0f};
	NiPoint3 high{0.0f, 0.0f, 0.0f};
};

inline TorchBox TorchSweptBox(const TorchPose& before, const TorchPose& now, float growUnits) {
	TorchBox box;
	box.low = now.a;
	box.high = now.a;
	const auto take = [&box](const NiPoint3& p) {
		box.low = NiPoint3{p.x < box.low.x ? p.x : box.low.x, p.y < box.low.y ? p.y : box.low.y,
		                   p.z < box.low.z ? p.z : box.low.z};
		box.high = NiPoint3{p.x > box.high.x ? p.x : box.high.x, p.y > box.high.y ? p.y : box.high.y,
		                    p.z > box.high.z ? p.z : box.high.z};
	};
	take(now.b);
	if (before.valid && TorchSweepSteps(before, now) > 1) {
		take(before.a);
		take(before.b);
	}
	box.low = box.low - NiPoint3{growUnits, growUnits, growUnits};
	box.high = box.high + NiPoint3{growUnits, growUnits, growUnits};
	return box;
}

// Whether one of the actor's capsules reaches into the box.
inline bool CapsulesInBox(const BladeBodies& bodies, UInt32 actor, const TorchBox& box) {
	for (UInt32 c = 0; c < bodies.count; ++c) {
		const BladeBodyCapsule& cap = bodies.cap[c];
		if (cap.actor != actor) {
			continue;
		}
		const float r = ShapeOuter(cap.radius, cap.shape);
		const float lowX = (cap.a.x < cap.b.x ? cap.a.x : cap.b.x) - r;
		const float lowY = (cap.a.y < cap.b.y ? cap.a.y : cap.b.y) - r;
		const float lowZ = (cap.a.z < cap.b.z ? cap.a.z : cap.b.z) - r;
		const float highX = (cap.a.x > cap.b.x ? cap.a.x : cap.b.x) + r;
		const float highY = (cap.a.y > cap.b.y ? cap.a.y : cap.b.y) + r;
		const float highZ = (cap.a.z > cap.b.z ? cap.a.z : cap.b.z) + r;
		if (highX >= box.low.x && lowX <= box.high.x && highY >= box.low.y && lowY <= box.high.y &&
		    highZ >= box.low.z && lowZ <= box.high.z) {
			return true;
		}
	}
	return false;
}

// The middle of an actor's capsules (the mean of their middles): where a
// push goes from. False when the list holds none of theirs.
inline bool CapsulesCentre(const BladeBodies& bodies, UInt32 actor, NiPoint3& out) {
	NiPoint3 sum{0.0f, 0.0f, 0.0f};
	UInt32 n = 0;
	for (UInt32 c = 0; c < bodies.count; ++c) {
		const BladeBodyCapsule& cap = bodies.cap[c];
		if (cap.actor != actor) {
			continue;
		}
		sum = sum + (cap.a + cap.b) * 0.5f;
		++n;
	}
	if (n == 0) {
		return false;
	}
	out = sum * (1.0f / static_cast<float>(n));
	return true;
}

// Where the shaft a-b lies nearest an actor's capsules, and the way out of
// them there: the capsule with the least gap. A point on a round capsule's
// axis has no way out of its own - the way from their middle is taken then.
// False with none of theirs.
inline bool TorchTouchPlace(const BladeBodies& bodies, UInt32 actor, const NiPoint3& a, const NiPoint3& b,
                            float marginUnits, NiPoint3& point, NiPoint3& normal, BodyPart* part = nullptr) {
	bool found = false;
	float best = 0.0f;
	for (UInt32 c = 0; c < bodies.count; ++c) {
		const BladeBodyCapsule& cap = bodies.cap[c];
		if (cap.actor != actor) {
			continue;
		}
		NiPoint3 at{0.0f, 0.0f, 0.0f};
		NiPoint3 out{0.0f, 0.0f, 0.0f};
		const float gap = BodySegmentGap(cap, a, b, marginUnits, &at, &out);
		if (!found || gap < best) {
			found = true;
			best = gap;
			point = at;
			normal = out;
			if (part != nullptr) {
				*part = cap.part;
			}
		}
	}
	if (found && !(normal.LengthSquared() > 0.25f)) {
		NiPoint3 middle{0.0f, 0.0f, 0.0f};
		CapsulesCentre(bodies, actor, middle);
		const NiPoint3 away = point - middle;
		const float length = math::Sqrt(away.LengthSquared());
		normal = length > 1e-3f ? away * (1.0f / length) : NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return found;
}

// The body the torch met this frame: newly - its pose the frame before did
// not touch them - on the way from that pose to this one, or at this one;
// the earliest pose it met them at, the torch's speed there and how much of
// it goes into them (against the way out of them; with no way out known all
// of it). Of several, the one met hardest. `skip`: someone not to be met
// (cooling down from the last blow).
inline TorchTouch TorchTouchOf(const BladeBodies& bodies, const TorchPose& before, const TorchPose& now,
                               const TorchHand& hand, UInt32 skip, float marginUnits = kTorchTouchUnits) {
	TorchTouch best;
	if (!now.valid || bodies.count == 0) {
		return best;
	}
	const UInt32 steps = TorchSweepSteps(before, now);
	const TorchBox box = TorchSweptBox(before, now, marginUnits);
	// The actors in the list, each once (a fitted body brings up to 36 capsules).
	constexpr UInt32 kSeenMost = 32;
	UInt32 seen[kSeenMost];
	UInt32 seenCount = 0;
	for (UInt32 c = 0; c < bodies.count; ++c) {
		const UInt32 actor = bodies.cap[c].actor;
		bool again = false;
		for (UInt32 s = 0; s < seenCount; ++s) {
			again = again || seen[s] == actor;
		}
		if (again || seenCount >= kSeenMost) {
			continue;
		}
		seen[seenCount++] = actor;
		if (actor == 0 || actor == skip || IsStopperId(actor) || !CapsulesInBox(bodies, actor, box)) {
			continue;
		}
		if (before.valid && CapsulesTouched(bodies, actor, before.a, before.b, marginUnits) == 1) {
			continue;  // on them the frame before too: no new meeting
		}
		NiPoint3 atA = now.a;
		NiPoint3 atB = now.b;
		bool swept = false;
		if (steps > 1 && SweptCapsulesTouched(bodies, actor, before.a, before.b, now.a, now.b, steps, marginUnits,
		                                      atA, atB) == 1) {
			swept = true;
		} else if (CapsulesTouched(bodies, actor, now.a, now.b, marginUnits) != 1) {
			continue;
		}
		TorchTouch t;
		t.actor = actor;
		t.swept = swept;
		TorchTouchPlace(bodies, actor, atA, atB, marginUnits, t.point, t.normal, &t.part);
		TorchPose at = now;
		at.a = atA;
		at.b = atB;
		const NiPoint3 v = TorchPointVelocity(at, t.point, hand);
		t.speedThere = math::Sqrt(v.LengthSquared());
		t.speedInto = t.normal.LengthSquared() > 0.25f
		                  ? -(v.x * t.normal.x + v.y * t.normal.y + v.z * t.normal.z)
		                  : t.speedThere;
		t.handSpeed = math::Sqrt(hand.velocity.LengthSquared());
		if (best.actor == 0 || t.speedInto > best.speedInto) {
			best = t;
		}
	}
	return best;
}

// Whether the torch went into them where it met them: some of its speed
// there against the way out of them, at least kTorchIntoShare of it.
inline bool TorchWentIn(const TorchTouch& t) {
	return t.actor != 0 && t.speedInto > 0.0f && t.speedInto >= kTorchIntoShare * t.speedThere;
}

// The blow: none when the torch did not go into them, else by the hand's
// speed - none under the stagger's (ShoveSettings::speed, [Hands]
// StaggerSpeed), a knockdown from the knockdown's (hardSpeed, KnockdownSpeed),
// else a stagger.
inline ShoveKind TorchTouchKind(const ShoveSettings& s, const TorchTouch& t) {
	if (!TorchWentIn(t) || !(t.handSpeed >= s.speed)) {
		return ShoveKind::None;
	}
	return t.handSpeed >= s.hardSpeed ? ShoveKind::Hard : ShoveKind::Light;
}

}  // namespace obvr::game
