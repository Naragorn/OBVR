#pragma once

// A walker's hand on the player's held arm (Oblivion Walkers' grab, its
// docs/walkers-spec.md 4.19; Nadi, 2026-11-04: "Walker-Griff per IK"). While
// Walkers' 'WGRB' names a holder for one of the player's arms, the holder's
// arm on that side is solved as two bones from its shoulder to a point on the
// player's forearm, after its own animation (the hold idle WalkersGrabHold.kf,
// both hands at chest height before it) - so the hand closes on the arm it
// holds instead of reaching past it.
//
// The skeleton: the zombie's (and MOO's zombie's) has the human Bip01 names
// (Walkers, read with pyffi 2026-10-28), so the same bones and the same rig
// as the player's own body IK (game/BodyIkLogic.h SolveLimb: each bone along
// its +x, the elbow a hinge about the upper bone's z).
//
// The weight eases the grip in and out (kGripEaseSeconds), and it is a blend
// of the solve's inputs, not of its rotations: the target goes from the
// animation's own wrist to the forearm, the elbow's way from the
// animation's own elbow to the grip's - so at a small weight the solve gives
// back the animation's arm, and taking hold does not snap. Pure, over plain
// values; game/WalkerGrip.cpp reads and writes the bones. walker_grip_test
// covers every flow.

#include "game/BodyIkLogic.h"

namespace obvr::game {

// 'WGRB's arm: the player's arm held, the walker standing on that side.
inline constexpr UInt32 kGripArmLeft = 1;
inline constexpr UInt32 kGripArmRight = 2;

// How far up the player's forearm from the wrist the walker's wrist goes, in
// game units (5 units are 7 cm at OBVR's 70 a metre): around the forearm, not
// on the hand.
inline constexpr float kGripBackUnits = 5.0f;

// The farthest reach laid on, as a share of the arm's two bones: farther
// and the walker keeps its animation's arm (it has not got there yet, or was
// torn away - Walkers lets go itself at TearFreeDistance). Between the arm's
// length and this the arm points straight at the forearm and ends short of
// it (SolveLimb, no stretch): it reaches for the arm it holds. Measured
// 2026-11-04 (walker-grip-on): with Walkers' holder 45 units from the
// player's centre the shoulder stood 45 to 80 units from the held forearm,
// arms of 21 + 15 - at 1.25 times no frame was laid on.
inline constexpr float kGripMostReach = 2.5f;

// The grip's ease, in and out.
inline constexpr float kGripEaseSeconds = 0.25f;

// 'WGHP' to Walkers when a holder takes hold (Walkers e103534): the held
// arm (1 left, 2 right) and the held forearm's world position - where the
// grip goes - so Walkers stands the holder HolderStandUnits beyond it,
// facing it, rather than before the player's feet (measured 2026-11-04: the
// forearm 31 units off the feet in the harness, the holder out of reach).
inline constexpr UInt32 kWalkersForearmMessage = 0x50484757;  // 'WGHP'

struct GripForearmMessage {
	UInt32 arm;
	float x;
	float y;
	float z;
};

inline GripForearmMessage ForearmMessageFor(UInt32 arm, const NiPoint3& forearm) {
	return GripForearmMessage{arm, forearm.x, forearm.y, forearm.z};
}

// Both forearms go to Walkers ten times a second while it listens (Nadi,
// 2026-11-04: a walker grabs only when it stands close enough already - a
// jump only when it is too small to notice), so Walkers can judge at the hit
// itself whether the holder is within reach of the arm it would take.
inline constexpr float kForearmSendSeconds = 0.1f;

// One frame of the send clock: true when the forearms are due. Start
// `since` at kForearmSendSeconds to send on the first frame.
inline bool ForearmSendDue(float& since, float dtSeconds) {
	if (!(dtSeconds >= 0.0f) || !ik::Finite(dtSeconds)) {
		return false;
	}
	since += dtSeconds;
	if (since < kForearmSendSeconds) {
		return false;
	}
	since = 0.0f;
	return true;
}

struct GripInput {
	NiPoint3 shoulder{0.0f, 0.0f, 0.0f};    // the walker's upper arm bone, world
	float upperLength = 0.0f;               // upper arm to forearm, units
	float foreLength = 0.0f;                // forearm to hand
	NiPoint3 walkerRight{1.0f, 0.0f, 0.0f}; // the walker's right, world, unit
	float side = 1.0f;                      // +1 the walker's right arm, -1 its left
	NiPoint3 playerWrist{0.0f, 0.0f, 0.0f}; // the player's held hand, world
	NiPoint3 playerHandAxis{0.0f, 1.0f, 0.0f}; // forearm towards the hand, unit
	NiPoint3 animElbow{0.0f, 0.0f, 0.0f};   // the animation's forearm bone, world
	NiPoint3 animWrist{0.0f, 0.0f, 0.0f};   // the animation's hand bone, world
	float weight = 1.0f;                    // 0 the animation, 1 the grip
};

struct GripPlan {
	bool use = false;
	LimbInput limb;
	const char* why = "";
};

inline NiPoint3 GripLerp(const NiPoint3& a, const NiPoint3& b, float t) {
	return NiPoint3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

inline GripPlan PlanGrip(const GripInput& in) {
	GripPlan plan;
	if (!(in.weight > 0.0f)) {
		plan.why = "no weight";
		return plan;
	}
	if (!ik::Finite(in.shoulder) || !ik::Finite(in.playerWrist) || !ik::Finite(in.animElbow) ||
	    !ik::Finite(in.animWrist) || !(in.upperLength > 1.0f) || !(in.foreLength > 1.0f)) {
		plan.why = "a bone or a hand that reads as nothing";
		return plan;
	}
	NiPoint3 axis;
	if (!ik::Unit(in.playerHandAxis, axis, 1e-3f)) {
		plan.why = "no forearm direction";
		return plan;
	}
	const NiPoint3 grip = in.playerWrist - axis * kGripBackUnits;
	const float reach = ik::Length(grip - in.shoulder);
	if (!(reach <= (in.upperLength + in.foreLength) * kGripMostReach)) {
		plan.why = "the forearm out of reach";
		return plan;
	}
	const float w = ik::Clamp(in.weight, 0.0f, 1.0f);
	// The elbow's way: the grip's down and out to the arm's side; the
	// animation's from its own elbow across the shoulder-wrist line.
	const NiPoint3 gripPole = NiPoint3{0.0f, 0.0f, -1.0f} + in.walkerRight * (0.6f * in.side);
	const NiPoint3 animPole = in.animElbow - in.shoulder;
	plan.limb.root = in.shoulder;
	plan.limb.upperLength = in.upperLength;
	plan.limb.lowerLength = in.foreLength;
	plan.limb.target = GripLerp(in.animWrist, grip, w);
	plan.limb.pole = GripLerp(animPole, gripPole, w);
	plan.limb.maxStretch = 1.0f;
	plan.limb.softShare = 0.0f;
	plan.use = true;
	plan.why = "laid on";
	return plan;
}

// One frame of the weight: towards 1 while held, towards 0 after.
inline float StepGripWeight(float weight, bool held, float dtSeconds) {
	if (!(dtSeconds >= 0.0f) || !ik::Finite(dtSeconds)) {
		return weight;
	}
	const float step = dtSeconds / kGripEaseSeconds;
	return ik::Clamp(held ? weight + step : weight - step, 0.0f, 1.0f);
}

// Which of the walker's hands takes the player's arm: the one nearer the
// grip as its animation has it - the hold idle raises both before it, so
// whichever way it stands, the nearer hand is the one to close.
enum WalkerHand { kWalkerNoHand = 0, kWalkerLeftHand = 1, kWalkerRightHand = 2 };

inline WalkerHand WalkerHandFor(const NiPoint3& leftWrist, const NiPoint3& rightWrist, const NiPoint3& grip) {
	const bool left = ik::Finite(leftWrist);
	const bool right = ik::Finite(rightWrist);
	if (!left && !right) {
		return kWalkerNoHand;
	}
	if (!left) {
		return kWalkerRightHand;
	}
	if (!right) {
		return kWalkerLeftHand;
	}
	return (leftWrist - grip).LengthSquared() <= (rightWrist - grip).LengthSquared() ? kWalkerLeftHand
	                                                                                  : kWalkerRightHand;
}

inline float GripSideSign(WalkerHand hand) { return hand == kWalkerLeftHand ? -1.0f : 1.0f; }

struct GripBones {
	const char* upper;
	const char* fore;
	const char* hand;
};

inline GripBones GripBoneNames(WalkerHand hand) {
	if (hand == kWalkerLeftHand) {
		return GripBones{"Bip01 L UpperArm", "Bip01 L Forearm", "Bip01 L Hand"};
	}
	return GripBones{"Bip01 R UpperArm", "Bip01 R Forearm", "Bip01 R Hand"};
}

}  // namespace obvr::game
