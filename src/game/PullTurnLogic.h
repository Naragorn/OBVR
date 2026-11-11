#pragma once

// A weapon pulled to the hand turns on the way (Nadi, 2026-11-11): it leaves
// the ground as it lay and comes in about as the hand will hold it, so the
// catch - the equip that puts it on the hand's Weapon node - has no snap.
//
// The float (game/HeldObject.h) places the held object's scene node each
// frame, so the turn is set, not driven by physics: against the hand, from
// the turn it had when the grip closed towards the Weapon node's turn
// against the hand, read live each frame (the hand may turn mid-flight). The
// share of the way follows the float's progress and is all there a little
// before the catch (kPullTurnDoneAt). A turn faster than
// kPullTurnMostRadiansPerSecond is held back, against a jump in one frame;
// it is set high because a pull is short - the harness's sword floated in
// over 9 frames (weapon-pull-turn.txt, 2026-11-11), and at 720 degrees a
// second it was still 30 degrees off at the catch, taken the frame after
// (the take does not wait). A far pull is slow by itself: the float lasts at
// most 0.6 s (game/HeldObject.h FloatSeconds).
//
// Pure: game/HeldObject.cpp places it; covered by pull_turn_test.

#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/NiMath.h"

namespace obvr::game {

inline constexpr float kPullTurnDoneAt = 0.85f;
inline constexpr float kPullTurnMostRadiansPerSecond = 37.699112f;  // 2160 degrees

// How much of the turn is done at this much of the float (0..1).
inline float PullTurnShare(float floated) {
	if (!(floated > 0.0f)) {
		return 0.0f;
	}
	const float share = floated / kPullTurnDoneAt;
	return share >= 1.0f ? 1.0f : share;
}

// The angle (radians, 0..pi) of a rotation matrix.
inline float RotationAngleOf(const NiMatrix33& r) {
	const float x = r.data[2][1] - r.data[1][2];
	const float y = r.data[0][2] - r.data[2][0];
	const float z = r.data[1][0] - r.data[0][1];
	const float twiceSine = math::Sqrt(x * x + y * y + z * z);
	const float twiceCosine = r.data[0][0] + r.data[1][1] + r.data[2][2] - 1.0f;
	return math::Atan2(twiceSine, twiceCosine);
}

// The angle between two rotations (radians).
inline float AngleBetweenRotations(const NiMatrix33& a, const NiMatrix33& b) {
	return RotationAngleOf(InverseRotation(a) * b);
}

// A rotation by `angle` radians about the unit axis (ax, ay, az).
inline NiMatrix33 AxisAngleRotation(float ax, float ay, float az, float angle) {
	const float c = math::Cos(angle);
	const float s = math::Sin(angle);
	const float t = 1.0f - c;
	NiMatrix33 m{};
	m.data[0][0] = c + ax * ax * t;
	m.data[0][1] = ax * ay * t - az * s;
	m.data[0][2] = ax * az * t + ay * s;
	m.data[1][0] = ay * ax * t + az * s;
	m.data[1][1] = c + ay * ay * t;
	m.data[1][2] = ay * az * t - ax * s;
	m.data[2][0] = az * ax * t - ay * s;
	m.data[2][1] = az * ay * t + ax * s;
	m.data[2][2] = c + az * az * t;
	return m;
}

// `share` of the way from `from` to `to` along the shortest turn (0: from,
// 1: to). Near a half turn the axis is read off the diagonal, where the
// skew part has vanished.
inline NiMatrix33 TurnPart(const NiMatrix33& from, const NiMatrix33& to, float share) {
	if (!(share > 0.0f)) {
		return from;
	}
	if (share >= 1.0f) {
		return to;
	}
	const NiMatrix33 r = InverseRotation(from) * to;
	const float angle = RotationAngleOf(r);
	if (!(angle > 1e-5f)) {
		return to;
	}
	float x = r.data[2][1] - r.data[1][2];
	float y = r.data[0][2] - r.data[2][0];
	float z = r.data[1][0] - r.data[0][1];
	float length = math::Sqrt(x * x + y * y + z * z);
	if (length < 1e-3f) {
		// About a half turn: the axis from the diagonal, the signs from the
		// symmetric part.
		x = math::Sqrt((r.data[0][0] + 1.0f) * 0.5f > 0.0f ? (r.data[0][0] + 1.0f) * 0.5f : 0.0f);
		y = math::Sqrt((r.data[1][1] + 1.0f) * 0.5f > 0.0f ? (r.data[1][1] + 1.0f) * 0.5f : 0.0f);
		z = math::Sqrt((r.data[2][2] + 1.0f) * 0.5f > 0.0f ? (r.data[2][2] + 1.0f) * 0.5f : 0.0f);
		if (x >= y && x >= z) {
			y = r.data[0][1] + r.data[1][0] < 0.0f ? -y : y;
			z = r.data[0][2] + r.data[2][0] < 0.0f ? -z : z;
		} else if (y >= z) {
			x = r.data[0][1] + r.data[1][0] < 0.0f ? -x : x;
			z = r.data[1][2] + r.data[2][1] < 0.0f ? -z : z;
		} else {
			x = r.data[0][2] + r.data[2][0] < 0.0f ? -x : x;
			y = r.data[1][2] + r.data[2][1] < 0.0f ? -y : y;
		}
		length = math::Sqrt(x * x + y * y + z * z);
		if (!(length > 0.0f)) {
			return to;
		}
	}
	return from * AxisAngleRotation(x / length, y / length, z / length, angle * share);
}

// The turn against the hand while a weapon floats in. Begun with the turn it
// had against the hand when the grip closed.
struct PullTurn {
	bool active = false;
	NiMatrix33 startRelative = NiMatrix33::Identity();
	NiMatrix33 relative = NiMatrix33::Identity();
};

inline void BeginPullTurn(PullTurn& t, const NiMatrix33& relative) {
	t.active = true;
	t.startRelative = relative;
	t.relative = relative;
}

// One frame: the turn wanted at this much of the float, towards the grip's
// turn against the hand now (`gripRelative`), reached at most
// kPullTurnMostRadiansPerSecond * dt from last frame's. No time (or none
// that is a number): it stays. Not begun: the grip's turn is not used.
inline NiMatrix33 StepPullTurn(PullTurn& t, const NiMatrix33& gripRelative, float floated, float dtSeconds) {
	if (!t.active) {
		return t.relative;
	}
	const NiMatrix33 wanted = TurnPart(t.startRelative, gripRelative, PullTurnShare(floated));
	const float dt = dtSeconds == dtSeconds && dtSeconds > 0.0f ? dtSeconds : 0.0f;
	const float most = kPullTurnMostRadiansPerSecond * dt;
	const float left = AngleBetweenRotations(t.relative, wanted);
	t.relative = left <= most ? wanted : TurnPart(t.relative, wanted, most / left);
	return t.relative;
}

// The watch on the way in, for the log and the harness: the angle to the
// grip's turn each frame - whether it ever grew by more than the tolerance
// (the hand turning faster than the cap, or a wrong turn), and whether it is
// close at the catch.
inline constexpr float kPullTurnRiseTolerance = 0.0349066f;   // 2 degrees
inline constexpr float kPullTurnCatchTolerance = 0.2617994f;  // 15 degrees

struct PullTurnWatch {
	float last = -1.0f;
	float worstRise = 0.0f;
};

inline void WatchPullTurn(PullTurnWatch& w, float angle) {
	if (w.last >= 0.0f && angle - w.last > w.worstRise) {
		w.worstRise = angle - w.last;
	}
	w.last = angle;
}

inline bool PullTurnNeverTurnedAway(const PullTurnWatch& w) { return w.worstRise <= kPullTurnRiseTolerance; }

inline bool PullTurnCaughtClose(float angleAtCatch) { return angleAtCatch < kPullTurnCatchTolerance; }

}  // namespace obvr::game
