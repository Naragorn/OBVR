#pragma once

// Holding a two-handed weapon with both hands (the tester, 2026-09-30: "bei 2
// händern mit der linken hand meine hand an das schwert/axt/whatever
// andocken kann mit grip und so zweihändig halte").
//
// The right hand holds the weapon as always. The left grip closed on its
// handle - near the line of the weapon, not too far along it - takes hold:
// from then on the weapon points along the line between the two hands
// (towards the left hand when it holds higher up the handle, away from it
// when it holds lower down), and the left hand stays on the handle at the
// distance it took hold. Letting go of the grip, sheathing, or pulling the
// hands apart ends it.
//
// Pure, covered by two_hand_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

struct TwoHandSettings {
	bool enabled = true;
	// The left hand takes hold within this many game units of the weapon's
	// line (70 a metre)...
	float reachUnits = 12.0f;
	// ...between this far behind the right hand (towards the pommel) and this
	// far ahead of it (up the handle), and not closer to it than minUnits.
	float behindUnits = 25.0f;
	float aheadUnits = 40.0f;
	float minUnits = 5.0f;
	// The hands this much further apart or closer than when it took hold: let go.
	float slackUnits = 30.0f;
};

// Where a point lies against a line through `origin` along the unit vector
// `dir`: how far along it (`axial`, negative behind) and how far from it.
inline void AxialLateral(const NiPoint3& point, const NiPoint3& origin, const NiPoint3& dir, float& axial,
                         float& lateral) {
	const NiPoint3 d = point - origin;
	axial = d.x * dir.x + d.y * dir.y + d.z * dir.z;
	const NiPoint3 off = d - dir * axial;
	lateral = math::Sqrt(off.LengthSquared());
}

struct TwoHandState {
	bool active = false;
	float sign = 1.0f;      // +1 the left hand up the handle, -1 below the right
	float distance = 0.0f;  // between the hands when it took hold
};

// Whether the left grip, closing now, takes hold of the handle.
inline bool TwoHandTakes(const TwoHandSettings& s, bool twoHandedDrawn, bool leftGripClosedNow, float axial,
                         float lateral) {
	if (!s.enabled || !twoHandedDrawn || !leftGripClosedNow) {
		return false;
	}
	const float along = axial < 0.0f ? -axial : axial;
	return lateral <= s.reachUnits && axial >= -s.behindUnits && axial <= s.aheadUnits && along >= s.minUnits;
}

inline void StartTwoHand(TwoHandState& t, float axial) {
	t.active = true;
	t.sign = axial >= 0.0f ? 1.0f : -1.0f;
	t.distance = axial < 0.0f ? -axial : axial;
}

// Whether it still holds this frame; clears the state when not.
inline bool TwoHandHolds(TwoHandState& t, const TwoHandSettings& s, bool twoHandedDrawn, bool leftGripDown,
                         float handsApartUnits) {
	if (!t.active) {
		return false;
	}
	const float change = handsApartUnits - t.distance;
	const bool held = s.enabled && twoHandedDrawn && leftGripDown && handsApartUnits == handsApartUnits &&
	                  (change < 0.0f ? -change : change) <= s.slackUnits;
	if (!held) {
		t = TwoHandState{};
	}
	return held;
}

// The weapon's direction while held with both hands: along the line from the
// right hand to the left, or against it. Zero when the hands are in one place.
inline NiPoint3 TwoHandDirection(const TwoHandState& t, const NiPoint3& right, const NiPoint3& left) {
	NiPoint3 d = left - right;
	const float length = math::Sqrt(d.LengthSquared());
	if (!(length > 0.001f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return d * (t.sign / length);
}

// Where the left hand is held: on the line, at the distance it took hold.
inline NiPoint3 TwoHandLeftAt(const TwoHandState& t, const NiPoint3& right, const NiPoint3& direction) {
	return right + direction * (t.sign * t.distance);
}

// The smallest rotation that turns the unit vector `from` onto the unit
// vector `to` (Rodrigues). Opposite vectors turn half round an axis across
// `from`; a vector of no length leaves everything as it is.
inline NiMatrix33 RotationBetween(const NiPoint3& from, const NiPoint3& to) {
	const float fl = math::Sqrt(from.LengthSquared());
	const float tl = math::Sqrt(to.LengthSquared());
	if (!(fl > 1e-6f) || !(tl > 1e-6f)) {
		return NiMatrix33::Identity();
	}
	const NiPoint3 a = from * (1.0f / fl);
	const NiPoint3 b = to * (1.0f / tl);
	const float c = a.x * b.x + a.y * b.y + a.z * b.z;
	NiPoint3 v{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
	if (c < -0.9999f) {
		// Half round an axis across `a`.
		NiPoint3 axis = (a.x < 0.9f && a.x > -0.9f) ? NiPoint3{0.0f, -a.z, a.y} : NiPoint3{a.z, 0.0f, -a.x};
		const float al = math::Sqrt(axis.LengthSquared());
		axis = axis * (1.0f / al);
		NiMatrix33 m{};
		m.data[0][0] = 2.0f * axis.x * axis.x - 1.0f;
		m.data[0][1] = 2.0f * axis.x * axis.y;
		m.data[0][2] = 2.0f * axis.x * axis.z;
		m.data[1][0] = 2.0f * axis.y * axis.x;
		m.data[1][1] = 2.0f * axis.y * axis.y - 1.0f;
		m.data[1][2] = 2.0f * axis.y * axis.z;
		m.data[2][0] = 2.0f * axis.z * axis.x;
		m.data[2][1] = 2.0f * axis.z * axis.y;
		m.data[2][2] = 2.0f * axis.z * axis.z - 1.0f;
		return m;
	}
	const float k = 1.0f / (1.0f + c);
	NiMatrix33 m{};
	m.data[0][0] = c + v.x * v.x * k;
	m.data[0][1] = v.x * v.y * k - v.z;
	m.data[0][2] = v.x * v.z * k + v.y;
	m.data[1][0] = v.y * v.x * k + v.z;
	m.data[1][1] = c + v.y * v.y * k;
	m.data[1][2] = v.y * v.z * k - v.x;
	m.data[2][0] = v.z * v.x * k - v.y;
	m.data[2][1] = v.z * v.y * k + v.x;
	m.data[2][2] = c + v.z * v.z * k;
	return m;
}

inline NiMatrix33 Transposed(const NiMatrix33& m) {
	NiMatrix33 t{};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			t.data[r][c] = m.data[c][r];
		}
	}
	return t;
}

}  // namespace obvr::vr
