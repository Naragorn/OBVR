#pragma once

// Capsules fitted to the meshes an actor wears (docs/mesh-fitted-capsules-spec.md
// 4.2; the prototype's numbers in 4.7, tools/capsule-fit.py the same fit
// offline): the vertices each group of bones owns - the bone with the
// largest weight - taken into the group's frame bone's own space through the
// skin's bind (NiSkinData's skin-to-bone, the inverse of the bone's bind in
// the skin's space, BoneMirror.h), and one capsule round them along the bone
// to its child, or along the vertices' principal axis for a bone without one
// (head, hand, foot). The radius is the smallest circle round the core of
// the cross-section (the vertices within the 95th percentile of their
// distance from the middle of its range), the line through that circle's
// centre; the ends are the vertices' extent trimmed by the radius. A flat
// torso (1.3 or flatter) gets two capsules side by side, B&S's collider
// group. The capsules stay in the frame bone's space and are placed by its
// world transform each frame (PlaceFittedCapsule).
//
// Pure, over plain values; the engine's reading is game/FittedCapsules.h.
// Covered by capsule_fit_test.

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/BodyViewLogic.h"
#include "game/BoneMirror.h"
#include "game/NiMath.h"

namespace obvr::game {

inline constexpr float kFitPercentile = 95.0f;
// The torso splits at this flatness (its reach along the section's major
// direction over its minor one): a naked man's chest 1.41, a naked woman's
// 1.23, every cuirass read 0.82-0.97 (spec 4.7).
inline constexpr float kFitSplitFlatness = 1.3f;
// A group owning fewer vertices than this gets no capsule: a clavicle's few
// shoulder vertices on a bare body, a stray weight.
inline constexpr UInt32 kFitMinVertices = 12;

inline float FitDot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline NiPoint3 FitCross(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline NiPoint3 FitUnit(const NiPoint3& a) {
	const float n = std::sqrt(a.LengthSquared());
	return n > 1e-9f ? a * (1.0f / n) : NiPoint3{1.0f, 0.0f, 0.0f};
}

// The value at `pct` percent, linear between the closest ranks (numpy's
// default, as the prototype). Reorders `values`: the two ranks are found by
// selection, not a sort.
inline float FitPercentile(std::vector<float>& values, float pct) {
	if (values.empty()) {
		return 0.0f;
	}
	const double k = static_cast<double>(values.size() - 1) * static_cast<double>(pct) / 100.0;
	const size_t lo = (std::min)(static_cast<size_t>(std::floor(k)), values.size() - 1);
	std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(lo), values.end());
	const float low = values[lo];
	// the next rank is the least of those the selection put after it
	const float high =
		lo + 1 < values.size() ? *std::min_element(values.begin() + static_cast<std::ptrdiff_t>(lo) + 1, values.end()) : low;
	return low + (high - low) * static_cast<float>(k - static_cast<double>(lo));
}

// ------------------------------------------------------------ the circles

struct FitCircle {
	float x = 0.0f;
	float y = 0.0f;
	float r = 0.0f;
};

inline FitCircle FitCircleTwo(float ax, float ay, float bx, float by) {
	FitCircle c;
	c.x = (ax + bx) * 0.5f;
	c.y = (ay + by) * 0.5f;
	c.r = std::hypot(ax - c.x, ay - c.y);
	return c;
}

inline FitCircle FitCircleThree(float ax, float ay, float bx, float by, float cx, float cy) {
	const double d = 2.0 * (static_cast<double>(ax) * (by - cy) + static_cast<double>(bx) * (cy - ay) +
	                        static_cast<double>(cx) * (ay - by));
	if (std::fabs(d) < 1e-9) {
		// in a line: the circle over the two farthest apart
		FitCircle best = FitCircleTwo(ax, ay, bx, by);
		const FitCircle ac = FitCircleTwo(ax, ay, cx, cy);
		const FitCircle bc = FitCircleTwo(bx, by, cx, cy);
		best = ac.r > best.r ? ac : best;
		return bc.r > best.r ? bc : best;
	}
	const double a2 = static_cast<double>(ax) * ax + static_cast<double>(ay) * ay;
	const double b2 = static_cast<double>(bx) * bx + static_cast<double>(by) * by;
	const double c2 = static_cast<double>(cx) * cx + static_cast<double>(cy) * cy;
	FitCircle c;
	c.x = static_cast<float>((a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d);
	c.y = static_cast<float>((a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d);
	c.r = std::hypot(ax - c.x, ay - c.y);
	return c;
}

inline bool FitInside(const FitCircle& c, float x, float y) {
	return std::hypot(x - c.x, y - c.y) <= c.r * (1.0f + 1e-5f) + 1e-5f;
}

// The smallest circle round 2D points (Welzl, iterative). The points are
// shuffled by a fixed generator of its own, so a fit repeats the same on
// every compiler (std::shuffle is the library's own).
inline FitCircle FitEnclosingCircle(std::vector<float>& xs, std::vector<float>& ys) {
	const size_t n = xs.size();
	UInt32 seed = 0x9E3779B9u;
	for (size_t i = n; i > 1; --i) {
		seed = seed * 1664525u + 1013904223u;
		const size_t j = static_cast<size_t>(seed % static_cast<UInt32>(i));
		std::swap(xs[i - 1], xs[j]);
		std::swap(ys[i - 1], ys[j]);
	}
	FitCircle c;
	bool have = false;
	for (size_t i = 0; i < n; ++i) {
		if (have && FitInside(c, xs[i], ys[i])) {
			continue;
		}
		c = FitCircle{xs[i], ys[i], 0.0f};
		have = true;
		for (size_t j = 0; j < i; ++j) {
			if (FitInside(c, xs[j], ys[j])) {
				continue;
			}
			c = FitCircleTwo(xs[i], ys[i], xs[j], ys[j]);
			for (size_t k = 0; k < j; ++k) {
				if (!FitInside(c, xs[k], ys[k])) {
					c = FitCircleThree(xs[i], ys[i], xs[j], ys[j], xs[k], ys[k]);
				}
			}
		}
	}
	return c;
}

// The circle of a cross-section: the vertices nearer than the percentile to
// the middle of their range (each axis' central `pct` share) form its core,
// and the circle is the smallest round the core. The percentile keeps a
// stray vertex out; the smallest circle centres a lopsided section (a half
// torso) on its shape, not on where its vertices crowd.
inline FitCircle FitSectionCircle(const std::vector<float>& us, const std::vector<float>& vs, float pct) {
	const float lo = (100.0f - pct) * 0.5f;
	const float hi = 100.0f - lo;
	std::vector<float> sorted = us;
	const float mu = (FitPercentile(sorted, lo) + FitPercentile(sorted, hi)) * 0.5f;
	sorted = vs;
	const float mv = (FitPercentile(sorted, lo) + FitPercentile(sorted, hi)) * 0.5f;
	std::vector<float> d(us.size());
	for (size_t i = 0; i < us.size(); ++i) {
		d[i] = std::hypot(us[i] - mu, vs[i] - mv);
	}
	std::vector<float> dSorted = d;
	const float cut = FitPercentile(dSorted, pct);
	std::vector<float> cx;
	std::vector<float> cy;
	cx.reserve(us.size());
	cy.reserve(us.size());
	for (size_t i = 0; i < us.size(); ++i) {
		if (d[i] <= cut * (1.0f + 1e-6f) + 1e-6f) {
			cx.push_back(us[i]);
			cy.push_back(vs[i]);
		}
	}
	return FitEnclosingCircle(cx, cy);
}

// ------------------------------------------------------------ one capsule

// The principal axis of points round their centroid: the eigenvector of the
// largest eigenvalue of their covariance (Jacobi's rotations).
inline NiPoint3 FitPrincipalAxis(const NiPoint3* pts, UInt32 n, const NiPoint3& centre) {
	double a[3][3] = {};
	for (UInt32 i = 0; i < n; ++i) {
		const double d[3] = {pts[i].x - centre.x, pts[i].y - centre.y, pts[i].z - centre.z};
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				a[r][c] += d[r] * d[c];
			}
		}
	}
	double v[3][3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
	for (int sweep = 0; sweep < 50; ++sweep) {
		const double off = std::fabs(a[0][1]) + std::fabs(a[0][2]) + std::fabs(a[1][2]);
		if (off < 1e-12) {
			break;
		}
		static const int kPairs[3][2] = {{0, 1}, {0, 2}, {1, 2}};
		for (const auto& pq : kPairs) {
			const int p = pq[0];
			const int q = pq[1];
			if (std::fabs(a[p][q]) < 1e-15) {
				continue;
			}
			const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
			const double t = (theta >= 0.0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
			const double c = 1.0 / std::sqrt(t * t + 1.0);
			const double s = t * c;
			for (int k = 0; k < 3; ++k) {
				const double akp = a[k][p];
				const double akq = a[k][q];
				a[k][p] = c * akp - s * akq;
				a[k][q] = s * akp + c * akq;
			}
			for (int k = 0; k < 3; ++k) {
				const double apk = a[p][k];
				const double aqk = a[q][k];
				a[p][k] = c * apk - s * aqk;
				a[q][k] = s * apk + c * aqk;
			}
			for (int k = 0; k < 3; ++k) {
				const double vkp = v[k][p];
				const double vkq = v[k][q];
				v[k][p] = c * vkp - s * vkq;
				v[k][q] = s * vkp + c * vkq;
			}
		}
	}
	int best = 0;
	for (int i = 1; i < 3; ++i) {
		if (a[i][i] > a[best][best]) {
			best = i;
		}
	}
	return FitUnit(NiPoint3{static_cast<float>(v[0][best]), static_cast<float>(v[1][best]), static_cast<float>(v[2][best])});
}

struct FitShape {
	NiPoint3 a{0.0f, 0.0f, 0.0f};  // the segment's ends
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
	// The section's reach along its major direction over its minor one: 1
	// round, 2 twice as wide as deep.
	float flatness = 1.0f;
	NiPoint3 axis{1.0f, 0.0f, 0.0f};
	NiPoint3 major{0.0f, 1.0f, 0.0f};
	NiPoint3 line{0.0f, 0.0f, 0.0f};  // a point of the capsule's line
	UInt32 count = 0;
};

// One capsule round `n` points: along `origin` to `towards` when both are
// given (a bone to its child), else along the points' principal axis. False
// without points.
inline bool FitCapsuleToPoints(const NiPoint3* pts, UInt32 n, const NiPoint3* origin, const NiPoint3* towards,
                               FitShape& out, float pct = kFitPercentile) {
	out = FitShape{};
	if (pts == nullptr || n == 0) {
		return false;
	}
	NiPoint3 centroid{0.0f, 0.0f, 0.0f};
	for (UInt32 i = 0; i < n; ++i) {
		centroid = centroid + pts[i];
	}
	centroid = centroid * (1.0f / static_cast<float>(n));
	NiPoint3 axis;
	NiPoint3 base;
	if (origin != nullptr && towards != nullptr && (*towards - *origin).LengthSquared() > 1e-6f) {
		axis = FitUnit(*towards - *origin);
		base = *origin;
	} else {
		axis = FitPrincipalAxis(pts, n, centroid);
		base = centroid;
	}
	const NiPoint3 helper = std::fabs(axis.z) < 0.9f ? NiPoint3{0.0f, 0.0f, 1.0f} : NiPoint3{1.0f, 0.0f, 0.0f};
	const NiPoint3 u = FitUnit(FitCross(axis, helper));
	const NiPoint3 v = FitCross(axis, u);
	std::vector<float> ts(n);
	std::vector<float> us(n);
	std::vector<float> vs(n);
	for (UInt32 i = 0; i < n; ++i) {
		const NiPoint3 d = pts[i] - base;
		ts[i] = FitDot(d, axis);
		us[i] = FitDot(d, u);
		vs[i] = FitDot(d, v);
	}
	const FitCircle circle = FitSectionCircle(us, vs, pct);
	const float r = circle.r;
	// the section's major direction, from the vertices within the radius: a
	// 2x2 eigen problem
	double suu = 0.0;
	double svv = 0.0;
	double suv = 0.0;
	UInt32 kept = 0;
	for (UInt32 i = 0; i < n; ++i) {
		const double du = us[i] - circle.x;
		const double dv = vs[i] - circle.y;
		if (std::sqrt(du * du + dv * dv) <= r * (1.0 + 1e-5) + 1e-5) {
			suu += du * du;
			svv += dv * dv;
			suv += du * dv;
			++kept;
		}
	}
	double mu = 1.0;
	double mv = 0.0;
	if (kept > 0) {
		const double tr = suu + svv;
		const double det = suu * svv - suv * suv;
		const double disc = std::sqrt((std::max)(tr * tr / 4.0 - det, 0.0));
		const double l1 = tr / 2.0 + disc;
		if (std::fabs(suv) > 1e-12) {
			mu = l1 - svv;
			mv = suv;
		} else if (svv > suu) {
			mu = 0.0;
			mv = 1.0;
		}
	}
	const double m = std::hypot(mu, mv);
	const NiPoint3 major = FitUnit(u * static_cast<float>(mu / m) + v * static_cast<float>(mv / m));
	const NiPoint3 minor = FitCross(axis, major);
	const NiPoint3 line = base + u * circle.x + v * circle.y;
	std::vector<float> reachMajor(n);
	std::vector<float> reachMinor(n);
	for (UInt32 i = 0; i < n; ++i) {
		const NiPoint3 d = pts[i] - line;
		reachMajor[i] = std::fabs(FitDot(d, major));
		reachMinor[i] = std::fabs(FitDot(d, minor));
	}
	const float rMajor = FitPercentile(reachMajor, pct);
	const float rMinor = FitPercentile(reachMinor, pct);
	float tMin = ts[0];
	float tMax = ts[0];
	for (UInt32 i = 1; i < n; ++i) {
		tMin = (std::min)(tMin, ts[i]);
		tMax = (std::max)(tMax, ts[i]);
	}
	float t0 = tMin + r;
	float t1 = tMax - r;
	if (t0 > t1) {
		t0 = t1 = (tMin + tMax) * 0.5f;
	}
	out.a = line + axis * t0;
	out.b = line + axis * t1;
	out.radius = r;
	out.flatness = rMajor / (std::max)(rMinor, 1e-6f);
	out.axis = axis;
	out.major = major;
	out.line = line;
	out.count = n;
	return true;
}

// Two capsules side by side along the section's major direction, each fitted
// to its half (spec 4.2 step 4); a vertex on the dividing line belongs to
// both. False when a half is empty.
inline bool SplitFittedCapsule(const NiPoint3* pts, UInt32 n, const FitShape& fit, FitShape out[2],
                               float pct = kFitPercentile) {
	std::vector<NiPoint3> halves[2];
	for (UInt32 i = 0; i < n; ++i) {
		const float side = FitDot(pts[i] - fit.line, fit.major);
		if (side <= 1e-5f) {
			halves[0].push_back(pts[i]);
		}
		if (side >= -1e-5f) {
			halves[1].push_back(pts[i]);
		}
	}
	const NiPoint3 towards = fit.line + fit.axis;
	for (int h = 0; h < 2; ++h) {
		if (halves[h].empty() || !FitCapsuleToPoints(halves[h].data(), static_cast<UInt32>(halves[h].size()), &fit.line,
		                                             &towards, out[h], pct)) {
			return false;
		}
	}
	return true;
}

// A group that narrows along its length - a plate skirt over a knee, a
// chest over a waist - got one capsule as round as its widest part to both
// its ends: the soldier's thigh capsule stood 7.0 units off his knee at its
// end, the beggar's torso 2.4-5.8 off his shoulders at its end, all the
// large gaps at a capsule's end (mesh-gap-*.txt, 2026-10-27 15:42-15:45,
// "at 1.00 along it"). Split at the middle of its extent into two along the
// same line, each fitted to its half, when one half's radius is this many
// times the other's; a group no longer than twice its radius stays whole.
inline constexpr float kFitLengthSplitRatio = 1.25f;

inline bool SplitAlongLength(const NiPoint3* pts, UInt32 n, const FitShape& fit, FitShape out[2],
                             float pct = kFitPercentile) {
	if (pts == nullptr || n == 0) {
		return false;
	}
	float lo = 0.0f;
	float hi = 0.0f;
	for (UInt32 i = 0; i < n; ++i) {
		const float t = FitDot(pts[i] - fit.line, fit.axis);
		lo = i == 0 || t < lo ? t : lo;
		hi = i == 0 || t > hi ? t : hi;
	}
	if (!(hi - lo > 2.0f * fit.radius)) {
		return false;
	}
	const float mid = 0.5f * (lo + hi);
	std::vector<NiPoint3> halves[2];
	for (UInt32 i = 0; i < n; ++i) {
		const float t = FitDot(pts[i] - fit.line, fit.axis);
		if (t <= mid) {
			halves[0].push_back(pts[i]);
		}
		if (t >= mid) {
			halves[1].push_back(pts[i]);
		}
	}
	const NiPoint3 towards = fit.line + fit.axis;
	for (int h = 0; h < 2; ++h) {
		if (halves[h].size() < kFitMinVertices ||
		    !FitCapsuleToPoints(halves[h].data(), static_cast<UInt32>(halves[h].size()), &fit.line, &towards, out[h],
		                        pct)) {
			return false;
		}
	}
	// (not "small": a Windows header's macro)
	const float wide = out[0].radius > out[1].radius ? out[0].radius : out[1].radius;
	const float narrow = out[0].radius > out[1].radius ? out[1].radius : out[0].radius;
	return narrow > 0.0f && wide >= narrow * kFitLengthSplitRatio;
}

// ------------------------------------------------------------ the groups

enum FitGroup : UInt8 {
	kFitHead,
	kFitNeck,
	kFitTorso,
	kFitPelvis,
	kFitLClavicle,
	kFitLUpperArm,
	kFitLForearm,
	kFitLHand,
	kFitLThigh,
	kFitLCalf,
	kFitLFoot,
	kFitRClavicle,
	kFitRUpperArm,
	kFitRForearm,
	kFitRHand,
	kFitRThigh,
	kFitRCalf,
	kFitRFoot,
	kFitGroupCount,
	kFitNoGroup = 0xFF
};

// A group's frame bone (whose space the capsule is kept in; the second name
// when the first is in no skin - the meshes skin to Neck1, not Neck), the
// bone its axis runs to (none: the principal axis), its body part, and
// whether it splits when flat.
struct FitGroupInfo {
	const char* frame;
	const char* frameAlt;
	const char* towards;
	const char* towardsAlt;
	BodyPart part;
	bool splitWhenFlat;
	const char* name;
};

inline const FitGroupInfo& FitGroupInfoOf(UInt8 group) {
	static const FitGroupInfo kInfo[kFitGroupCount + 1] = {
		{"Bip01 Head", nullptr, nullptr, nullptr, BodyPart::Head, false, "head"},
		{"Bip01 Neck1", "Bip01 Neck", "Bip01 Head", nullptr, BodyPart::Neck, false, "neck"},
		{"Bip01 Spine", "Bip01 Spine1", "Bip01 Neck1", "Bip01 Neck", BodyPart::Torso, true, "torso"},
		{"Bip01 Pelvis", nullptr, "Bip01 Spine", nullptr, BodyPart::Torso, false, "pelvis"},
		{"Bip01 L Clavicle", nullptr, "Bip01 L UpperArm", nullptr, BodyPart::Torso, false, "left shoulder"},
		{"Bip01 L UpperArm", "Bip01 L UpperArmTwist", "Bip01 L Forearm", nullptr, BodyPart::LeftArm, false,
		 "left upper arm"},
		{"Bip01 L Forearm", "Bip01 L ForearmTwist", "Bip01 L Hand", nullptr, BodyPart::LeftArm, false, "left forearm"},
		{"Bip01 L Hand", nullptr, nullptr, nullptr, BodyPart::LeftArm, false, "left hand"},
		{"Bip01 L Thigh", nullptr, "Bip01 L Calf", nullptr, BodyPart::LeftLeg, false, "left thigh"},
		{"Bip01 L Calf", nullptr, "Bip01 L Foot", nullptr, BodyPart::LeftLeg, false, "left calf"},
		{"Bip01 L Foot", nullptr, nullptr, nullptr, BodyPart::LeftLeg, false, "left foot"},
		{"Bip01 R Clavicle", nullptr, "Bip01 R UpperArm", nullptr, BodyPart::Torso, false, "right shoulder"},
		{"Bip01 R UpperArm", "Bip01 R UpperArmTwist", "Bip01 R Forearm", nullptr, BodyPart::RightArm, false,
		 "right upper arm"},
		{"Bip01 R Forearm", "Bip01 R ForearmTwist", "Bip01 R Hand", nullptr, BodyPart::RightArm, false, "right forearm"},
		{"Bip01 R Hand", nullptr, nullptr, nullptr, BodyPart::RightArm, false, "right hand"},
		{"Bip01 R Thigh", nullptr, "Bip01 R Calf", nullptr, BodyPart::RightLeg, false, "right thigh"},
		{"Bip01 R Calf", nullptr, "Bip01 R Foot", nullptr, BodyPart::RightLeg, false, "right calf"},
		{"Bip01 R Foot", nullptr, nullptr, nullptr, BodyPart::RightLeg, false, "right foot"},
		{nullptr, nullptr, nullptr, nullptr, BodyPart::Column, false, "none"},
	};
	return kInfo[group < kFitGroupCount ? group : kFitGroupCount];
}

inline bool FitSameName(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return false;
	}
	for (; *a != '\0' && *b != '\0'; ++a, ++b) {
		char x = *a;
		char y = *b;
		x = (x >= 'A' && x <= 'Z') ? static_cast<char>(x - 'A' + 'a') : x;
		y = (y >= 'A' && y <= 'Z') ? static_cast<char>(y - 'A' + 'a') : y;
		if (x != y) {
			return false;
		}
	}
	return *a == *b;
}

inline bool FitStartsWith(const char* text, const char* prefix) {
	for (; *prefix != '\0'; ++text, ++prefix) {
		char x = *text;
		char y = *prefix;
		x = (x >= 'A' && x <= 'Z') ? static_cast<char>(x - 'A' + 'a') : x;
		y = (y >= 'A' && y <= 'Z') ? static_cast<char>(y - 'A' + 'a') : y;
		if (x != y) {
			return false;
		}
	}
	return true;
}

// The group a bone's vertices go to: a twist bone with its limb, the fingers
// with the hand, the toes with the foot, both neck bones together, the three
// spine bones as one torso (spec 4.2 step 1). kFitNoGroup for any other
// bone (a helper, a creature's chin).
inline UInt8 FitGroupOfBone(const char* name) {
	if (name == nullptr || !FitStartsWith(name, "Bip01 ")) {
		return kFitNoGroup;
	}
	const char* rest = name + 6;
	if (FitSameName(rest, "Head")) {
		return kFitHead;
	}
	if (FitSameName(rest, "Neck") || FitSameName(rest, "Neck1")) {
		return kFitNeck;
	}
	if (FitSameName(rest, "Spine") || FitSameName(rest, "Spine1") || FitSameName(rest, "Spine2")) {
		return kFitTorso;
	}
	if (FitSameName(rest, "Pelvis")) {
		return kFitPelvis;
	}
	const bool left = FitStartsWith(rest, "L ");
	const bool right = FitStartsWith(rest, "R ");
	if (!left && !right) {
		return kFitNoGroup;
	}
	const char* limb = rest + 2;
	const UInt8 side = left ? 0 : static_cast<UInt8>(kFitRClavicle - kFitLClavicle);
	UInt8 g = kFitNoGroup;
	if (FitSameName(limb, "Clavicle")) {
		g = kFitLClavicle;
	} else if (FitSameName(limb, "UpperArm") || FitSameName(limb, "UpperArmTwist")) {
		g = kFitLUpperArm;
	} else if (FitSameName(limb, "Forearm") || FitSameName(limb, "ForearmTwist")) {
		g = kFitLForearm;
	} else if (FitSameName(limb, "Hand") || FitStartsWith(limb, "Finger")) {
		g = kFitLHand;
	} else if (FitSameName(limb, "Thigh")) {
		g = kFitLThigh;
	} else if (FitSameName(limb, "Calf")) {
		g = kFitLCalf;
	} else if (FitSameName(limb, "Foot") || FitStartsWith(limb, "Toe")) {
		g = kFitLFoot;
	}
	return g == kFitNoGroup ? g : static_cast<UInt8>(g + side);
}

// ------------------------------------------------------------ the body

// A bone of the skins read: its name and the transform from the space the
// vertices are given in into the bone's own. Offline (tools/capsule-fit.py)
// that space is the skeleton's bind; in the game the world at the fit's
// moment, the transform the inverse of the bone's world transform
// (game/FittedCapsules.cpp) - the shapes' own bind spaces differ.
struct FitBone {
	const char* name;
	NiTransform skinToBone;
};

// A skinned vertex in the vertices' space (FitBone), with the bone that owns
// it.
struct FitVertex {
	NiPoint3 bind;
	UInt16 bone;
};

struct FittedCapsule {
	UInt8 group = kFitNoGroup;
	UInt16 frameBone = 0;  // index into the bones the fit was given
	NiPoint3 a{0.0f, 0.0f, 0.0f};  // in the frame bone's own space
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
	float flatness = 1.0f;
	UInt32 vertices = 0;
	BodyPart part = BodyPart::Column;
};

// Every group once, twice when it splits (the torso side by side, a
// narrowing group along its length).
inline constexpr UInt32 kFitCapsulesMax = 2 * kFitGroupCount;

inline int FitFindBone(const FitBone* bones, UInt32 count, const char* name) {
	if (name == nullptr) {
		return -1;
	}
	for (UInt32 i = 0; i < count; ++i) {
		if (FitSameName(bones[i].name, name)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

// The capsules of a body from its skins' vertices: per group, in its frame
// bone's space (spec 4.2). Answers how many were written to `out`.
inline UInt32 FitBodyCapsules(const FitBone* bones, UInt32 boneCount, const FitVertex* vertices, UInt32 vertexCount,
                              FittedCapsule* out, UInt32 most, float pct = kFitPercentile,
                              float splitFlatness = kFitSplitFlatness) {
	if (bones == nullptr || vertices == nullptr || out == nullptr || most == 0) {
		return 0;
	}
	std::vector<UInt8> groupOf(boneCount);
	for (UInt32 i = 0; i < boneCount; ++i) {
		groupOf[i] = FitGroupOfBone(bones[i].name);
	}
	UInt32 written = 0;
	std::vector<NiPoint3> pts;
	for (UInt8 g = 0; g < kFitGroupCount && written < most; ++g) {
		const FitGroupInfo& info = FitGroupInfoOf(g);
		int frame = FitFindBone(bones, boneCount, info.frame);
		if (frame < 0) {
			frame = FitFindBone(bones, boneCount, info.frameAlt);
		}
		for (UInt32 i = 0; frame < 0 && i < boneCount; ++i) {
			if (groupOf[i] == g) {
				frame = static_cast<int>(i);
			}
		}
		if (frame < 0) {
			continue;
		}
		const NiTransform& toFrame = bones[frame].skinToBone;
		pts.clear();
		for (UInt32 i = 0; i < vertexCount; ++i) {
			if (vertices[i].bone < boneCount && groupOf[vertices[i].bone] == g) {
				pts.push_back(TransformPoint(toFrame, vertices[i].bind));
			}
		}
		if (pts.size() < kFitMinVertices) {
			continue;
		}
		int towardsBone = FitFindBone(bones, boneCount, info.towards);
		if (towardsBone < 0) {
			towardsBone = FitFindBone(bones, boneCount, info.towardsAlt);
		}
		const NiPoint3 origin{0.0f, 0.0f, 0.0f};
		NiPoint3 towards{0.0f, 0.0f, 0.0f};
		const bool alongBone = towardsBone >= 0 && towardsBone != frame;
		if (alongBone) {
			towards = TransformPoint(toFrame, InverseTransform(bones[towardsBone].skinToBone).pos);
		}
		FitShape fit;
		if (!FitCapsuleToPoints(pts.data(), static_cast<UInt32>(pts.size()), alongBone ? &origin : nullptr,
		                        alongBone ? &towards : nullptr, fit, pct)) {
			continue;
		}
		FitShape two[2];
		// flat (the torso): side by side; else narrowing: along its length
		const bool split = written + 2 <= most &&
		                   ((info.splitWhenFlat && fit.flatness >= splitFlatness &&
		                     SplitFittedCapsule(pts.data(), static_cast<UInt32>(pts.size()), fit, two, pct)) ||
		                    SplitAlongLength(pts.data(), static_cast<UInt32>(pts.size()), fit, two, pct));
		const UInt32 pieces = split ? 2 : 1;
		for (UInt32 p = 0; p < pieces; ++p) {
			const FitShape& s = split ? two[p] : fit;
			FittedCapsule& c = out[written++];
			c.group = g;
			c.frameBone = static_cast<UInt16>(frame);
			c.a = s.a;
			c.b = s.b;
			c.radius = s.radius;
			c.flatness = fit.flatness;
			c.vertices = static_cast<UInt32>(pts.size());
			c.part = info.part;
		}
	}
	return written;
}

// A fitted capsule in the world: its ends through its frame bone's world
// transform, its radius times the bone's scale.
inline void PlaceFittedCapsule(const NiTransform& frameWorld, const FittedCapsule& c, NiPoint3& a, NiPoint3& b,
                               float& radius) {
	a = TransformPoint(frameWorld, c.a);
	b = TransformPoint(frameWorld, c.b);
	const float s = frameWorld.scale > 0.0f && frameWorld.scale < 20.0f ? frameWorld.scale : 1.0f;
	radius = c.radius * s;
}

// A print of the shapes an actor wears (each shape's words: its data's
// address, its vertex count, ...), FNV-1a; never 0, so 0 can mean "not
// read".
inline UInt32 OutfitPrint(const UInt32* words, UInt32 count) {
	UInt32 h = 2166136261u;
	for (UInt32 i = 0; i < count; ++i) {
		for (int k = 0; k < 4; ++k) {
			h ^= (words[i] >> (8 * k)) & 0xFFu;
			h *= 16777619u;
		}
	}
	h ^= count;
	h *= 16777619u;
	return h != 0 ? h : 1u;
}

}  // namespace obvr::game
