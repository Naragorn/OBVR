#pragma once

// Handing a thing from one hand to the other (free-weapons-spec 5.14; Nadi,
// 2026-10-28: "rechte hand schwert. ich gebe es in die linke dort ist es dann
// wo auch immer festgehalten wurde ... aber als objekt. kann es werfen,
// loslassen. rechte hand kann es dann wieder ausrüsten am griff"). The
// engine wears a one-hander only in the weapon hand and a bow, a shield or a
// burning torch only in the other; handed to the hand that cannot wear it,
// the thing is dropped and held there as an object (the inventory's step 4,
// holding-objects-spec). The gesture: the other hand closes on it, then the
// hand that wears it lets go - that let go is the hand-over.
//
// Pure; handover_test.

#include "core/MathFns.h"
#include "game/InventoryGrabLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

enum class HandoverThing : UInt8 { None, OneHander, TwoHander, Bow, Shield, Torch, Item };
enum class WearHand : UInt8 { None, Weapon, Other };

inline WearHand WearHandOf(HandoverThing thing) {
	switch (thing) {
	case HandoverThing::OneHander:
		return WearHand::Weapon;
	case HandoverThing::Bow:
	case HandoverThing::Shield:
	case HandoverThing::Torch:
		return WearHand::Other;
	default:
		return WearHand::None;  // held by either hand alike; a two-hander takes both
	}
}

// The other hand within this many units of the thing counts as on it: about
// a hand's width round its line (a first guess, [Hands] HandoverReachMetres).
inline constexpr float kHandoverReachMetres = 0.12f;

// The let go of the hand that wears it (the drop its release decided) is a
// hand-over when the other hand's hold is closed within `reachUnits` of the
// thing (`otherToThingUnits`, negative unknown); else a drop or a throw.
inline bool HandOverAtLetGo(bool letGo, bool otherHoldClosed, float otherToThingUnits, float reachUnits) {
	return letGo && otherHoldClosed && reachUnits > 0.0f && otherToThingUnits >= 0.0f &&
	       otherToThingUnits <= reachUnits;
}

// How far a hand is from a thing's line (grip to tip), units.
inline float HandToSpanUnits(const NiPoint3& hand, const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 ab = b - a;
	const float len2 = ab.LengthSquared();
	float t = len2 > 1e-6f ? ((hand.x - a.x) * ab.x + (hand.y - a.y) * ab.y + (hand.z - a.z) * ab.z) / len2 : 0.0f;
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	const NiPoint3 on = a + ab * t;
	return math::Sqrt((hand - on).LengthSquared());
}

// Taken back: the hand that wears it closes within reach of the thing the
// other hand holds as an object. The other hand's hold is let go first, and
// the thing is taken (into the inventory, then equipped by the take's steps)
// only once the engine's grab has let go of it - a reference taken while the
// grab still posed it went down with the game (the pull's lesson,
// 2026-10-24). Given up after kHandBackWaitSeconds.
inline constexpr float kHandBackWaitSeconds = 1.0f;

struct HandBackState {
	UInt32 ref = 0;  // being taken back
	float waited = 0.0f;
	bool seenFree = false;  // the engine's grab let go of it last frame
};

struct HandBackInput {
	UInt32 heldRef = 0;               // what the other hand holds as an object (0 none)
	bool wearHandClosedNow = false;   // the wearing hand's hold closed this frame
	float wearHandToThingUnits = -1.0f;
	float reachUnits = 0.0f;
	UInt32 grabbedRef = 0;            // what the engine's grab holds now
	float dt = 0.0f;
};

struct HandBackVerdict {
	bool releaseHold = false;  // let the other hand's hold of it go (keep it off the grab)
	UInt32 takeRef = 0;        // take this one now
	bool gaveUp = false;
};

inline HandBackVerdict StepHandBack(HandBackState& s, const HandBackInput& in) {
	HandBackVerdict v;
	if (s.ref == 0) {
		if (in.heldRef != 0 && in.wearHandClosedNow && in.reachUnits > 0.0f && in.wearHandToThingUnits >= 0.0f &&
		    in.wearHandToThingUnits <= in.reachUnits) {
			s.ref = in.heldRef;
			s.waited = 0.0f;
			v.releaseHold = true;
		}
		return v;
	}
	// One frame after the grab let go: the grab physics deals with its
	// release (its body, its group) in the frame it sees it, after this step
	// - a reference taken in that frame was gone under it (handover-sword.txt
	// 2026-10-28 11:59: the game ended right after the take).
	if (in.grabbedRef != s.ref) {
		if (!s.seenFree) {
			s.seenFree = true;
			v.releaseHold = true;
			return v;
		}
		v.takeRef = s.ref;
		s = HandBackState{};
		return v;
	}
	s.seenFree = false;
	s.waited += in.dt > 0.0f ? in.dt : 0.0f;
	if (s.waited > kHandBackWaitSeconds) {
		v.gaveUp = true;
		s = HandBackState{};
		return v;
	}
	v.releaseHold = true;
	return v;
}

// A hand closing on a thing the other hand holds as an object: equipped
// back when this hand can wear it (the inventory take's choice by kind and
// hand), else passed to this hand and held there.
enum class HandoverAct : UInt8 { None, Pass, EquipBack };
inline HandoverAct ActOnClosing(InventoryTake take) {
	switch (take) {
	case InventoryTake::EquipWeapon:
	case InventoryTake::EquipOther:
		return HandoverAct::EquipBack;
	case InventoryTake::Hold:
		return HandoverAct::Pass;
	default:
		return HandoverAct::None;
	}
}

// A bow, a shield or a torch is worn at the other hand: a hand that far from
// that hand is this far off the thing, taking it as a ball of `thingUnits`
// round the hand (a first guess - [Hands] HandoverThingMetres; negative
// unknown).
inline float NearHeldInHandUnits(float toWearingHandUnits, float thingUnits) {
	if (toWearingHandUnits < 0.0f) {
		return -1.0f;
	}
	const float off = toWearingHandUnits - thingUnits;
	return off > 0.0f ? off : 0.0f;
}

}  // namespace obvr::game
