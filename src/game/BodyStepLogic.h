#pragma once

// The feet planted while the player stands, and stepped after the body
// (Nadi in the headset, 2026-11-10: walking about the room the body came
// along, but its legs did not move).
//
// Standing, each foot is put back to where the animation has it relative to
// the root (game/BodyIkLogic.h, StepFootStance), and the root is wherever the
// room follow (vr/RoomFollowLogic.h) and the torso's turn put the body - so a
// body carried through the room, or turned after the head, slid its feet
// along the ground with it. Now each foot stays where it was put down, in the
// world, and steps when the place the animation wants it at has gone
// kFootStepTriggerUnits from it (10 cm), or the body has turned
// kFootStepTurnRadians since it was put down: one foot at a time, the one
// further off first, over kFootStepSeconds, lifted kFootStepLiftUnits at the
// middle of the step, on its way to the wanted place as it moves. A place
// further than kFootSnapUnits off (a load, a teleport) is taken at once, and
// one more than kFootLiftOffUnits above or below the plant (a jump, a fall:
// the feet off the ground) is the animation's until the feet are down again.
//
// A change of stance with the body still (the weapon drawn: the feet 5 to 8
// units further out) is no step: a planted foot glides with the stance's own
// change from the root - already held to [Body] IKStanceSpeed (BodyIkLogic.h,
// StepFootStance) - so the feet ease into the new stance in place.
//
// Only while the feet stand (no step bits in the movement flags: the engine
// walks the legs itself) and not for a kicking leg; anything else forgets the
// plant, and the foot is the animation's.
//
// Pure, covered by body_step_test.

#include "core/MathFns.h"
#include "game/BodyIkLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

inline constexpr float kFootStepTriggerUnits = 7.0f;  // 10 cm
inline constexpr float kFootStepTurnRadians = 30.0f * math::kDegreesToRadians;
inline constexpr float kFootStepSeconds = 0.3f;
inline constexpr float kFootStepLiftUnits = 3.0f;
inline constexpr float kFootSnapUnits = 40.0f;
inline constexpr float kFootLiftOffUnits = 10.0f;

struct FootPlant {
	bool valid = false;
	NiPoint3 at{0.0f, 0.0f, 0.0f};    // where it was put down, in the world
	float yaw = 0.0f;                 // the body's yaw then
	bool stepping = false;
	float share = 0.0f;               // of the step's time, 0..1
	NiPoint3 from{0.0f, 0.0f, 0.0f};  // where the step began
	NiPoint3 stance{0.0f, 0.0f, 0.0f};  // the foot's stance from the root, the body's frame, last frame
};

struct FeetPlantState {
	FootPlant foot[2];
	int lastStepped = -1;
};

struct FeetPlantInput {
	bool standing = false;  // the feet stand (FeetStand) and the steps are on
	bool valid[2] = {false, false};
	bool kicking[2] = {false, false};
	NiPoint3 wanted[2] = {};  // where the animation, carried with the body, has each foot
	NiPoint3 stance[2] = {};  // each foot from the root as the stance holds it, the body's frame
	float bodyYaw = 0.0f;
	float dt = 0.0f;
};

struct FeetPlantVerdict {
	NiPoint3 target[2] = {};
	bool stepping[2] = {false, false};
	bool began[2] = {false, false};  // a step started this frame
};

inline bool FootPlantFinite(const NiPoint3& p) {
	return p.x == p.x && p.y == p.y && p.z == p.z && p.x < 1.0e7f && p.x > -1.0e7f && p.y < 1.0e7f &&
	       p.y > -1.0e7f && p.z < 1.0e7f && p.z > -1.0e7f;
}

inline float FootPlantAcross(const NiPoint3& a, const NiPoint3& b) {
	const float dx = a.x - b.x;
	const float dy = a.y - b.y;
	return math::Sqrt(dx * dx + dy * dy);
}

// How far a planted foot is due a step: its distance across the ground from
// the wanted place, or the trigger's when the body has turned past the turn
// trigger since; 0 when it is not due.
inline float FootStepDue(const FootPlant& f, const NiPoint3& wanted, float bodyYaw) {
	const float across = FootPlantAcross(f.at, wanted);
	if (across > kFootStepTriggerUnits) {
		return across;
	}
	const float turned = math::WrapAngle(bodyYaw - f.yaw);
	return turned > kFootStepTurnRadians || turned < -kFootStepTurnRadians ? kFootStepTriggerUnits : 0.0f;
}

// The frame's input from the legs' reading: each foot where the animation
// has it, carried with the body across the ground; the kick's leg
// (`rightLeg` when `kicking`) left to the kick.
inline FeetPlantInput FeetPlantFrom(bool feetStand, float bodyYaw, float dt, const bool valid[2],
                                    const NiPoint3 animated[2], const NiPoint3& rootAnimated, const NiPoint3& carried,
                                    bool kicking, bool rightLeg) {
	FeetPlantInput in;
	in.standing = feetStand;
	in.bodyYaw = bodyYaw;
	in.dt = dt;
	for (int side = 0; side < 2; ++side) {
		in.valid[side] = valid[side];
		in.wanted[side] = animated[side] + carried;
		in.stance[side] = ToBodyFrame(animated[side] - rootAnimated, bodyYaw);
		in.kicking[side] = kicking && (side == 0) == rightLeg;
	}
	return in;
}

// A planted foot carried along by the stance's change since last frame,
// across the ground (the height stays the plant's).
inline void GlideWithStance(FootPlant& f, const NiPoint3& stance, float bodyYaw) {
	NiPoint3 shift = FromBodyFrame(stance - f.stance, bodyYaw);
	shift.z = 0.0f;
	if (FootPlantFinite(shift)) {
		f.at = f.at + shift;
		f.from = f.from + shift;
	}
	f.stance = stance;
}

inline FeetPlantVerdict StepFeetPlant(FeetPlantState& s, const FeetPlantInput& in) {
	FeetPlantVerdict v;
	const float dt = in.dt == in.dt && in.dt > 0.0f && in.dt < 1.0f ? in.dt : 0.0f;
	bool anyStepping = false;
	for (int side = 0; side < 2; ++side) {
		FootPlant& f = s.foot[side];
		v.target[side] = in.wanted[side];
		if (!in.standing || !in.valid[side] || in.kicking[side] || !FootPlantFinite(in.wanted[side])) {
			f = FootPlant{};
			continue;
		}
		const float rise = in.wanted[side].z - f.at.z;
		if (!f.valid || FootPlantAcross(f.at, in.wanted[side]) > kFootSnapUnits || rise > kFootLiftOffUnits ||
		    rise < -kFootLiftOffUnits) {
			f = FootPlant{};
			f.valid = true;
			f.at = in.wanted[side];
			f.yaw = in.bodyYaw;
			f.stance = in.stance[side];
		}
		GlideWithStance(f, in.stance[side], in.bodyYaw);
		anyStepping = anyStepping || f.stepping;
	}
	// A step begins for the foot furthest due, while no foot steps; even, the
	// one that did not step last.
	if (!anyStepping) {
		int pick = -1;
		float most = 0.0f;
		for (int side = 0; side < 2; ++side) {
			const FootPlant& f = s.foot[side];
			if (!f.valid) {
				continue;
			}
			const float due = FootStepDue(f, in.wanted[side], in.bodyYaw);
			if (due > most || (due > 0.0f && due == most && side != s.lastStepped)) {
				most = due;
				pick = side;
			}
		}
		if (pick >= 0) {
			FootPlant& f = s.foot[pick];
			f.stepping = true;
			f.share = 0.0f;
			f.from = f.at;
			v.began[pick] = true;
		}
	}
	for (int side = 0; side < 2; ++side) {
		FootPlant& f = s.foot[side];
		if (!f.valid) {
			continue;
		}
		if (!f.stepping) {
			v.target[side] = f.at;
			continue;
		}
		f.share += dt / kFootStepSeconds;
		if (f.share >= 1.0f) {
			f.stepping = false;
			f.share = 0.0f;
			f.at = in.wanted[side];
			f.yaw = in.bodyYaw;
			s.lastStepped = side;
			v.target[side] = f.at;
			continue;
		}
		const float k = f.share * f.share * (3.0f - 2.0f * f.share);  // eased in and out
		v.target[side] = f.from + (in.wanted[side] - f.from) * k;
		v.target[side].z += kFootStepLiftUnits * math::Sin(math::kPi * f.share);
		v.stepping[side] = true;
	}
	return v;
}

}  // namespace obvr::game
