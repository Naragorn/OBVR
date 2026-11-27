#pragma once

// The worn shield as a plate - its outline (ShieldOutlineLogic.h) given its
// thickness - for the blows at a shield guarding by contact (Nadi,
// 2026-11-27: blocking "oddly too generous"; BlockAngleLogic.h,
// ShieldByContactVerdict). Without an outline the plate is the circle of the
// longer spoke, as the disc always was.
//
// Pure; shield_plate_test.

#include <cmath>

#include "core/Types.h"
#include "game/NiMath.h"
#include "game/ShieldOutlineLogic.h"

namespace obvr::game {

struct ShieldPlate {
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 1.0f};  // unit
	NiPoint3 axisA{1.0f, 0.0f, 0.0f};   // unit, in its plane: the outline's rows
	NiPoint3 axisB{0.0f, 1.0f, 0.0f};   // unit, in its plane: the rows' spans
	float radius = 0.0f;                // the circle, without an outline
	float halfThick = 0.0f;
	const ShieldOutline* outline = nullptr;  // null or not valid: the circle

	bool ByOutline() const { return outline != nullptr && outline->valid; }
};

inline float PlateDot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// How far a point in the plate's plane (its offset from the centre) lies
// outside the plate's face: 0 inside.
inline float PlateOutsideInPlane(const ShieldPlate& s, const NiPoint3& off) {
	if (s.ByOutline()) {
		const float d = OutlineDistance(*s.outline, PlateDot(off, s.axisA), PlateDot(off, s.axisB));
		return d > 0.0f ? d : 0.0f;
	}
	const float along = PlateDot(off, s.normal);
	const NiPoint3 radial = off - s.normal * along;
	const float r = std::sqrt(radial.LengthSquared()) - s.radius;
	return r > 0.0f ? r : 0.0f;
}

// The distance from a point to the plate (0 inside it).
inline float PointPlateDistance(const NiPoint3& p, const ShieldPlate& s) {
	const NiPoint3 off = p - s.centre;
	const float along = PlateDot(off, s.normal);
	const float beyond = (along < 0.0f ? -along : along) - s.halfThick;
	const float face = beyond > 0.0f ? beyond : 0.0f;
	const float side = PlateOutsideInPlane(s, off - s.normal * along);
	return std::sqrt(face * face + side * side);
}

// The distance from the segment a-b to the plate: the nearest of 17 points
// along it (a 50-unit blade sampled every 3 units; the error is under half
// a step).
inline constexpr UInt32 kPlateSamples = 17;

inline float SegmentPlateDistance(const NiPoint3& a, const NiPoint3& b, const ShieldPlate& s) {
	float best = -1.0f;
	for (UInt32 i = 0; i < kPlateSamples; ++i) {
		const float t = static_cast<float>(i) / static_cast<float>(kPlateSamples - 1);
		const float d = PointPlateDistance(a + (b - a) * t, s);
		best = best < 0.0f || d < best ? d : best;
	}
	return best;
}

// The point of the segment from-to nearest `p`.
inline NiPoint3 NearestOnSegment(const NiPoint3& p, const NiPoint3& from, const NiPoint3& to) {
	const NiPoint3 d = to - from;
	const float l2 = d.LengthSquared();
	if (!(l2 > 0.0f)) {
		return from;
	}
	float t = PlateDot(p - from, d) / l2;
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return from + d * t;
}

// Whether the plate stands between `from` (the blow: their blade's middle, or
// their centre) and `to` (the player's body): the segment crosses the
// plate's plane, both ends on either side of it, at a point within `margin`
// of the plate's face. `outside` gets that point's distance outside the
// face (0 within it); -1 when the segment does not cross the plane.
inline bool PlateBetween(const NiPoint3& from, const NiPoint3& to, const ShieldPlate& s, float margin,
                         float& outside) {
	outside = -1.0f;
	const float sa = PlateDot(from - s.centre, s.normal);
	const float sb = PlateDot(to - s.centre, s.normal);
	if ((sa > 0.0f) == (sb > 0.0f) || sa == sb) {
		return false;
	}
	const float t = sa / (sa - sb);
	const NiPoint3 x = from + (to - from) * t;
	outside = PlateOutsideInPlane(s, x - s.centre);
	return outside <= margin;
}

}  // namespace obvr::game
