#pragma once

// The living give way where a hand or the blade presses or strikes them - a
// prototype for the head and the arms ([Hands] BodiesGive, off; Nadi,
// 2026-11-17: physics on the living "wie in Blade and Sorcery", option A).
//
// What it is: the living have no bone bodies in the Havok world (a person is
// one character controller; docs/physical-combat-spec.md, F), so nothing
// physical moves when they are touched. Here a touched part is turned about
// its joint on top of the animation, by a spring: the neck for the head, the
// shoulder for the upper arm, the elbow (and a share of the shoulder) for the
// forearm. No Havok, no ragdoll - the look of a body that gives, as a
// procedural hit reaction does it (RootMotion Final IK's HitReaction:
// root-motion.com/finalikdox/html/class_root_motion_1_1_final_i_k_1_1_hit_reaction.html).
//
// Two things turn a joint:
// - a press: a pusher (the controller's own hand, not the drawn one the
//   contact holds at their surface; or the blade) lies in the part as the
//   animation has it. The joint is asked to turn so the part's surface moves
//   out of the pusher - a target, taken from the animation's pose every
//   frame, so it does not feed on its own turn;
// - a strike: the frame a pusher first lies in the part, its speed into the
//   part kicks the joint's turning speed.
// A joint short of its press is eased up to it, without overshoot, and its
// spring is off meanwhile, so a strike's kick is not braked by the press it
// came with; let go, or swung past the press by a strike, it comes back on a
// spring that overshoots a little - the wobble - and is caught again at the
// press if one is still there. Every turn and speed is capped.
//
// What it is not: the pushed part meets nothing - an arm pushed towards the
// body gives only a little (kGiveInwardShare) so it does not swing into the
// chest, but nothing stops a head or an arm at a wall or at someone else -
// the legs and the torso do not give, and nobody loses their footing by it. Pure, over plain values; game/BodyGive.cpp reads and
// writes the bones. body_give_test covers every flow.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BodyIkLogic.h"
#include "game/NiMath.h"
#include "game/SegmentGeometry.h"

namespace obvr::game {

// The joints that give. The neck is the bone the head turns about
// (HeadTurnsAtParent); the upper joint of an arm is its shoulder (the upper
// arm bone), the fore joint its elbow (the forearm bone).
enum GiveJoint : UInt32 {
	kGiveNeck,
	kGiveLeftUpper,
	kGiveLeftFore,
	kGiveRightUpper,
	kGiveRightFore,
	kGiveJointCount
};

// The flesh round the bones, game units at scale 1 (70 a metre): the guessed
// capsules contact uses (BladeContactLogic.h, kBladeHeadRadius and the
// others - proposed for a human, not measured on the meshes). The head
// reaches kGiveHeadAbove past its bone, away from the neck; the forearm's
// part kGiveHandUnits past the wrist, for the hand.
inline constexpr float kGiveHeadRadius = 7.0f;
inline constexpr float kGiveHeadAbove = 9.0f;
inline constexpr float kGiveUpperArmRadius = 4.5f;
inline constexpr float kGiveForearmRadius = 3.5f;
inline constexpr float kGiveHandUnits = 5.0f;

// How far a joint turns at most, and how fast (radians, radians a second).
// Proposed, not tuned in the headset: a head pushed 30 degrees over, an
// upper arm 35, an elbow 45.
inline constexpr float kGiveNeckMost = 30.0f * math::kDegreesToRadians;
inline constexpr float kGiveUpperMost = 35.0f * math::kDegreesToRadians;
inline constexpr float kGiveForeMost = 45.0f * math::kDegreesToRadians;
inline constexpr float kGiveMostSpeed = 12.0f;

// A joint short of its press closes the rest at this rate a second (two
// thirds of it in 30 ms); one within this share past its press counts as
// held at it, so a held joint does not flip between the ease and the spring.
// The spring back, as its natural frequency (radians a second) and damping
// ratio: 12 (1.9 Hz) at 0.4 - one visible swing past the middle, a quarter
// as far. Proposed, not tuned in the headset.
inline constexpr float kGivePushRate = 35.0f;
inline constexpr float kGiveHeldBand = 0.02f;
inline constexpr float kGiveReturnRate = 12.0f;
inline constexpr float kGiveReturnDamping = 0.4f;

// How much of a pusher's speed into a part its joint takes at the first
// touch, and how much of a touch on the forearm its elbow takes (the
// shoulder the rest).
inline constexpr float kGiveStrikeShare = 0.6f;
inline constexpr float kGiveForeShare = 0.65f;

// An arm pushed towards the body it hangs on meets that body: of a push that
// way only this share is taken (the arm gives a little, as flesh does, and
// does not swing into the chest); a push forward, back or outward is taken
// whole. Proposed, not tuned in the headset.
inline constexpr float kGiveInwardShare = 0.25f;

// A touch nearer its joint than this turns it as if it were this far off: a
// press on the shoulder itself asks for no turn of 90 degrees.
inline constexpr float kGiveLeverLeast = 4.0f;

// A joint this near its rest, this slow and untouched is at rest: its turn
// is nothing and its bone is left to the animation.
inline constexpr float kGiveRestTurn = 0.002f;
inline constexpr float kGiveRestSpeed = 0.02f;

// A frame longer than this is stepped as this (a hitch does not throw a
// head), and a pusher moved further than kGiveJumpUnits in a frame was put
// there (a teleport, a load): no speed from it.
inline constexpr float kGiveMostStepSeconds = 1.0f / 30.0f;
inline constexpr float kGiveJumpUnits = 100.0f;

// The pushers: the weapon hand when it is a hand, the other hand, the blade.
inline constexpr UInt32 kGivePusherWeaponHand = 0;
inline constexpr UInt32 kGivePusherOffHand = 1;
inline constexpr UInt32 kGivePusherBlade = 2;
inline constexpr UInt32 kGivePushers = 3;

// How thick a pusher is, game units: the hand's capsule as contact has it
// (HandBodyLogic.h, kHandBodyRadiusUnits), and a blade.
inline constexpr float kGiveHandRadius = 2.5f;
inline constexpr float kGiveBladeRadius = 1.0f;

// Which pushers there are this frame. A hand pushes while it is tracked and
// meets the world as itself (as a hand, or the hand under a shield or round
// a torch - not one busy with a bow, a handle, a held thing or a person);
// the blade while the weapon hand is tracked, does not meet the world as a
// hand, a swung weapon is drawn and its span was read.
struct GivePushersNow {
	bool hand[2] = {false, false};  // 0 the weapon hand, 1 the other
	bool blade = false;
};

inline GivePushersNow PushersNow(bool weaponHandTracked, bool offHandTracked, bool weaponHandMeets, bool offHandMeets,
                                 bool meleeDrawn, bool bladeMeasured) {
	GivePushersNow n;
	n.hand[0] = weaponHandTracked && weaponHandMeets;
	n.hand[1] = offHandTracked && offHandMeets;
	n.blade = weaponHandTracked && !weaponHandMeets && meleeDrawn && bladeMeasured;
	return n;
}

// A pusher: a segment with a radius in the world, and the speed of its two
// ends (units a second).
struct GivePusher {
	bool valid = false;
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
	NiPoint3 velA{0.0f, 0.0f, 0.0f};
	NiPoint3 velB{0.0f, 0.0f, 0.0f};
};

// Where a pusher was the frame before, for its speed.
struct GivePusherMemory {
	bool had = false;
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
};

// This frame's pusher from where it is now and where it was: its ends'
// speeds over the frame. No speed on its first frame, over a frame of no
// length, or after a jump; an invalid one forgets where it was.
inline GivePusher PusherFrom(GivePusherMemory& m, bool valid, const NiPoint3& a, const NiPoint3& b, float radius,
                             float dtSeconds) {
	GivePusher p;
	if (!valid || !ik::Finite(a) || !ik::Finite(b) || !(radius > 0.0f)) {
		m = GivePusherMemory{};
		return p;
	}
	p.valid = true;
	p.a = a;
	p.b = b;
	p.radius = radius;
	const bool timed = dtSeconds > 1e-4f && ik::Finite(dtSeconds);
	const float jump = kGiveJumpUnits * kGiveJumpUnits;
	if (m.had && timed && (a - m.a).LengthSquared() <= jump && (b - m.b).LengthSquared() <= jump) {
		p.velA = (a - m.a) * (1.0f / dtSeconds);
		p.velB = (b - m.b) * (1.0f / dtSeconds);
	}
	m.had = true;
	m.a = a;
	m.b = b;
	return p;
}

// Someone's bones as the animation has them this frame, in the world.
// Index 0 the left arm, 1 the right.
struct GiveBones {
	bool head = false;  // the head bone and the bone it turns about were read
	NiPoint3 neck{0.0f, 0.0f, 0.0f};  // where the head turns (HeadTurnsAtParent)
	NiPoint3 headBone{0.0f, 0.0f, 0.0f};
	bool arm[2] = {false, false};  // the upper arm, forearm and hand bones were read
	NiPoint3 shoulder[2]{};
	NiPoint3 elbow[2]{};
	NiPoint3 wrist[2]{};
	float scale = 1.0f;
};

// The bone the head turns about: its parent, when that carries nothing but
// the head ("Bip01 Neck1" on Oblivion's human skeleton) - else the head bone
// itself. Never "Bip01 Neck": the clavicles hang on it, and a turn of it
// swings both arms with the head (seen in the harness 2026-11-17: a head
// pressed aside lifted one arm and folded the other behind the back).
inline bool HeadTurnsAtParent(bool parentIsBone, UInt32 parentChildren) { return parentIsBone && parentChildren == 1; }

// A part that can be touched: a capsule, the joint it turns and where that
// joint is; for a forearm also the joint above (the shoulder) and its place.
struct GivePart {
	GiveJoint joint = kGiveNeck;
	NiPoint3 pivot{0.0f, 0.0f, 0.0f};
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
	bool hasUpper = false;
	GiveJoint upperJoint = kGiveNeck;
	NiPoint3 upperPivot{0.0f, 0.0f, 0.0f};
	// For an arm: the way to the body it hangs on (unit; nothing when
	// neither the neck nor the other shoulder was read).
	NiPoint3 inward{0.0f, 0.0f, 0.0f};
};

inline constexpr UInt32 kGivePartsMax = 5;

inline GiveJoint GiveUpperJoint(UInt32 arm) { return arm == 0 ? kGiveLeftUpper : kGiveRightUpper; }
inline GiveJoint GiveForeJoint(UInt32 arm) { return arm == 0 ? kGiveLeftFore : kGiveRightFore; }

// The parts of someone from their bones: the head, and each arm read as an
// upper arm and a forearm with its hand. Answers how many.
inline UInt32 GiveParts(const GiveBones& bones, GivePart out[kGivePartsMax]) {
	const float k = bones.scale > 0.0f && bones.scale < 20.0f ? bones.scale : 1.0f;
	UInt32 count = 0;
	if (bones.head) {
		NiPoint3 up{0.0f, 0.0f, 1.0f};
		ik::Unit(bones.headBone - bones.neck, up, 1e-3f);
		GivePart& p = out[count++];
		p = GivePart{};
		p.joint = kGiveNeck;
		p.pivot = bones.neck;
		p.a = bones.headBone;
		p.b = bones.headBone + up * (kGiveHeadAbove * k);
		p.radius = kGiveHeadRadius * k;
	}
	for (UInt32 arm = 0; arm < 2; ++arm) {
		if (!bones.arm[arm]) {
			continue;
		}
		NiPoint3 inward{0.0f, 0.0f, 0.0f};
		if (bones.head) {
			ik::Unit(bones.neck - bones.shoulder[arm], inward, 1e-3f);
		} else if (bones.arm[1 - arm]) {
			ik::Unit(bones.shoulder[1 - arm] - bones.shoulder[arm], inward, 1e-3f);
		}
		GivePart& upper = out[count++];
		upper = GivePart{};
		upper.inward = inward;
		upper.joint = GiveUpperJoint(arm);
		upper.pivot = bones.shoulder[arm];
		upper.a = bones.shoulder[arm];
		upper.b = bones.elbow[arm];
		upper.radius = kGiveUpperArmRadius * k;
		NiPoint3 along{0.0f, 0.0f, 0.0f};
		ik::Unit(bones.wrist[arm] - bones.elbow[arm], along, 1e-3f);
		GivePart& fore = out[count++];
		fore = GivePart{};
		fore.joint = GiveForeJoint(arm);
		fore.pivot = bones.elbow[arm];
		fore.a = bones.elbow[arm];
		fore.b = bones.wrist[arm] + along * (kGiveHandUnits * k);
		fore.radius = kGiveForearmRadius * k;
		fore.inward = inward;
		fore.hasUpper = true;
		fore.upperJoint = GiveUpperJoint(arm);
		fore.upperPivot = bones.shoulder[arm];
	}
	return count;
}

// A pusher in a part: how deep, where on the part's axis, which way the part
// is pushed (out of the pusher, unit) and the pusher's speed there.
struct GiveTouch {
	bool hit = false;
	float depth = 0.0f;
	NiPoint3 at{0.0f, 0.0f, 0.0f};
	NiPoint3 into{0.0f, 0.0f, 0.0f};
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
};

inline GiveTouch TouchOf(const GivePusher& pusher, const GivePart& part) {
	GiveTouch t;
	if (!pusher.valid) {
		return t;
	}
	NiPoint3 onPusher;
	NiPoint3 onPart;
	ClosestOnSegments(pusher.a, pusher.b, part.a, part.b, onPusher, onPart);
	const float depth = pusher.radius + part.radius - ik::Length(onPart - onPusher);
	if (!(depth > 0.0f)) {
		return t;
	}
	const float share = ShareOnSegment(onPusher, pusher.a, pusher.b);
	t.velocity = pusher.velA + (pusher.velB - pusher.velA) * share;
	// The pusher's line through the part's axis has no way out of its own:
	// the way it moves is taken, and a still one pushes nowhere.
	if (!ik::Unit(onPart - onPusher, t.into, 1e-3f) && !ik::Unit(t.velocity, t.into, 1e-3f)) {
		return t;
	}
	t.hit = true;
	t.depth = depth;
	t.at = onPart;
	return t;
}

// The small turn about a joint that moves the point `lever` off it by `push`
// (the part of `push` across the lever: a turn cannot move a point along its
// own lever). A rotation vector: its direction the axis, its length the
// angle in radians.
inline NiPoint3 TurnFor(const NiPoint3& lever, const NiPoint3& push) {
	const float least = kGiveLeverLeast * kGiveLeverLeast;
	const float l2 = lever.LengthSquared();
	return ik::Cross(lever, push) * (1.0f / (l2 > least ? l2 : least));
}

// What this frame asks of a joint.
struct GiveDrive {
	UInt32 touchedBy = 0;  // the pushers in one of its parts, a bit each
	NiPoint3 target{0.0f, 0.0f, 0.0f};  // the turn the presses ask for
	NiPoint3 kick{0.0f, 0.0f, 0.0f};    // turning speed from the pushers new to it
};

// A joint's turn (a rotation vector in the world's axes, on top of the
// animation), its turning speed, and which pushers lay in it the frame
// before.
struct GiveJointState {
	NiPoint3 turn{0.0f, 0.0f, 0.0f};
	NiPoint3 speed{0.0f, 0.0f, 0.0f};
	UInt32 touchedBy = 0;
};

struct GiveState {
	GiveJointState joint[kGiveJointCount];
};

// The way a part is pushed, with what goes towards the body it hangs on cut
// to its share.
inline NiPoint3 PushWay(const NiPoint3& into, const NiPoint3& inward) {
	const float towards = ik::Dot(into, inward);
	return towards > 0.0f ? into - inward * (towards * (1.0f - kGiveInwardShare)) : into;
}

inline void AddDrive(GiveDrive& d, UInt32 pusherBit, bool wasTouched, const NiPoint3& lever, const GiveTouch& t,
                     const NiPoint3& way, float share) {
	d.touchedBy |= pusherBit;
	d.target = d.target + TurnFor(lever, way * (t.depth * share));
	const float closing = ik::Dot(t.velocity, t.into);
	if (!wasTouched && closing > 0.0f) {
		d.kick = d.kick + TurnFor(lever, way * (closing * kGiveStrikeShare * share));
	}
}

// This frame's drives from the pushers and the parts. `before` says which
// pushers lay in each joint the frame before: only one new to a joint kicks
// it.
inline void GatherDrives(const GiveBones& bones, const GivePusher* pushers, UInt32 count, const GiveState& before,
                         GiveDrive out[kGiveJointCount]) {
	for (UInt32 j = 0; j < kGiveJointCount; ++j) {
		out[j] = GiveDrive{};
	}
	GivePart parts[kGivePartsMax];
	const UInt32 partCount = GiveParts(bones, parts);
	for (UInt32 p = 0; p < count && p < 32; ++p) {
		const UInt32 bit = 1u << p;
		for (UInt32 i = 0; i < partCount; ++i) {
			const GivePart& part = parts[i];
			const GiveTouch t = TouchOf(pushers[p], part);
			if (!t.hit) {
				continue;
			}
			const float share = part.hasUpper ? kGiveForeShare : 1.0f;
			const NiPoint3 way = PushWay(t.into, part.inward);
			AddDrive(out[part.joint], bit, (before.joint[part.joint].touchedBy & bit) != 0, t.at - part.pivot, t, way,
			         share);
			if (part.hasUpper) {
				AddDrive(out[part.upperJoint], bit, (before.joint[part.upperJoint].touchedBy & bit) != 0,
				         t.at - part.upperPivot, t, way, 1.0f - kGiveForeShare);
			}
		}
	}
}

inline float GiveMostTurn(UInt32 joint) {
	if (joint == kGiveNeck) {
		return kGiveNeckMost;
	}
	return joint == kGiveLeftUpper || joint == kGiveRightUpper ? kGiveUpperMost : kGiveForeMost;
}

// `v` no longer than `most`.
inline NiPoint3 GiveCapped(const NiPoint3& v, float most) {
	const float l2 = v.LengthSquared();
	return l2 > most * most ? v * (most / math::Sqrt(l2)) : v;
}

// One frame of a joint; true while it is not at rest.
inline bool StepJoint(GiveJointState& s, const GiveDrive& d, float most, float dtSeconds) {
	const bool touched = d.touchedBy != 0;
	s.touchedBy = d.touchedBy;
	if (!(dtSeconds > 0.0f) || !ik::Finite(dtSeconds)) {
		return touched || s.turn.LengthSquared() > 0.0f || s.speed.LengthSquared() > 0.0f;
	}
	const float dt = dtSeconds < kGiveMostStepSeconds ? dtSeconds : kGiveMostStepSeconds;
	s.speed = GiveCapped(s.speed + d.kick, kGiveMostSpeed);
	const NiPoint3 target = touched ? GiveCapped(d.target, most) : NiPoint3{0.0f, 0.0f, 0.0f};
	const float asked = target.LengthSquared();
	// Held by its press (short of it, or just at it): the spring back is off.
	const bool held = touched && asked > 0.0f && ik::Dot(s.turn, target) <= asked * (1.0f + kGiveHeldBand);
	if (!held) {
		const NiPoint3 pull = s.turn * (-kGiveReturnRate * kGiveReturnRate) -
		                      s.speed * (2.0f * kGiveReturnDamping * kGiveReturnRate);
		s.speed = GiveCapped(s.speed + pull * dt, kGiveMostSpeed);
	}
	s.turn = s.turn + s.speed * dt;
	NiPoint3 along;
	if (touched && ik::Dot(s.turn, target) < asked && ik::Unit(target, along)) {
		// Still short of the press: eased up to it, and no speed back from it.
		s.turn = s.turn + (target - s.turn) * (1.0f - math::Exp(-dt * kGivePushRate));
		const float back = ik::Dot(s.speed, along);
		if (back < 0.0f) {
			s.speed = s.speed - along * back;
		}
	}
	if (s.turn.LengthSquared() > most * most && ik::Unit(s.turn, along)) {
		// At its most: held there, and what speed went further out is gone.
		s.turn = along * most;
		const float outward = ik::Dot(s.speed, along);
		if (outward > 0.0f) {
			s.speed = s.speed - along * outward;
		}
	}
	if (!touched && s.turn.LengthSquared() < kGiveRestTurn * kGiveRestTurn &&
	    s.speed.LengthSquared() < kGiveRestSpeed * kGiveRestSpeed) {
		s.turn = NiPoint3{0.0f, 0.0f, 0.0f};
		s.speed = NiPoint3{0.0f, 0.0f, 0.0f};
		return false;
	}
	return true;
}

// One frame of someone: the drives gathered and every joint stepped. True
// while any joint is not at rest (their bones are then written).
inline bool StepGive(GiveState& s, const GiveBones& bones, const GivePusher* pushers, UInt32 count, float dtSeconds) {
	GiveDrive drives[kGiveJointCount];
	GatherDrives(bones, pushers, count, s, drives);
	bool active = false;
	for (UInt32 j = 0; j < kGiveJointCount; ++j) {
		active = StepJoint(s.joint[j], drives[j], GiveMostTurn(j), dtSeconds) || active;
	}
	return active;
}

// Whether any pusher comes within `reach` of the ball round `centre`: whom
// the step looks at at all.
inline bool PusherNear(const GivePusher* pushers, UInt32 count, const NiPoint3& centre, float reach) {
	for (UInt32 p = 0; p < count; ++p) {
		if (pushers[p].valid && PointSegmentDistance(centre, pushers[p].a, pushers[p].b) <= reach + pushers[p].radius) {
			return true;
		}
	}
	return false;
}

// A rotation vector as a rotation; one too small to have an axis is none.
inline NiMatrix33 TurnMatrix(const NiPoint3& turn) {
	NiPoint3 axis;
	if (!ik::Finite(turn) || !ik::Unit(turn, axis, 1e-6f)) {
		return NiMatrix33::Identity();
	}
	return ik::TurnAbout(axis, ik::Length(turn));
}

// A bone's local rotation for its world rotation turned by `turn` (in the
// world's axes, about the bone's own place) under a parent whose world
// rotation is given.
inline NiMatrix33 TurnedLocalRot(const NiMatrix33& parentWorldRot, const NiMatrix33& boneWorldRot,
                                 const NiPoint3& turn) {
	return ik::Transposed(parentWorldRot) * TurnMatrix(turn) * boneWorldRot;
}

}  // namespace obvr::game
