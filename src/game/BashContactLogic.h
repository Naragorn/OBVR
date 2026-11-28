#pragma once

// The shield bash's contact and its speed into the body (Nadi's headset run of
// 2026-11-28 14:56 on an imp: "ging anfangs nicht dann wieder schon").
//
// Until then the bash tested the shield's middle, a point, against the target's
// column (game::LivingActorAt(at, 0.5, 8, ..., 1): half their bound's radius
// plus 8 across, the radius plus 8 up and down), and judged the speed towards
// their middle, the bound's centre. Both read wrong on a small hovering one:
// - in the column the middle is near their axis, so the line to their middle
//   stands nearly upright and a level jab has little speed along it - 14:56
//   line 22427: 4 across and 14 below, 1.6 m/s across the ground, 0.8 towards
//   the middle; 23064: 25 across and 43 above, 3.7 across the ground, 2.4
//   towards the middle (60 degrees off);
// - a shield whose rim or face is on them while its middle is beside the
//   column was "never in their column" (23148: the middle 57 out of 46, the
//   approach 4.4 m/s).
// Now:
// - the contact is the shield's disc (its two half spokes in its plane, from
//   game/ShieldShape - an ellipse, not the outline's polygon) against the
//   column, by alternating projections onto the two (both convex): touching
//   when they come within kBashTouchUnits; or the contact step has the shield
//   on their body (its outline against their capsules), which holds it at a
//   body wider than the column (BashTouches);
// - the speed is into the body: the hand's velocity along the line from the
//   disc's nearest point to the column's nearest point while apart - the rate
//   the gap closes - and once touching along the inward normal of the
//   column's face nearest to that point; or towards their middle, whichever is
//   more, so a jab aimed at their middle keeps what it had (14:56, 23113,
//   landed: 2.7 into the side, 3.1 at the middle). A graze along them closes
//   nothing, a pull back is negative, a shield held still while they fly into
//   it is the hand's nothing.
// Pure, covered by bash_contact_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/ShieldBashLogic.h"

namespace obvr::game {

// The shield: its middle and its plane's two axes with the half spoke along
// each (0 and 0: the middle alone, a shield whose shape was not read).
struct BashDisc {
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	NiPoint3 axisA{1.0f, 0.0f, 0.0f};
	NiPoint3 axisB{0.0f, 1.0f, 0.0f};
	float halfA = 0.0f;
	float halfB = 0.0f;
};

// The target's upright column: its bound's centre, half its width and half its
// height.
struct BashColumn {
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	float wide = 0.0f;
	float high = 0.0f;
};

struct BashGap {
	bool touching = false;
	float gap = 0.0f;                        // units between the nearest points; 0 touching
	NiPoint3 onDisc{0.0f, 0.0f, 0.0f};
	NiPoint3 onColumn{0.0f, 0.0f, 0.0f};
};

constexpr float kBashColumnSide = 0.5f;  // of the bound's radius, across (the shove's HandAtBody)
constexpr float kBashColumnPad = 8.0f;
constexpr float kBashTouchUnits = 1.0f;
constexpr int kBashGapSteps = 12;  // alternating projections: within a unit for a disc against a column

inline BashColumn BashColumnOf(const NiPoint3& centre, float boundRadius) {
	return BashColumn{centre, boundRadius * kBashColumnSide + kBashColumnPad, boundRadius + kBashColumnPad};
}

inline NiPoint3 OntoColumn(const NiPoint3& p, const BashColumn& c) {
	NiPoint3 q = p;
	const float dx = p.x - c.centre.x;
	const float dy = p.y - c.centre.y;
	const float r = math::Sqrt(dx * dx + dy * dy);
	if (r > c.wide && r > 0.0f) {
		q.x = c.centre.x + dx * (c.wide / r);
		q.y = c.centre.y + dy * (c.wide / r);
	}
	const float top = c.centre.z + c.high;
	const float bottom = c.centre.z - c.high;
	q.z = p.z > top ? top : (p.z < bottom ? bottom : p.z);
	return q;
}

// Onto the disc's plane, then into its ellipse along the line from its middle
// (inside the ellipse, not the nearest point of its rim: close enough for the
// alternating steps, and never outside the shield).
inline NiPoint3 OntoDisc(const NiPoint3& p, const BashDisc& d) {
	const NiPoint3 off = p - d.centre;
	float a = d.halfA > 0.0f ? off.x * d.axisA.x + off.y * d.axisA.y + off.z * d.axisA.z : 0.0f;
	float b = d.halfB > 0.0f ? off.x * d.axisB.x + off.y * d.axisB.y + off.z * d.axisB.z : 0.0f;
	const float ea = d.halfA > 0.0f ? a / d.halfA : 0.0f;
	const float eb = d.halfB > 0.0f ? b / d.halfB : 0.0f;
	const float e = ea * ea + eb * eb;
	if (e > 1.0f) {
		const float s = 1.0f / math::Sqrt(e);
		a *= s;
		b *= s;
	}
	return d.centre + d.axisA * a + d.axisB * b;
}

inline BashGap DiscColumnGap(const BashDisc& d, const BashColumn& c) {
	BashGap g;
	NiPoint3 onDisc = d.centre;
	NiPoint3 onColumn = OntoColumn(onDisc, c);
	for (int i = 0; i < kBashGapSteps; ++i) {
		onDisc = OntoDisc(onColumn, d);
		onColumn = OntoColumn(onDisc, c);
	}
	const float gap = math::Sqrt((onColumn - onDisc).LengthSquared());
	if (!(gap == gap)) {
		return g;  // a figure not a number: no contact
	}
	g.onDisc = onDisc;
	g.onColumn = onColumn;
	g.touching = gap <= kBashTouchUnits;
	g.gap = g.touching ? 0.0f : gap;
	return g;
}

// The disc meets them: by its own test, or the contact step has the shield on
// their body.
inline bool BashTouches(const BashGap& g, bool contactOnThem) { return g.touching || contactOnThem; }

// The hand's velocity (m/s) into the column's surface from the disc: along the
// gap while apart; touching, along the inward normal of the face nearest to
// the touching point (on their axis: none).
inline float SurfaceClosingSpeed(const NiPoint3& v, const BashDisc& d, const BashColumn& c) {
	const BashGap g = DiscColumnGap(d, c);
	NiPoint3 n{0.0f, 0.0f, 0.0f};
	if (!g.touching) {
		const NiPoint3 line = g.onColumn - g.onDisc;
		const float length = math::Sqrt(line.LengthSquared());
		if (!(length > 0.0001f)) {
			return 0.0f;
		}
		n = line * (1.0f / length);
	} else {
		const NiPoint3& p = g.onDisc;
		const float dx = p.x - c.centre.x;
		const float dy = p.y - c.centre.y;
		const float r = math::Sqrt(dx * dx + dy * dy);
		const float side = c.wide - r;
		const float top = c.centre.z + c.high - p.z;
		const float bottom = p.z - (c.centre.z - c.high);
		if (side <= top && side <= bottom) {
			if (!(r > 0.5f)) {
				return 0.0f;
			}
			n = NiPoint3{-dx / r, -dy / r, 0.0f};
		} else {
			n = NiPoint3{0.0f, 0.0f, top < bottom ? -1.0f : 1.0f};
		}
	}
	const float s = v.x * n.x + v.y * n.y + v.z * n.z;
	return s == s ? s : 0.0f;
}

// The bash's speed into the body: into its surface or at its middle, the more.
inline float BashClosingSpeed(const NiPoint3& v, const BashDisc& d, const BashColumn& c) {
	const float surface = SurfaceClosingSpeed(v, d, c);
	const float middle = BashSpeedTowards(v, d.centre, c.centre);
	const float s = surface > middle ? surface : middle;
	return s == s ? s : 0.0f;
}

// Degrees between the travel and the line from the shield's middle to theirs,
// for the attempt's line; no travel or no line: 0.
inline float TravelDegreesOffMiddle(const NiPoint3& v, const NiPoint3& at, const NiPoint3& centre) {
	const NiPoint3 d = centre - at;
	const float lv = math::Sqrt(v.LengthSquared());
	const float ld = math::Sqrt(d.LengthSquared());
	if (!(lv > 0.0001f) || !(ld > 0.0001f)) {
		return 0.0f;
	}
	const float dot = v.x * d.x + v.y * d.y + v.z * d.z;
	const NiPoint3 cross{v.y * d.z - v.z * d.y, v.z * d.x - v.x * d.z, v.x * d.y - v.y * d.x};
	const float degrees = math::Atan2(math::Sqrt(cross.LengthSquared()), dot) * 57.29578f;
	return degrees == degrees ? degrees : 0.0f;
}

}  // namespace obvr::game
