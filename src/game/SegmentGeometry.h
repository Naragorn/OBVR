#pragma once

// Points, segments and capsules: the distances and crossings every contact
// is measured with (game/BladeContactLogic.h, game/RadialField.h). Pure.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

inline float DotOf3(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline NiPoint3 CrossOf3(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// The closest point of segment a-b to p, as a share 0..1 along it.
inline float ShareOnSegment(const NiPoint3& p, const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 ab = b - a;
	const float abab = DotOf3(ab, ab);
	if (!(abab > 1e-12f)) {
		return 0.0f;
	}
	const float t = DotOf3(p - a, ab) / abab;
	return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

inline float PointSegmentDistance(const NiPoint3& p, const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 q = a + (b - a) * ShareOnSegment(p, a, b);
	return math::Sqrt((p - q).LengthSquared());
}

// A point inside a capsule: how deep (0 at the surface, more inside), and
// the normal out of it at the point (up when the point lies on the axis).
inline float CapsuleDepthAt(const NiPoint3& p, const NiPoint3& a, const NiPoint3& b, float radius,
                            NiPoint3& outNormal) {
	const NiPoint3 q = a + (b - a) * ShareOnSegment(p, a, b);
	const NiPoint3 off = p - q;
	const float d = math::Sqrt(off.LengthSquared());
	outNormal = d > 1e-4f ? off * (1.0f / d) : NiPoint3{0.0f, 0.0f, 1.0f};
	return radius - d;
}

// The closest points of two segments (Ericson, Real-Time Collision
// Detection, 5.1.9, clamped to both): on p1-q1 and on p2-q2.
inline void ClosestOnSegments(const NiPoint3& p1, const NiPoint3& q1, const NiPoint3& p2, const NiPoint3& q2,
                              NiPoint3& on1, NiPoint3& on2) {
	const NiPoint3 d1 = q1 - p1;
	const NiPoint3 d2 = q2 - p2;
	const NiPoint3 r = p1 - p2;
	const float a = DotOf3(d1, d1);
	const float e = DotOf3(d2, d2);
	const float f = DotOf3(d2, r);
	float s = 0.0f;
	float t = 0.0f;
	if (!(a > 1e-12f) && !(e > 1e-12f)) {
		on1 = p1;
		on2 = p2;
		return;
	}
	if (!(a > 1e-12f)) {
		t = f / e;
		t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	} else {
		const float c = DotOf3(d1, r);
		if (!(e > 1e-12f)) {
			s = -c / a;
			s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
		} else {
			const float b = DotOf3(d1, d2);
			const float denom = a * e - b * b;
			s = denom > 1e-12f ? (b * f - c * e) / denom : 0.0f;
			s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
			t = (b * s + f) / e;
			if (t < 0.0f) {
				t = 0.0f;
				s = -c / a;
				s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
			} else if (t > 1.0f) {
				t = 1.0f;
				s = (b - c) / a;
				s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
			}
		}
	}
	on1 = p1 + d1 * s;
	on2 = p2 + d2 * t;
}

// The distance between two segments.
inline float SegmentSegmentDistance(const NiPoint3& p1, const NiPoint3& q1, const NiPoint3& p2, const NiPoint3& q2) {
	NiPoint3 on1;
	NiPoint3 on2;
	ClosestOnSegments(p1, q1, p2, q2, on1, on2);
	return math::Sqrt((on1 - on2).LengthSquared());
}

// Where the segment from -> to first enters the capsule a-b of radius r: its
// fraction and the capsule's outward normal there. `inside` when `from` is
// already in it (then fraction 0). False when it never enters.
inline bool SegmentEntersCapsule(const NiPoint3& from, const NiPoint3& to, const NiPoint3& a, const NiPoint3& b,
                                 float r, float& fraction, NiPoint3& normal, bool& inside) {
	inside = false;
	if (!(r > 0.0f)) {
		return false;
	}
	if (PointSegmentDistance(from, a, b) <= r) {
		inside = true;
		fraction = 0.0f;
		normal = NiPoint3{0.0f, 0.0f, 1.0f};
		return true;
	}
	const NiPoint3 d = to - from;
	const float length = math::Sqrt(d.LengthSquared());
	if (!(length > 1e-6f)) {
		return false;
	}
	const NiPoint3 rd = d * (1.0f / length);
	float best = -1.0f;
	// The side: |(o + t rd - a) x (b - a)| = r |b - a|, between the ends.
	const NiPoint3 ba = b - a;
	const NiPoint3 oa = from - a;
	const float baba = DotOf3(ba, ba);
	if (baba > 1e-12f) {
		const float bard = DotOf3(ba, rd);
		const float baoa = DotOf3(ba, oa);
		const float qa = baba - bard * bard;
		const float qb = baba * DotOf3(rd, oa) - baoa * bard;
		const float qc = baba * DotOf3(oa, oa) - baoa * baoa - r * r * baba;
		const float h = qb * qb - qa * qc;
		if (qa > 1e-9f && h >= 0.0f) {
			const float t = (-qb - math::Sqrt(h)) / qa;
			const float y = baoa + t * bard;
			if (t >= 0.0f && y > 0.0f && y < baba) {
				best = t;
			}
		}
	}
	// The two round ends.
	const NiPoint3 ends[2] = {a, b};
	for (const NiPoint3& c : ends) {
		const NiPoint3 oc = from - c;
		const float hb = DotOf3(rd, oc);
		const float hc = DotOf3(oc, oc) - r * r;
		const float h = hb * hb - hc;
		if (h > 0.0f) {
			const float t = -hb - math::Sqrt(h);
			if (t >= 0.0f && (best < 0.0f || t < best)) {
				best = t;
			}
		}
	}
	if (!(best >= 0.0f) || best > length) {
		return false;
	}
	fraction = best / length;
	const NiPoint3 p = from + rd * best;
	const NiPoint3 q = a + ba * ShareOnSegment(p, a, b);
	const NiPoint3 out = p - q;
	const float outLength = math::Sqrt(out.LengthSquared());
	normal = outLength > 1e-6f ? out * (1.0f / outLength) : rd * -1.0f;
	return true;
}

}  // namespace obvr::game
