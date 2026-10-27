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
// shaft's (a mace, an axe, a hammer), as a capsule of its own fitted to its
// vertices along the axis (CapsuleFitLogic.h). Pure, tested without the game
// (weapon_shape_test).

#include <algorithm>
#include <vector>

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/CapsuleFitLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// The slices along the weapon the section and the head are read in.
inline constexpr UInt32 kWeaponSlices = 16;
// The section is read from here on (a share of the length): past the grip
// and most guards.
inline constexpr float kWeaponSectionFrom = 0.2f;
// The shaft a head is measured against: the slices' reach at this
// quantile - the thinnest part, a grip or a haft (a median took an iron
// mace's long head and a war axe's blade for the shaft, 2026-10-27).
inline constexpr float kWeaponShaftQuantile = 0.25f;
// A head: the slices up to the tip - one of the last kWeaponHeadNearTip -
// that stand out round the axis this many times the shaft's reach and at
// least this many units more. A guard stands out too, but far from the tip.
inline constexpr UInt32 kWeaponHeadNearTip = 3;
inline constexpr float kWeaponHeadOverShaft = 2.2f;
inline constexpr float kWeaponHeadMinOverUnits = 2.0f;
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
	NiPoint3 headA{0.0f, 0.0f, 0.0f};
	NiPoint3 headB{0.0f, 0.0f, 0.0f};
	float headRadius = 0.0f;
	// for the log
	UInt32 vertices = 0;
	float length = 0.0f;     // the tip along the axis
	float shaftReach = 0.0f;  // the slices' reach round the axis at kWeaponShaftQuantile
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

}  // namespace weapon_detail

// Fits the shape to `n` vertices in the node's space along `axis` (the
// node's y). False - `out` not valid - with fewer than kWeaponMinVertices
// past the node or a length under kBladeMinUnits.
inline bool FitWeaponShape(const NiPoint3* pts, UInt32 n, const NiPoint3& axisIn, WeaponShape& out) {
	using weapon_detail::Median;
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
	std::vector<float> shaft;
	for (UInt32 k = 0; k < kWeaponSlices; ++k) {
		if (any[k]) {
			shaft.push_back(reach[k]);
		}
	}
	float shaftReach = 0.0f;
	if (!shaft.empty()) {
		std::sort(shaft.begin(), shaft.end());
		shaftReach = shaft[static_cast<size_t>(kWeaponShaftQuantile * static_cast<float>(shaft.size() - 1))];
	}
	auto standsOut = [&](UInt32 k) {
		return any[k] && reach[k] > shaftReach * kWeaponHeadOverShaft && reach[k] > shaftReach + kWeaponHeadMinOverUnits;
	};
	// the head: from the highest slice near the tip that stands out, down
	// while they stand out
	float headFrom = -1.0f;
	int top = -1;
	for (UInt32 i = 0; i < kWeaponHeadNearTip && shaftReach > 0.0f; ++i) {
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
	if (headFrom > 0.0f) {
		std::vector<NiPoint3> head;
		for (UInt32 i = 0; i < n; ++i) {
			if (DotOf3(pts[i], axis) >= headFrom) {
				head.push_back(pts[i]);
			}
		}
		// along the weapon's axis: a mace's head round the shaft, an axe's
		// edge along the haft - the line through the middle of its section
		const NiPoint3 node{0.0f, 0.0f, 0.0f};
		FitShape f;
		if (head.size() >= kWeaponMinVertices &&
		    FitCapsuleToPoints(head.data(), static_cast<UInt32>(head.size()), &node, &axis, f)) {
			out.head = true;
			out.headA = f.a;
			out.headB = f.b;
			out.headRadius = f.radius;
		}
	}
	// the section: the vertices from a fifth of the way to the head or the
	// tip, across the axis - their middle, their broad direction, and each
	// slice's half extents along it and across it
	const float sectionTo = out.head ? headFrom : length;
	std::vector<float> xs;
	std::vector<float> ys;
	std::vector<UInt32> ks;
	for (UInt32 i = 0; i < n; ++i) {
		const float t = DotOf3(pts[i], axis);
		if (t < length * kWeaponSectionFrom || t > sectionTo) {
			continue;
		}
		xs.push_back(DotOf3(pts[i], u1));
		ys.push_back(DotOf3(pts[i], u2));
		ks.push_back(sliceOf(t));
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
	float wide[kWeaponSlices] = {};
	float thick[kWeaponSlices] = {};
	bool seen[kWeaponSlices] = {};
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
	for (UInt32 k = 0; k < kWeaponSlices; ++k) {
		if (seen[k]) {
			widths.push_back(wide[k]);
			thicknesses.push_back(thick[k]);
		}
	}
	const NiPoint3 middle = u1 * cx + u2 * cy;
	out.section.valid = true;
	out.section.major = Unit(u1 * ca + u2 * sa, u1);
	out.section.minor = Unit(CrossOf3(axis, out.section.major), u2);
	out.section.halfWidth = Median(widths);
	out.section.halfThickness = Median(thicknesses);
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
