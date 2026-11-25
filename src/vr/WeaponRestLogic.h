#pragma once

// The drawn weapon kept at its rest in the hand (camera/hands/TwoHands.cpp
// HoldWeaponAtRest): through an action's animation - an attack, a block - and
// on that animation's way back after it.
//
// The game's first-person animations move the Weapon node in the hand: a
// staff's block holds it 5.7 units further along its axis than its idle
// (Weapon node at z 42.4 against 36.7). While the action plays OBVR writes the
// rest over it. When the action has ended (the player's action -1 again) the
// engine still blends back for a few frames: the staff, 2026-11-24, the node at
// z 42.4, 42.4, 41.5, 40.3, 39.3, 38.2 over the six frames after the block let
// go. Taken for the rest, those frames slid the staff 5.9 units along the hand
// and back (the palm measured at -31.3 down to -27.0 against -25.4 wanted).
// So after an action the rest is held until the engine's own pose is back at
// it, has stood still elsewhere (a rest of its own), or for kRestBackSeconds
// at the most. Pure, so it can be checked without the game
// (tests/WeaponRestTest.cpp).

#include "core/MathFns.h"
#include "vr/HalfSwordLogic.h"

namespace obvr::vr {

// Back at the rest: this near it, and turned this little off it.
inline constexpr float kRestNearUnits = 0.1f;
inline constexpr float kRestNearDegrees = 0.5f;
// Standing still: moving slower than these, for kRestStillSeconds.
inline constexpr float kRestStillUnitsPerSecond = 1.0f;
inline constexpr float kRestStillDegreesPerSecond = 5.0f;
inline constexpr float kRestStillSeconds = 0.2f;
// The longest the rest is held after an action.
inline constexpr float kRestBackSeconds = 0.75f;

struct RestHoldState {
	bool back = false;          // an action has ended, the engine's pose on its way back
	float backSeconds = 0.0f;   // since the action ended
	float stillSeconds = 0.0f;  // the engine's pose standing still since (after the action)
	WeaponPose last;            // the engine's pose last frame
	bool lastValid = false;
};

enum class RestStep : UInt8 {
	Take,   // the engine's pose is the rest: kept as it
	Hold,   // the kept rest written over the engine's pose
	Leave,  // an action plays and no rest is kept yet: the engine's pose stays, nothing taken
};

// The angle between two rotations, degrees.
inline float TurnDegrees(const NiMatrix33& a, const NiMatrix33& b) {
	const NiMatrix33 turn = Transposed(a) * b;
	float cosine = (turn.data[0][0] + turn.data[1][1] + turn.data[2][2] - 1.0f) * 0.5f;
	cosine = cosine > 1.0f ? 1.0f : (cosine < -1.0f ? -1.0f : cosine);
	return math::Atan2(math::Sqrt(1.0f - cosine * cosine), cosine) * math::kRadiansToDegrees;
}

inline float PoseDistance(const WeaponPose& a, const WeaponPose& b) {
	return math::Sqrt((a.pos - b.pos).LengthSquared());
}

// One frame of the rest hold: `engine` the Weapon node's local pose as the
// engine left it this frame, `rest` the rest kept (`haveRest`), `dt` the
// frame's seconds (0 or not a number: neither stillness nor the way back is
// counted).
inline RestStep StepRestHold(RestHoldState& st, bool actionPlays, bool haveRest, const WeaponPose& engine,
                             const WeaponPose& rest, float dt) {
	const bool timed = dt == dt && dt > 0.0f;
	if (timed && st.lastValid) {
		const bool still = PoseDistance(engine, st.last) < kRestStillUnitsPerSecond * dt &&
		                   TurnDegrees(st.last.rot, engine.rot) < kRestStillDegreesPerSecond * dt;
		st.stillSeconds = still ? st.stillSeconds + dt : 0.0f;
	}
	st.last = engine;
	st.lastValid = true;
	if (actionPlays) {
		st.back = haveRest;
		st.backSeconds = 0.0f;
		st.stillSeconds = 0.0f;
		return haveRest ? RestStep::Hold : RestStep::Leave;
	}
	if (!st.back || !haveRest) {
		st.back = false;
		return RestStep::Take;
	}
	if (timed) {
		st.backSeconds += dt;
	}
	const bool there = PoseDistance(engine, rest) <= kRestNearUnits && TurnDegrees(rest.rot, engine.rot) <= kRestNearDegrees;
	if (there || st.stillSeconds >= kRestStillSeconds || st.backSeconds >= kRestBackSeconds) {
		st.back = false;
		return RestStep::Take;
	}
	return RestStep::Hold;
}

}  // namespace obvr::vr
