#pragma once

// The drawn weapon's shape fitted to its model ([Hands] FittedWeaponShapes;
// Nadi, 2026-10-27: the blade rests 1.5 units off everyone, and "dann
// müssen wir eig auch alle waffen nach mesh berechnen wie wirs für rüstungen
// tun"). Until now a weapon was one line from its node to the far side of
// its bound's ball, with no width or thickness - the contact margin stood
// in for both, and a mace's or an axe's head was not there at all.
//
// From the model's vertices in the weapon node's own space (its y axis is
// the blade's, BladeSpanFromNode): the line from the node to the furthest
// vertex along the axis, through the middle of the blade's section; the
// section's broad direction (the edge) and its half width and thickness,
// each the median of the slices between a fifth of the way and the head or
// the tip - a guard, a grip or a tapered point do not set them; and a head,
// where the last part's slices stand out round the axis well past the
// grip's (a mace, an axe, a hammer), as a line of its own along the axis
// through its section's middle with its own section - flat for an axe's
// blade, round for a mace's flanges. Pure, tested without the game
// (weapon_shape_test).

#include <algorithm>
#include <vector>

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The slices along the weapon the section and the head are read in.
inline constexpr UInt32 kWeaponSlices = 16;
// The section is read from here on (a share of the length): past the grip
// and most guards.
inline constexpr float kWeaponSectionFrom = 0.2f;
// The shaft a head is measured against: the thinnest slice of the first
// half - a grip or a haft (a median took an iron mace's long head for the
// shaft, the quantile 0.25 the iron war axe's blade, 70 % of its length:
// 9.8 round against its 1.3 grip, 2026-10-27).
inline constexpr float kWeaponShaftUpTo = 0.5f;
// A head: the slices up to the tip - one of the last kWeaponHeadNearTip -
// that stand out round the axis this many times the shaft's reach and at
// least this many units more. A guard stands out too, but far from the tip.
inline constexpr UInt32 kWeaponHeadNearTip = 3;
inline constexpr float kWeaponHeadOverShaft = 2.2f;
inline constexpr float kWeaponHeadMinOverUnits = 2.5f;
// A head whose section is less flat than this is round (a mace's flanges):
// its reach the same every way.
inline constexpr float kWeaponHeadRoundUnder = 1.3f;
// Fewer vertices than this past the node and nothing is fitted.
inline constexpr UInt32 kWeaponMinVertices = 12;

struct WeaponShape {
	bool valid = false;
	// The line from the node's end to the tip, through the section's middle;
	// in the node's space.
	NiPoint3 a{0.0f, 0.0f, 0.0f};
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	BladeSection section;  // in the node's space
	bool head = false;
	// the head's line along the axis through its section's middle, from
	// where it stands out to the tip, and its section: flat (an axe's
	// blade) or round (a mace's flanges, halfWidth every way)
	NiPoint3 headA{0.0f, 0.0f, 0.0f};
	NiPoint3 headB{0.0f, 0.0f, 0.0f};
	BladeSection headSection;
	float headRadius = 0.0f;  // the head's furthest reach from its line, for the log
	// for the log
	UInt32 vertices = 0;
	float length = 0.0f;     // the tip along the axis
	float shaftReach = 0.0f;  // the thinnest slice's reach of the first half
	float sliceReach[kWeaponSlices] = {};  // each slice's reach round the axis, 0 for an empty one
};

namespace weapon_detail {

inline float Median(std::vector<float>& v) {
	if (v.empty()) {
		return 0.0f;
	}
	const size_t mid = v.size() / 2;
	std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
	return v[mid];
}

inline NiPoint3 Unit(const NiPoint3& v, const NiPoint3& fallback) {
	const float l = math::Sqrt(v.LengthSquared());
	return l > 1e-6f ? v * (1.0f / l) : fallback;
}

// The section across `axis` of the vertices between `from` and `to` along
// it: their middle (across the axis), their broad direction, and each
// slice's half extents along it and across it, the medians of the slices.
// False with fewer than three vertices there.
inline bool SectionAcross(const NiPoint3* pts, UInt32 n, const NiPoint3& axis, const NiPoint3& u1, const NiPoint3& u2,
                          float from, float to, float slice, UInt32 slices, BladeSection& out, NiPoint3& middle) {
	std::vector<float> xs;
	std::vector<float> ys;
	std::vector<UInt32> ks;
	for (UInt32 i = 0; i < n; ++i) {
		const float t = DotOf3(pts[i], axis);
		if (t < from || t > to) {
			continue;
		}
		xs.push_back(DotOf3(pts[i], u1));
		ys.push_back(DotOf3(pts[i], u2));
		const UInt32 k = slice > 0.0f ? static_cast<UInt32>(t / slice) : 0u;
		ks.push_back(k < slices ? k : slices - 1);
	}
	if (xs.size() < 3) {
		return false;
	}
	const float count = static_cast<float>(xs.size());
	float cx = 0.0f;
	float cy = 0.0f;
	for (size_t i = 0; i < xs.size(); ++i) {
		cx += xs[i];
		cy += ys[i];
	}
	cx /= count;
	cy /= count;
	float sxx = 0.0f;
	float syy = 0.0f;
	float sxy = 0.0f;
	for (size_t i = 0; i < xs.size(); ++i) {
		const float x = xs[i] - cx;
		const float y = ys[i] - cy;
		sxx += x * x;
		syy += y * y;
		sxy += x * y;
	}
	const float angle = 0.5f * math::Atan2(2.0f * sxy, sxx - syy);
	const float ca = math::Cos(angle);
	const float sa = math::Sin(angle);
	std::vector<float> wide(slices, 0.0f);
	std::vector<float> thick(slices, 0.0f);
	std::vector<bool> seen(slices, false);
	for (size_t i = 0; i < xs.size(); ++i) {
		const float x = xs[i] - cx;
		const float y = ys[i] - cy;
		const float w = x * ca + y * sa;
		const float h = -x * sa + y * ca;
		const UInt32 k = ks[i];
		const float aw = w < 0.0f ? -w : w;
		const float ah = h < 0.0f ? -h : h;
		wide[k] = !seen[k] || aw > wide[k] ? aw : wide[k];
		thick[k] = !seen[k] || ah > thick[k] ? ah : thick[k];
		seen[k] = true;
	}
	std::vector<float> widths;
	std::vector<float> thicknesses;
	for (UInt32 k = 0; k < slices; ++k) {
		if (seen[k]) {
			widths.push_back(wide[k]);
			thicknesses.push_back(thick[k]);
		}
	}
	middle = u1 * cx + u2 * cy;
	out = BladeSection{};
	out.valid = true;
	out.major = Unit(u1 * ca + u2 * sa, u1);
	out.minor = Unit(CrossOf3(axis, out.major), u2);
	out.halfWidth = Median(widths);
	out.halfThickness = Median(thicknesses);
	return true;
}

}  // namespace weapon_detail

// Fits the shape to `n` vertices in the node's space along `axis` (the
// node's y). False - `out` not valid - with fewer than kWeaponMinVertices
// past the node or a length under kBladeMinUnits.
inline bool FitWeaponShape(const NiPoint3* pts, UInt32 n, const NiPoint3& axisIn, WeaponShape& out) {
	using weapon_detail::Unit;
	out = WeaponShape{};
	if (pts == nullptr) {
		return false;
	}
	const NiPoint3 axis = Unit(axisIn, NiPoint3{0.0f, 1.0f, 0.0f});
	// a plane across the axis
	const NiPoint3 helper = (axis.x < 0.9f && axis.x > -0.9f) ? NiPoint3{1.0f, 0.0f, 0.0f} : NiPoint3{0.0f, 0.0f, 1.0f};
	const NiPoint3 u1 = Unit(CrossOf3(axis, helper), NiPoint3{1.0f, 0.0f, 0.0f});
	const NiPoint3 u2 = CrossOf3(axis, u1);
	float length = 0.0f;
	UInt32 past = 0;
	for (UInt32 i = 0; i < n; ++i) {
		const float t = DotOf3(pts[i], axis);
		if (t > 0.0f) {
			++past;
			length = t > length ? t : length;
		}
	}
	if (past < kWeaponMinVertices || !(length >= kBladeMinUnits)) {
		return false;
	}
	const float slice = length / static_cast<float>(kWeaponSlices);
	auto sliceOf = [&](float t) {
		const UInt32 k = static_cast<UInt32>(t / slice);
		return k < kWeaponSlices ? k : kWeaponSlices - 1;
	};
	// each slice's reach round the axis
	float reach[kWeaponSlices] = {};
	bool any[kWeaponSlices] = {};
	for (UInt32 i = 0; i < n; ++i) {
		const float t = DotOf3(pts[i], axis);
		if (!(t > 0.0f)) {
			continue;
		}
		const UInt32 k = sliceOf(t);
		const NiPoint3 r = pts[i] - axis * t;
		const float d = math::Sqrt(r.LengthSquared());
		reach[k] = !any[k] || d > reach[k] ? d : reach[k];
		any[k] = true;
	}
	// the shaft: the thinnest slice of the first half
	float shaftReach = 0.0f;
	bool haveShaft = false;
	for (UInt32 k = 0; k < kWeaponSlices; ++k) {
		const float mid = (static_cast<float>(k) + 0.5f) / static_cast<float>(kWeaponSlices);
		if (any[k] && mid <= kWeaponShaftUpTo && (!haveShaft || reach[k] < shaftReach)) {
			shaftReach = reach[k];
			haveShaft = true;
		}
	}
	auto standsOut = [&](UInt32 k) {
		return any[k] && reach[k] > shaftReach * kWeaponHeadOverShaft && reach[k] > shaftReach + kWeaponHeadMinOverUnits;
	};
	// the head: from the highest slice near the tip that stands out, down
	// while they stand out
	float headFrom = -1.0f;
	int top = -1;
	for (UInt32 i = 0; i < kWeaponHeadNearTip && haveShaft; ++i) {
		const UInt32 k = kWeaponSlices - 1 - i;
		if (standsOut(k)) {
			top = static_cast<int>(k);
			break;
		}
	}
	if (top >= 0) {
		int k = top;
		while (k > 0 && standsOut(static_cast<UInt32>(k - 1))) {
			--k;
		}
		headFrom = static_cast<float>(k) * slice;
	}
	// the head's own section and line, from where it stands out to the tip:
	// flat (an axe's blade), or round when hardly flat (a mace's flanges)
	if (headFrom > 0.0f) {
		NiPoint3 headMiddle{0.0f, 0.0f, 0.0f};
		BladeSection hs;
		if (weapon_detail::SectionAcross(pts, n, axis, u1, u2, headFrom, length, slice, kWeaponSlices, hs, headMiddle)) {
			const float wider = hs.halfWidth > hs.halfThickness ? hs.halfWidth : hs.halfThickness;
			const float thinner = hs.halfWidth > hs.halfThickness ? hs.halfThickness : hs.halfWidth;
			if (!(thinner > 0.0f) || wider < thinner * kWeaponHeadRoundUnder) {
				hs.round = true;
				hs.halfWidth = wider;
				hs.halfThickness = wider;
			}
			out.head = true;
			out.headSection = hs;
			out.headA = headMiddle + axis * headFrom;
			out.headB = headMiddle + axis * length;
			out.headRadius = wider;
		}
	}
	// the line's section: from a fifth of the way to the head or the tip
	NiPoint3 middle{0.0f, 0.0f, 0.0f};
	// (short of the head's first vertices: they would pull its middle off).
	// A haft with no vertex between (the iron war axe's: a few long
	// triangles, its slices from a fifth of the way to its head empty,
	// 2026-10-27) is read from the node on, and with none at all it is
	// round with the shaft's reach
	const float sectionTo = out.head ? headFrom - 0.001f * length : length;
	if (!weapon_detail::SectionAcross(pts, n, axis, u1, u2, length * kWeaponSectionFrom, sectionTo, slice, kWeaponSlices,
	                                  out.section, middle) &&
	    !weapon_detail::SectionAcross(pts, n, axis, u1, u2, 0.0f, sectionTo, slice, kWeaponSlices, out.section,
	                                  middle)) {
		if (!out.head || !haveShaft) {
			return false;
		}
		out.section = BladeSection{};
		out.section.valid = true;
		out.section.round = true;
		out.section.halfWidth = shaftReach;
		out.section.halfThickness = shaftReach;
		middle = NiPoint3{0.0f, 0.0f, 0.0f};
	}
	out.a = middle;
	out.b = middle + axis * length;
	out.valid = true;
	out.vertices = n;
	out.length = length;
	out.shaftReach = shaftReach;
	for (UInt32 k = 0; k < kWeaponSlices; ++k) {
		out.sliceReach[k] = any[k] ? reach[k] : 0.0f;
	}
	return true;
}

}  // namespace obvr::game
