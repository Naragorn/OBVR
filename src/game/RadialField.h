#pragma once

// A fitted capsule's radial field (docs/mesh-fitted-capsules-spec.md 13,
// option C): the capsule's radius as the mesh has it, by direction - along
// its length and round its axis on the side, by tilt and round the axis on
// each round end. A capsule is round; the armour it stands for is not (a
// flat chest, a pauldron standing off the arm), and its one radius left the
// torso's front 3.5-4.9 units of air and the arms' ends 3.3-4.3 units into a
// mod's cuirass (armour-touch-*.txt, 2026-11-07). On the three outfits of
// tests/data, both ends included, 97-100 % of the surface within 3 units of
// the mesh, against 86-97 % for the round capsules (capsule_accuracy_test).
//
// The bins form one grid over a meridian, from one end's pole over its rim,
// down the side and over the other rim to the other pole (rows), and round
// the axis (sectors); a point's radius is read between the four bins round
// it (bilinear), so the surface is continuous over the rims. How the bins
// are measured: game/RadialFieldBuild.h.
//
// The shape queries below (Shape*) answer for a capsule with a field the
// same questions the round capsule's geometry answers (SegmentGeometry.h),
// and fall back to it when there is none. Pure.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "game/SegmentGeometry.h"

namespace obvr::game {

inline constexpr UInt32 kFieldSlices = 8;    // along the side
inline constexpr UInt32 kFieldSectors = 24;  // round the axis, 15 degrees each
inline constexpr UInt32 kFieldCapRings = 3;  // per round end, from its rim to its pole
inline constexpr UInt32 kFieldRows = kFieldSlices + 2 * kFieldCapRings;
inline constexpr UInt32 kFieldBins = kFieldRows * kFieldSectors;

struct RadialField {
	bool built = false;
	// The field's frame in the space it was built in: sector 0's direction,
	// square to the axis (the next sectors turn from it toward axis x u),
	// and the axis a ball stands on - a capsule of no length has none of its
	// own, and the space's up is not the bone's once placed.
	NiPoint3 u{1.0f, 0.0f, 0.0f};
	NiPoint3 w{0.0f, 0.0f, 1.0f};
	float radius[kFieldBins] = {};
	float outer = 0.0f;    // the largest radius: the capsule round it all
	UInt32 measured = 0;   // bins the mesh gave (the rest keep the capsule's radius)
};

// Where a field's capsule is: its ends, its frame (RadialField's u and w)
// in the space it is read in, and how its radii scale there (the frame
// bone's scale).
struct FieldPlace {
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	NiPoint3 u{1.0f, 0.0f, 0.0f};
	float scale = 1.0f;
	NiPoint3 w{0.0f, 0.0f, 1.0f};
};

// A field read in the space it was built in.
inline FieldPlace HomePlace(const RadialField& f, const NiPoint3& a, const NiPoint3& b) {
	return FieldPlace{a, b, f.u, 1.0f, f.w};
}

// The capsule's axis as a unit direction (`ball` for a ball) and its length.
inline NiPoint3 FieldAxis(const NiPoint3& a, const NiPoint3& b, const NiPoint3& ball, float& length) {
	const NiPoint3 ab = b - a;
	length = math::Sqrt(ab.LengthSquared());
	return length > 1e-4f ? ab * (1.0f / length) : ball;
}

// A direction square to the axis a-b (`ball` for a ball), the same for the
// same capsule: sector 0 of a field built on it.
inline NiPoint3 FieldSectorZero(const NiPoint3& a, const NiPoint3& b, const NiPoint3& ball) {
	float length = 0.0f;
	const NiPoint3 dir = FieldAxis(a, b, ball, length);
	const NiPoint3 helper = (dir.z < 0.9f && dir.z > -0.9f) ? NiPoint3{0.0f, 0.0f, 1.0f} : NiPoint3{1.0f, 0.0f, 0.0f};
	const NiPoint3 u = CrossOf3(dir, helper);
	return u * (1.0f / math::Sqrt(u.LengthSquared()));
}

// A point's lane: the foot it is measured from (on the axis, or the end
// past it), the unit direction out through it, its distance from the foot,
// and where it falls in the grid (fractional row and sector, bin centres
// at whole numbers).
struct FieldLane {
	NiPoint3 foot{0.0f, 0.0f, 0.0f};
	NiPoint3 out{0.0f, 0.0f, 1.0f};
	float pointRadius = 0.0f;
	float row = 0.0f;
	float sector = 0.0f;
};

inline FieldLane FieldLaneOf(const NiPoint3& p, const FieldPlace& place) {
	const NiPoint3& a = place.a;
	const NiPoint3& b = place.b;
	const NiPoint3& u = place.u;
	float length = 0.0f;
	const NiPoint3 dir = FieldAxis(place.a, place.b, place.w, length);
	const NiPoint3 v = CrossOf3(dir, u);
	const float s = DotOf3(p - a, dir);
	const NiPoint3 perp = (p - a) - dir * s;
	const float across = math::Sqrt(perp.LengthSquared());
	const float rings = static_cast<float>(kFieldCapRings);
	const float slices = static_cast<float>(kFieldSlices);
	FieldLane lane;
	if (s < 0.0f) {
		lane.foot = a;
		lane.row = rings - 0.5f - math::Atan2(-s, across) / math::kHalfPi * rings;
	} else if (s > length) {
		lane.foot = b;
		lane.row = rings + slices - 0.5f + math::Atan2(s - length, across) / math::kHalfPi * rings;
	} else {
		lane.foot = a + dir * s;
		lane.row = rings + (length > 1e-4f ? s / length : 0.5f) * slices - 0.5f;
	}
	const float turn = math::Atan2(DotOf3(perp, v), DotOf3(perp, u));
	lane.sector = (turn < 0.0f ? turn + 2.0f * math::kPi : turn) / (2.0f * math::kPi) * static_cast<float>(kFieldSectors) - 0.5f;
	const NiPoint3 off = p - lane.foot;
	lane.pointRadius = math::Sqrt(off.LengthSquared());
	lane.out = lane.pointRadius > 1e-4f ? off * (1.0f / lane.pointRadius) : (across > 1e-4f ? perp * (1.0f / across) : u);
	return lane;
}

// A bin's centre direction and foot: the inverse of FieldLaneOf.
inline void FieldBinLane(UInt32 row, UInt32 sector, const FieldPlace& place,
                         NiPoint3& foot, NiPoint3& out) {
	const NiPoint3& a = place.a;
	const NiPoint3& b = place.b;
	const NiPoint3& u = place.u;
	float length = 0.0f;
	const NiPoint3 dir = FieldAxis(place.a, place.b, place.w, length);
	const NiPoint3 v = CrossOf3(dir, u);
	const float turn = 2.0f * math::kPi * (static_cast<float>(sector) + 0.5f) / static_cast<float>(kFieldSectors);
	const NiPoint3 radial = u * math::Cos(turn) + v * math::Sin(turn);
	const float r = static_cast<float>(row);
	const float rings = static_cast<float>(kFieldCapRings);
	if (row < kFieldCapRings) {
		const float tilt = (rings - 0.5f - r) / rings * math::kHalfPi;
		foot = a;
		out = radial * math::Cos(tilt) - dir * math::Sin(tilt);
	} else if (row >= kFieldCapRings + kFieldSlices) {
		const float tilt = (r - rings - static_cast<float>(kFieldSlices) + 0.5f) / rings * math::kHalfPi;
		foot = b;
		out = radial * math::Cos(tilt) + dir * math::Sin(tilt);
	} else {
		foot = a + dir * (length * (r - rings + 0.5f) / static_cast<float>(kFieldSlices));
		out = radial;
	}
}

// The field's radius along a lane (bilinear between the bins round it; rows
// held at the poles, sectors round), in the field's own units.
inline float FieldRadiusOf(const RadialField& f, const FieldLane& lane) {
	const float rowMax = static_cast<float>(kFieldRows - 1);
	const float row = lane.row < 0.0f ? 0.0f : (lane.row > rowMax ? rowMax : lane.row);
	const UInt32 r0 = static_cast<UInt32>(row);
	const UInt32 r1 = r0 + 1 < kFieldRows ? r0 + 1 : r0;
	const float wr = row - static_cast<float>(r0);
	const float sectors = static_cast<float>(kFieldSectors);
	float sector = lane.sector;
	while (sector < 0.0f) {
		sector += sectors;
	}
	while (sector >= sectors) {
		sector -= sectors;
	}
	const UInt32 s0 = static_cast<UInt32>(sector) % kFieldSectors;
	const UInt32 s1 = (s0 + 1) % kFieldSectors;
	const float ws = sector - static_cast<float>(static_cast<UInt32>(sector));
	const float* R = f.radius;
	const float near0 = R[r0 * kFieldSectors + s0] * (1.0f - ws) + R[r0 * kFieldSectors + s1] * ws;
	const float near1 = R[r1 * kFieldSectors + s0] * (1.0f - ws) + R[r1 * kFieldSectors + s1] * ws;
	return near0 * (1.0f - wr) + near1 * wr;
}

// How far `p` lies outside the field's surface (below 0: inside), along its
// lane; the lane's outward direction in `normal`.
inline float FieldGapAt(const RadialField& f, const FieldPlace& place, const NiPoint3& p, NiPoint3* normal = nullptr) {
	const FieldLane lane = FieldLaneOf(p, place);
	if (normal != nullptr) {
		*normal = lane.out;
	}
	return lane.pointRadius - FieldRadiusOf(f, lane) * place.scale;
}

// Where along the lane through `p` the field's surface lies.
inline NiPoint3 FieldSurfaceAt(const RadialField& f, const FieldPlace& place, const NiPoint3& p) {
	const FieldLane lane = FieldLaneOf(p, place);
	return lane.foot + lane.out * (FieldRadiusOf(f, lane) * place.scale);
}

// ---------------------------------------------------------------- the shapes
//
// A body capsule as contact meets it: its round capsule a-b of `radius`,
// and the field it carries where one was built (FieldRef: the field, its
// sector 0 and its scale where the capsule is placed).

struct FieldRef {
	const RadialField* field = nullptr;
	NiPoint3 u{1.0f, 0.0f, 0.0f};
	float scale = 1.0f;
	NiPoint3 w{0.0f, 0.0f, 1.0f};  // the ball's axis, as FieldPlace's
};

// A segment is searched for its nearest approach to a field in steps of
// this along it, then by halving round the nearest step; only where it
// comes within kFieldNearUnits of the capsule round the whole field - past
// that, the bound's gap is answered (smaller than the field's: never a
// contact missed, the margins asked about are all within it).
inline constexpr float kFieldStepUnits = 1.0f;
inline constexpr float kFieldNearUnits = 12.0f;
inline constexpr UInt32 kFieldHalvings = 10;
inline constexpr UInt32 kFieldStepsMax = 128;

// The radius of the capsule round the whole shape.
inline float ShapeOuter(float radius, const FieldRef& ref) {
	if (ref.field == nullptr) {
		return radius;
	}
	const float outer = ref.field->outer * ref.scale;
	return outer > radius ? outer : radius;
}

// How far `p` lies outside the shape (below 0: inside), and the normal out
// of it there (up when `p` lies on a round capsule's axis).
inline float ShapeGapAt(const NiPoint3& a, const NiPoint3& b, float radius, const FieldRef& ref, const NiPoint3& p,
                        NiPoint3* normal = nullptr) {
	if (ref.field == nullptr) {
		NiPoint3 out;
		const float depth = CapsuleDepthAt(p, a, b, radius, out);
		if (normal != nullptr) {
			*normal = out;
		}
		return -depth;
	}
	return FieldGapAt(*ref.field, FieldPlace{a, b, ref.u, ref.scale, ref.w}, p, normal);
}

// The share along from-to of the part of it within `reach` of the axis a-b:
// [first, last]; false when none of it comes that near.
inline bool SegmentWithin(const NiPoint3& from, const NiPoint3& to, const NiPoint3& a, const NiPoint3& b, float reach,
                          float& first, float& last) {
	float f = 0.0f;
	NiPoint3 n;
	bool inside = false;
	if (!SegmentEntersCapsule(from, to, a, b, reach, f, n, inside)) {
		return false;
	}
	first = f;
	float back = 0.0f;
	bool insideBack = false;
	last = SegmentEntersCapsule(to, from, a, b, reach, back, n, insideBack) ? 1.0f - back : 1.0f;
	return true;
}

// The least gap between the segment from-to and the shape (below 0: into
// it), the segment's point where it is in `at`, and the direction out of
// the shape there in `normal` (zero when that point lies on a round
// capsule's axis).
inline float ShapeSegmentGap(const NiPoint3& a, const NiPoint3& b, float radius, const FieldRef& ref,
                             const NiPoint3& from, const NiPoint3& to, NiPoint3* at = nullptr,
                             NiPoint3* normal = nullptr) {
	NiPoint3 onSegment;
	NiPoint3 onAxis;
	ClosestOnSegments(from, to, a, b, onSegment, onAxis);
	const NiPoint3 off = onSegment - onAxis;
	const float offUnits = math::Sqrt(off.LengthSquared());
	const float outer = ShapeOuter(radius, ref);
	float first = 0.0f;
	float last = 0.0f;
	if (ref.field == nullptr || !SegmentWithin(from, to, a, b, outer + kFieldNearUnits, first, last)) {
		if (at != nullptr) {
			*at = onSegment;
		}
		if (normal != nullptr) {
			*normal = offUnits > 1e-6f ? off * (1.0f / offUnits) : NiPoint3{0.0f, 0.0f, 0.0f};
		}
		return offUnits - (ref.field == nullptr ? radius : outer);
	}
	const NiPoint3 way = to - from;
	const float span = (last - first) * math::Sqrt(way.LengthSquared());
	UInt32 steps = static_cast<UInt32>(span / kFieldStepUnits) + 1;
	steps = steps > kFieldStepsMax ? kFieldStepsMax : steps;
	const auto gapAt = [&](float share) { return ShapeGapAt(a, b, radius, ref, from + way * share); };
	float bestShare = first;
	float best = gapAt(first);
	for (UInt32 k = 1; k <= steps; ++k) {
		const float share = first + (last - first) * static_cast<float>(k) / static_cast<float>(steps);
		const float g = gapAt(share);
		if (g < best) {
			best = g;
			bestShare = share;
		}
	}
	// halve round the nearest step
	const float stepShare = (last - first) / static_cast<float>(steps);
	float lo = bestShare - stepShare < first ? first : bestShare - stepShare;
	float hi = bestShare + stepShare > last ? last : bestShare + stepShare;
	for (UInt32 h = 0; h < kFieldHalvings; ++h) {
		const float m1 = lo + (hi - lo) / 3.0f;
		const float m2 = hi - (hi - lo) / 3.0f;
		if (gapAt(m1) < gapAt(m2)) {
			hi = m2;
		} else {
			lo = m1;
		}
	}
	const float mid = 0.5f * (lo + hi);
	const float midGap = gapAt(mid);
	if (midGap < best) {
		best = midGap;
		bestShare = mid;
	}
	const NiPoint3 p = from + way * bestShare;
	NiPoint3 out;
	ShapeGapAt(a, b, radius, ref, p, &out);
	if (at != nullptr) {
		*at = p;
	}
	if (normal != nullptr) {
		*normal = out;
	}
	return best;
}

// Where the segment from -> to first comes within `grow` of the shape: its
// fraction and the direction out of the shape there. `inside` when `from`
// already is (then fraction 0 and the normal up, as SegmentEntersCapsule
// answers). False when it never comes that near.
inline bool ShapeSegmentEnters(const NiPoint3& a, const NiPoint3& b, float radius, const FieldRef& ref,
                               const NiPoint3& from, const NiPoint3& to, float grow, float& fraction, NiPoint3& normal,
                               bool& inside) {
	if (ref.field == nullptr) {
		return SegmentEntersCapsule(from, to, a, b, radius + grow, fraction, normal, inside);
	}
	inside = false;
	if (ShapeGapAt(a, b, radius, ref, from) <= grow) {
		inside = true;
		fraction = 0.0f;
		normal = NiPoint3{0.0f, 0.0f, 1.0f};
		return true;
	}
	float first = 0.0f;
	float last = 0.0f;
	if (!SegmentWithin(from, to, a, b, ShapeOuter(radius, ref) + grow, first, last)) {
		return false;
	}
	const NiPoint3 way = to - from;
	const float span = (last - first) * math::Sqrt(way.LengthSquared());
	UInt32 steps = static_cast<UInt32>(span / (0.5f * kFieldStepUnits)) + 1;
	steps = steps > kFieldStepsMax ? kFieldStepsMax : steps;
	float outside = first;
	for (UInt32 k = 0; k <= steps; ++k) {
		const float share = first + (last - first) * static_cast<float>(k) / static_cast<float>(steps);
		if (ShapeGapAt(a, b, radius, ref, from + way * share) > grow) {
			outside = share;
			continue;
		}
		float lo = outside;
		float hi = share;
		for (UInt32 h = 0; h < kFieldHalvings; ++h) {
			const float m = 0.5f * (lo + hi);
			if (ShapeGapAt(a, b, radius, ref, from + way * m) > grow) {
				lo = m;
			} else {
				hi = m;
			}
		}
		fraction = hi;
		ShapeGapAt(a, b, radius, ref, from + way * hi, &normal);
		return true;
	}
	return false;
}

}  // namespace obvr::game
