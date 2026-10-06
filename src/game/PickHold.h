#pragma once

#include "core/Types.h"
#include "game/NearbyItems.h"

namespace obvr::game {

// The pick held steady (the tester, 2026-10-06: "bisher ist es ziemlich
// jittery. es sollte sich natürlich anfühlen").
//
// Until now the item nearest a hand was chosen afresh every frame
// (FindNearestItem, PickRank), and the ring, the info row and the mark
// followed the point the pick's ray hit. Three things shook:
//   - the choice: two items ranked alike (the laser between them, a hand
//     near both) swapped with every tremor of the hand, and the ring, the
//     text and the mark jumped with them;
//   - the hand: the same item reached for by both hands swapped the hand the
//     pick came from, and the ray's hit moved to the item's other side;
//   - the point: the ray's hit wandered over the surface with the hand, and
//     the aim at the item's near side jumped from vertex to vertex.
// So the pick holds what it has: a challenger has to beat it clearly, by a
// better class or by a margin within the class, and keep that up for a
// moment (kPickHoldChallengeSeconds); a touch wins at once, since a hand on
// a thing is not ambiguous. An item no longer reached for is kept for a
// short grace (kPickHoldGraceSeconds), so a hand at the edge of the reach
// does not blink. The hand is held the same way. And the points the ring,
// the row and the aim use ease towards their targets (StepAnchor) and snap
// only when the thing changes.
inline constexpr float kPickHoldChallengeSeconds = 0.15f;
inline constexpr float kPickHoldGraceSeconds = 0.15f;
// A clear margin within a class: for the classes keyed by the laser's miss
// (1 and 3) an angle; for those keyed by distance (0, 2 and 4) units.
inline constexpr float kPickHoldMarginRadians = 0.035f;  // 2 degrees
inline constexpr float kPickHoldMarginUnits = 5.0f;       // 7 cm

struct PickHoldState {
	NearItem held;                 // what the pick is on, with this frame's values
	UInt32 challenger = 0;         // the item that has been beating it
	bool challengerLeft = false;   // ... from this hand
	float challengeSeconds = 0.0f;
	float missingSeconds = 0.0f;   // how long the held item has not been reached for
};

inline bool RankKeyedByAngle(UInt8 rankClass) {
	return rankClass == 1 || rankClass == 3;
}

// Whether `a` ranks clearly better than `b`: a better class, or the same
// class with a key better by the class's margin.
inline bool RanksClearlyBetter(UInt8 aClass, float aKey, UInt8 bClass, float bKey) {
	if (aClass != bClass) {
		return aClass < bClass;
	}
	const float margin = RankKeyedByAngle(aClass) ? kPickHoldMarginRadians : kPickHoldMarginUnits;
	return aKey + margin < bKey;
}

// One frame. `best` is the frame's best-ranked item (FindNearestItem);
// `heldByHand[0]` (right) and `[1]` (left) are the held item's own ranks this
// frame, by hand (invalid where that hand does not reach for it). Returns
// the item the pick is on now, invalid for none.
inline NearItem StepPickHold(PickHoldState& s, const NearItem& best, const NearItem heldByHand[2], float dt) {
	if (!s.held.valid) {
		// Nothing held: the best is taken at once.
		s = PickHoldState{};
		s.held = best;
		return s.held;
	}
	const NearItem& byHeldHand = heldByHand[s.held.left ? 1 : 0];
	const NearItem& byOtherHand = heldByHand[s.held.left ? 0 : 1];
	const NearItem* fresh = byHeldHand.valid ? &byHeldHand : (byOtherHand.valid ? &byOtherHand : nullptr);
	if (fresh == nullptr) {
		// No hand reaches for it any more: kept through the grace, then let
		// go for the best.
		s.missingSeconds += dt;
		if (s.missingSeconds < kPickHoldGraceSeconds) {
			return s.held;
		}
		s = PickHoldState{};
		s.held = best;
		return s.held;
	}
	s.missingSeconds = 0.0f;
	// The held hand stays while it reaches; the other hand takes over only
	// when it is clearly better, after the challenge - or at once when the
	// held hand has let go of it.
	bool left = s.held.left;
	if (!byHeldHand.valid) {
		left = byOtherHand.left;
	}
	const NearItem& current = heldByHand[left ? 1 : 0];
	// The challenger: the best when it is another item or the other hand and
	// clearly better than what is held; a touch beats a non-touch at once.
	const bool otherThing = best.valid && (best.ref != s.held.ref || best.left != left);
	const bool clearly =
		otherThing && RanksClearlyBetter(best.rankClass, best.rankKey, current.rankClass, current.rankKey);
	if (clearly) {
		if (s.challenger != best.ref || s.challengerLeft != best.left) {
			s.challenger = best.ref;
			s.challengerLeft = best.left;
			s.challengeSeconds = 0.0f;
		}
		s.challengeSeconds += dt;
		const bool touchWins = best.rankClass == 0 && current.rankClass != 0;
		if (touchWins || s.challengeSeconds >= kPickHoldChallengeSeconds) {
			s = PickHoldState{};
			s.held = best;
			return s.held;
		}
	} else {
		s.challenger = 0;
		s.challengeSeconds = 0.0f;
	}
	s.held = current;
	s.held.left = left;
	return s.held;
}

// What the ring, the info row and the mark are shown on: the engine's pick
// target, but only once it has been the same for a moment
// (kShownSettleSeconds). The ray aimed at the held item can cross another
// thing lying against it for a few frames (pick-hold, 2026-10-06: the mark
// went to the other sword and back during one sweep); those frames show
// the thing shown before. Nothing shown yet takes the first thing at once.
inline constexpr float kShownSettleSeconds = 0.12f;

struct RefSettleState {
	UInt32 shown = 0;
	UInt32 wanted = 0;
	float seconds = 0.0f;
};

inline UInt32 StepRefSettle(RefSettleState& s, UInt32 wanted, float dt, float settleSeconds = kShownSettleSeconds) {
	if (wanted != s.wanted) {
		s.wanted = wanted;
		s.seconds = 0.0f;
	}
	s.seconds += dt;
	if (s.shown == 0 || s.wanted == s.shown || s.seconds >= settleSeconds) {
		s.shown = s.wanted;
	}
	return s.shown;
}

// A point that follows its target smoothly while the thing it is on stays
// the same, and snaps when the thing changes: for the ring, the info row and
// the pick's aim. A first-order ease, the share per frame dt/(tc+dt) - the
// time constant kAnchorSeconds leaves a tenth of a step after about 0.2 s at
// any frame rate. No dt (0) takes the target as it is.
inline constexpr float kAnchorSeconds = 0.08f;

struct AnchorState {
	bool valid = false;
	UInt32 ref = 0;
	NiPoint3 point{0.0f, 0.0f, 0.0f};
};

inline NiPoint3 StepAnchor(AnchorState& a, UInt32 ref, const NiPoint3& target, float dt,
                           float timeConstant = kAnchorSeconds) {
	if (!a.valid || a.ref != ref || ref == 0 || !(dt > 0.0f) || !(timeConstant > 0.0f)) {
		a.valid = ref != 0;
		a.ref = ref;
		a.point = target;
		return target;
	}
	const float share = dt / (timeConstant + dt);
	a.point = a.point + (target - a.point) * share;
	return a.point;
}

}  // namespace obvr::game
