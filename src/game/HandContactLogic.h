#pragma once

#include <cmath>

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/HandBodyLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The free hand and the worn shield meet the world as the drawn weapon does
// (docs/weapon-collision-spec.md, section 14; the tester, 2026-10-11: "Gib
// nun einhand Waffe und Schild oder freier Hand Kollisionen. Same für alle
// anderen Waffen mit der freien Hand"): the weapon's contact step
// (BladeContactLogic.h, StepBladeContact) runs for each hand the weapon is
// not drawn in, with the hand's own span - the wrist to the fingertips, the
// one its Havok body has (HandSpanFromBones) - and, with a shield on the off
// hand's arm, with the shield's disc instead: two spokes through its centre
// (ShieldDisc). Walls and the floor hold them, held slowly they rest on
// people, a hand fast enough for a shove goes through, and pulled further
// than the weapon's caps they let go. Pure, covered by hand_contact_test.

// ------------------------------------------------------------- which hand

enum class HandShape : UInt8 { None, Hand, Shield };

inline const char* HandShapeName(HandShape s) {
	switch (s) {
	case HandShape::Hand: return "the hand";
	case HandShape::Shield: return "the shield";
	default: return "none";
	}
}

struct FreeHandFrame {
	bool valid = false;        // tracked, first person, in the world, no menu, not adjusting
	bool weaponHand = false;   // the hand the weapon is drawn in (the right role)
	bool meleeDrawn = false;   // a swung weapon is drawn: the weapon hand is the blade's
	bool bowDrawn = false;     // a bow is drawn: one hand holds it, the other the string
	bool onHandle = false;     // the off hand holds a two-hander's handle, or is about to
	bool holdsObject = false;  // the grab's object is in this hand
	bool leads = false;        // leading someone by this hand
	bool grapples = false;     // holding a fighter with this hand
	bool shieldWorn = false;   // a shield on the off hand's arm
};

// A hand busy with something is left to it: the weapon hand with a swung
// weapon drawn has the blade's own step; with a bow drawn one hand holds it
// and the other draws it; a hand on a handle, round an object, leading or
// holding someone follows what it holds. The off hand with a shield on its
// arm meets the world as the shield. With nothing drawn, or a staff, a bow
// away or a torch, both hands are hands (the staff and the torch pass
// through: only the hand itself is swept).
inline HandShape FreeHandShape(const FreeHandFrame& f) {
	if (!f.valid || f.bowDrawn || f.onHandle || f.holdsObject || f.leads || f.grapples) {
		return HandShape::None;
	}
	if (f.weaponHand && f.meleeDrawn) {
		return HandShape::None;
	}
	return !f.weaponHand && f.shieldWorn ? HandShape::Shield : HandShape::Hand;
}

// The hand is held this far short of a surface: its capsule's radius
// (HandBodyLogic.h), about a hand's half width.
inline constexpr float kHandContactMarginUnits = kHandBodyRadiusUnits;

// A hand fast enough to go through someone rather than come to rest on
// them: a fist's swing in progress, or the hand at the shove's speed
// (ShoveSettings::speed, m/s) - the speed that shoves or slaps them anyway.
inline bool HandSwinging(float speedMetresPerSecond, float shoveSpeed, bool fistSwing) {
	return fistSwing || (shoveSpeed > 0.0f && speedMetresPerSecond >= shoveSpeed);
}

// The speed that is judged: not one frame's velocity - a scripted hand
// jumps a step in one frame (the harness, 2026-10-11: 10 cm steps read 3 to
// 11 m/s and went through the beggar they should have rested on), and a
// tracker glitches - but the way come over the last kHandSpeedWindowSeconds
// over that time: each frame's speed times its time, summed over the
// window. A real slap at 2 m/s reads so within a tenth of a second; a hand
// put somewhere and left there reads its step over the window and no more.
inline constexpr float kHandSpeedWindowSeconds = 0.1f;
inline constexpr UInt32 kHandTravelEntries = 32;

struct HandTravel {
	UInt32 count = 0;
	UInt32 next = 0;
	float seconds[kHandTravelEntries] = {};
	float metres[kHandTravelEntries] = {};

	void Clear() {
		count = 0;
		next = 0;
	}

	// This frame: its length and the hand's speed in it (m/s).
	void Add(float dtSeconds, float speedMetresPerSecond) {
		if (!(dtSeconds > 0.0f)) {
			return;
		}
		const float speed = speedMetresPerSecond > 0.0f ? speedMetresPerSecond : 0.0f;
		seconds[next] = dtSeconds;
		metres[next] = speed * dtSeconds;
		next = (next + 1) % kHandTravelEntries;
		count = count < kHandTravelEntries ? count + 1 : kHandTravelEntries;
	}

	// The way come over the last `window` seconds, over that time; the
	// frames kept cover less than half the window (just started): 0.
	float MeanSpeed(float window) const {
		if (!(window > 0.0f) || count == 0) {
			return 0.0f;
		}
		float time = 0.0f;
		float way = 0.0f;
		for (UInt32 back = 0; back < count && time < window; ++back) {
			const UInt32 i = (next + kHandTravelEntries - 1 - back) % kHandTravelEntries;
			const float take = seconds[i] < window - time ? seconds[i] : window - time;
			way += seconds[i] > 0.0f ? metres[i] * (take / seconds[i]) : 0.0f;
			time += take;
		}
		if (time < window * 0.5f) {
			return 0.0f;
		}
		return way / time;
	}
};

// The keys the span cache tells the shapes apart by (StepBladeSpanCache
// starts anew when the key changes).
inline constexpr UInt32 kSpanKeyNone = 0;
inline constexpr UInt32 kSpanKeyHand = 1;
inline constexpr UInt32 kSpanKeyShieldDisc = 2;
inline constexpr UInt32 kSpanKeyShieldBall = 3;

// ------------------------------------------------------------- the shield
//
// The shield as the step meets it: a disc in the hand's frame - its centre,
// two spokes across it and its normal (unit), and how far it reaches along
// each: the spokes' half-lengths and half its thickness. Read from the
// shield model's vertices under the forearm's twist bone (game/ShieldShape):
// the disc's plane is the two directions the vertices spread most in, the
// normal the one they spread least in (the principal axes of their scatter),
// and the reach along each is measured in a second pass. A rim and a boss do
// not move the plane. The bound's ball (ParryLogic.h, ShieldBall) would hold
// the shield a hand's length short of a wall it faces, so it only stands in
// when the model cannot be read.

struct DiscScatter {
	UInt32 count = 0;
	double sum[3] = {0.0, 0.0, 0.0};
	double product[3][3] = {};

	void Add(const NiPoint3& p) {
		const double c[3] = {p.x, p.y, p.z};
		for (int i = 0; i < 3; ++i) {
			sum[i] += c[i];
			for (int j = 0; j < 3; ++j) {
				product[i][j] += c[i] * c[j];
			}
		}
		++count;
	}
};

inline double AbsD(double x) { return x < 0.0 ? -x : x; }

// The eigenvectors and eigenvalues of a symmetric 3x3 matrix by Jacobi's
// rotations (the cyclic method, Numerical Recipes' jacobi cut to 3x3): `a`
// is destroyed, `v` gets the eigenvectors as its columns, `d` the
// eigenvalues.
inline void JacobiEigen3(double a[3][3], double v[3][3], double d[3]) {
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			v[i][j] = i == j ? 1.0 : 0.0;
		}
		d[i] = a[i][i];
	}
	for (int sweep = 0; sweep < 50; ++sweep) {
		const double off = AbsD(a[0][1]) + AbsD(a[0][2]) + AbsD(a[1][2]);
		if (off < 1e-18) {
			break;
		}
		for (int p = 0; p < 2; ++p) {
			for (int q = p + 1; q < 3; ++q) {
				const double apq = a[p][q];
				if (AbsD(apq) < 1e-300) {
					continue;
				}
				const double theta = (d[q] - d[p]) / (2.0 * apq);
				const double t = (theta >= 0.0 ? 1.0 : -1.0) / (AbsD(theta) + std::sqrt(theta * theta + 1.0));
				const double c = 1.0 / std::sqrt(t * t + 1.0);
				const double s = t * c;
				d[p] -= t * apq;
				d[q] += t * apq;
				a[p][q] = 0.0;
				a[q][p] = 0.0;
				const int r = 3 - p - q;  // the third index
				const double arp = a[r][p];
				const double arq = a[r][q];
				a[r][p] = c * arp - s * arq;
				a[p][r] = a[r][p];
				a[r][q] = s * arp + c * arq;
				a[q][r] = a[r][q];
				for (int k = 0; k < 3; ++k) {
					const double vkp = v[k][p];
					const double vkq = v[k][q];
					v[k][p] = c * vkp - s * vkq;
					v[k][q] = s * vkp + c * vkq;
				}
			}
		}
	}
}

inline NiPoint3 CrossOf3(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline NiPoint3 Normalised(const NiPoint3& v, const NiPoint3& fallback) {
	const float l = math::Sqrt(v.LengthSquared());
	return l > 1e-6f ? v * (1.0f / l) : fallback;
}

// Fewer vertices than this are no model to read a disc from.
inline constexpr UInt32 kDiscMinVertices = 8;

// The scatter's principal axes, the largest spread first, with the spread
// (the variance) along each; the third axis is made the cross of the first
// two, a right-handed set. False with too few vertices or no spread at all.
inline bool DiscAxesFrom(const DiscScatter& s, NiPoint3& centre, NiPoint3 axes[3], float spread[3]) {
	if (s.count < kDiscMinVertices) {
		return false;
	}
	const double n = static_cast<double>(s.count);
	const double mean[3] = {s.sum[0] / n, s.sum[1] / n, s.sum[2] / n};
	double cov[3][3];
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			cov[i][j] = s.product[i][j] / n - mean[i] * mean[j];
		}
	}
	double v[3][3];
	double d[3];
	JacobiEigen3(cov, v, d);
	int order[3] = {0, 1, 2};
	for (int i = 0; i < 2; ++i) {
		for (int j = i + 1; j < 3; ++j) {
			if (d[order[j]] > d[order[i]]) {
				const int t = order[i];
				order[i] = order[j];
				order[j] = t;
			}
		}
	}
	for (int k = 0; k < 3; ++k) {
		const int c = order[k];
		axes[k] = NiPoint3{static_cast<float>(v[0][c]), static_cast<float>(v[1][c]), static_cast<float>(v[2][c])};
		spread[k] = static_cast<float>(d[c]);
	}
	if (!(spread[0] > 1e-6f) || !(spread[1] > 1e-6f)) {
		return false;
	}
	axes[0] = Normalised(axes[0], NiPoint3{1.0f, 0.0f, 0.0f});
	axes[1] = Normalised(axes[1], NiPoint3{0.0f, 1.0f, 0.0f});
	axes[2] = Normalised(CrossOf3(axes[0], axes[1]), NiPoint3{0.0f, 0.0f, 1.0f});
	centre = NiPoint3{static_cast<float>(mean[0]), static_cast<float>(mean[1]), static_cast<float>(mean[2])};
	return true;
}

// The second pass: how far the vertices reach along the axes found, from
// the centre.
struct DiscExtent {
	UInt32 count = 0;
	float low[3] = {0.0f, 0.0f, 0.0f};
	float high[3] = {0.0f, 0.0f, 0.0f};

	void Add(const NiPoint3& p, const NiPoint3& centre, const NiPoint3* axes) {
		const NiPoint3 d = p - centre;
		for (int k = 0; k < 3; ++k) {
			const float a = DotOf3(d, axes[k]);
			if (count == 0 || a < low[k]) {
				low[k] = a;
			}
			if (count == 0 || a > high[k]) {
				high[k] = a;
			}
		}
		++count;
	}
};

struct ShieldDisc {
	bool valid = false;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};  // in the hand's frame
	NiPoint3 axis[3] = {NiPoint3{1.0f, 0.0f, 0.0f}, NiPoint3{0.0f, 1.0f, 0.0f}, NiPoint3{0.0f, 0.0f, 1.0f}};
	float half[3] = {0.0f, 0.0f, 0.0f};  // the reach along each from the centre
	UInt32 vertices = 0;
};

// Only a shield's size and flatness: the spokes 6 to 60 units from the
// centre (a buckler to a tower shield), the thickness under 0.6 of the
// short spoke. A bare twist bone has no vertices; an odd model is refused,
// and the ball stands in.
inline constexpr float kShieldSpokeMinUnits = 6.0f;
inline constexpr float kShieldSpokeMaxUnits = 60.0f;
inline constexpr float kShieldFlatness = 0.6f;

// The disc from the axes and a finished extent: its centre moved to the
// middle of the extents, the half-lengths half of them.
inline bool ShieldDiscFrom(const NiPoint3& centre, const NiPoint3 axes[3], const DiscExtent& e, ShieldDisc& out) {
	out = ShieldDisc{};
	if (e.count < kDiscMinVertices) {
		return false;
	}
	NiPoint3 middle = centre;
	float half[3];
	for (int k = 0; k < 3; ++k) {
		half[k] = (e.high[k] - e.low[k]) * 0.5f;
		middle = middle + axes[k] * ((e.high[k] + e.low[k]) * 0.5f);
	}
	if (!(half[0] >= kShieldSpokeMinUnits && half[0] <= kShieldSpokeMaxUnits) ||
	    !(half[1] >= kShieldSpokeMinUnits && half[1] <= kShieldSpokeMaxUnits)) {
		return false;
	}
	if (!(half[2] <= half[1] * kShieldFlatness)) {
		return false;
	}
	out.valid = true;
	out.centre = middle;
	for (int k = 0; k < 3; ++k) {
		out.axis[k] = axes[k];
		out.half[k] = half[k];
	}
	out.vertices = e.count;
	return true;
}

// The shape the step sweeps for a disc: its two spokes end to end, and the
// margin it is held short of a surface by - half its thickness and a
// little, never under the blade's and never over kShieldMarginMaxUnits (a
// boss would hold the rim far off a wall).
inline constexpr float kShieldMarginMaxUnits = 4.0f;

inline void ShieldDiscSpans(const ShieldDisc& d, BodySpan& a, BodySpan& b, float& marginUnits) {
	a.valid = d.valid;
	b.valid = d.valid;
	a.a = d.centre - d.axis[0] * d.half[0];
	a.b = d.centre + d.axis[0] * d.half[0];
	b.a = d.centre - d.axis[1] * d.half[1];
	b.b = d.centre + d.axis[1] * d.half[1];
	float m = d.half[2] + 1.0f;
	m = m < kBladeContactMarginUnits ? kBladeContactMarginUnits : (m > kShieldMarginMaxUnits ? kShieldMarginMaxUnits : m);
	marginUnits = m;
}

// The ball that stands in when the model cannot be read: one point at the
// bound's centre, held the ball's radius short of a surface.
inline void ShieldBallSpan(const NiPoint3& centreLocal, float ballRadius, BodySpan& a, float& marginUnits) {
	a.valid = ballRadius > 0.0f;
	a.a = centreLocal;
	a.b = centreLocal;
	marginUnits = ballRadius;
}

// The disc in the world, as the stoppers take it (BladeContactLogic.h,
// BladeDisc): the hand's (or the twist bone's) pose carries it out of its
// frame; round, the longer spoke's size - a kite shield's short sides stop a
// blade a little early rather than let it through; half its thickness and
// a little, never under the blade's own margin.
inline BladeDisc ShieldDiscInWorld(const ShieldDisc& d, const NiMatrix33& rot, const NiPoint3& pos, UInt32 id) {
	BladeDisc out;
	out.centre = pos + rot * d.centre;
	out.normal = rot * d.axis[2];
	out.radius = d.half[0] > d.half[1] ? d.half[0] : d.half[1];
	const float half = d.half[2] + 0.5f;
	out.halfThick = half < kBladeContactMarginUnits ? kBladeContactMarginUnits : half;
	out.id = id;
	return out;
}

// The drawn blade as a stopper for the free hand: its span at the pose it
// is drawn, a capsule a little wider than its own body.
inline constexpr float kOwnBladeStopRadiusUnits = 2.5f;

inline bool AddOwnBladeStopper(const BladePose& drawn, const BodySpan& span, BladeBodies& out) {
	if (!span.valid) {
		return false;
	}
	return out.Add(BladePointAt(drawn, span, 0.0f), BladePointAt(drawn, span, 1.0f), kOwnBladeStopRadiusUnits,
	               kStopperOwnBlade);
}

// Whether the disc kept is still the shield worn: read again when the
// node or its bound change, or every so often.
inline constexpr UInt32 kShieldDiscReadFrames = 90;

inline bool ShieldDiscStale(bool have, UInt32 keptNode, UInt32 node, float keptBound, float bound, UInt32 age) {
	if (!have || keptNode != node) {
		return true;
	}
	const float d = keptBound - bound;
	return d > 0.5f || d < -0.5f || age >= kShieldDiscReadFrames;
}

}  // namespace obvr::game
