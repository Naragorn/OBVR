#pragma once

// The holding hand's finger skin, measured (docs/inventory-tablet-spec.md
// 32.2, Nadi's run of 2026-11-28 14:56: "clippen finger noch leicht durch va
// zeigefinger"). The limit (vr/HeldFingers.h) keeps a finger's joints and
// tip behind the page by a skin round them that was a guess, 1 cm; the drawn
// finger is its mesh, whose flesh, nail and pad may reach further. The
// hand's vertices are read for its reach anyway (vr/HandReach.h,
// camera/hands/HandReachRead.cpp): each is given to the nearest bone - one of
// the four fingers' links (its tip past the last joint as the tap's), the
// palm's (the wrist to each knuckle) or the thumb's - and a finger's skin is
// how far its vertices reach from its bones; the widest reading of a tree is
// kept, and the limit holds each finger by its own. The same vertices say
// how deep each finger's skin stands against the page - the instrument the
// run did not have. Pure; covered by finger_skin_test.

#include "vr/HeldFingers.h"

namespace obvr::vr {

// A finger's skin is held to these bounds: thinner is a mesh read wrongly
// (no finger is a centimetre across), wider is some other part's vertex.
// [G] from the human hand, 1 to 2.5 cm thick fingers, a gauntlet's more.
inline constexpr float kFingerSkinLeastMetres = 0.005f;
inline constexpr float kFingerSkinMostMetres = 0.025f;

// The hand's bones where the vertices were read, in the world: the wrist,
// each finger's three joints and its tip (vr::IndexTipBeyond), the thumb's.
struct HandBonesNow {
	bool valid = false;
	NiPoint3 wrist{0.0f, 0.0f, 0.0f};
	bool fingerValid[4] = {false, false, false, false};
	NiPoint3 finger[4][4] = {};
	bool thumbValid = false;
	NiPoint3 thumb[4] = {};
};

struct FingerSkinRead {
	bool valid = false;
	bool measured[4] = {false, false, false, false};  // a vertex was this finger's
	float radius[4] = {0.0f, 0.0f, 0.0f, 0.0f};        // metres: how far its vertices reach from its bones
	UInt32 counted[4] = {0, 0, 0, 0};
	bool over[4] = {false, false, false, false};       // a vertex of it over the page's face
	float deepest[4] = {-1.0f, -1.0f, -1.0f, -1.0f};   // metres before the face (above 0: through it)
};

inline float FingerSegmentDistanceSquared(const NiPoint3& p, const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 ab = b - a;
	const float len = game::DotOf3(ab, ab);
	float t = len > 0.0f ? game::DotOf3(p - a, ab) / len : 0.0f;
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	const NiPoint3 d = p - (a + ab * t);
	return game::DotOf3(d, d);
}

// The bone a vertex hangs nearest: 0-3 a finger, -1 the palm or the thumb;
// `distance2` its squared distance from that finger's bones.
inline int NearestFinger(const NiPoint3& p, const HandBonesNow& b, float& distance2) {
	int best = -1;
	float bestD = 1.0e30f;
	for (int f = 0; f < 4; ++f) {
		if (!b.fingerValid[f]) {
			continue;
		}
		const float palm = FingerSegmentDistanceSquared(p, b.wrist, b.finger[f][0]);
		bestD = palm < bestD ? (best = -1, palm) : bestD;
		for (int j = 0; j < 3; ++j) {
			const float d = FingerSegmentDistanceSquared(p, b.finger[f][j], b.finger[f][j + 1]);
			if (d < bestD) {
				bestD = d;
				best = f;
			}
		}
	}
	for (int j = 0; b.thumbValid && j < 4; ++j) {
		const float d = FingerSegmentDistanceSquared(p, j == 0 ? b.wrist : b.thumb[j - 1], b.thumb[j]);
		bestD = d < bestD ? (best = -1, d) : bestD;
	}
	distance2 = bestD;
	return best;
}

// One reading: `vertices` the holding hand's as drawn, `sheet` the page this
// frame, both in the world's units (`unitsPerMetre`).
inline FingerSkinRead ReadFingerSkin(const NiPoint3* vertices, UInt32 count, const HandBonesNow& b,
                                     const SheetInWorld& sheet, float unitsPerMetre) {
	FingerSkinRead r;
	if (vertices == nullptr || count == 0 || !b.valid || !(unitsPerMetre > 0.0f)) {
		return r;
	}
	r.valid = true;
	const float perMetre = 1.0f / unitsPerMetre;
	for (UInt32 i = 0; i < count; ++i) {
		float d2 = 0.0f;
		const int f = NearestFinger(vertices[i], b, d2);
		if (f < 0) {
			continue;
		}
		const float radius = math::Sqrt(d2) * perMetre;
		r.radius[f] = !r.measured[f] || radius > r.radius[f] ? radius : r.radius[f];
		r.measured[f] = true;
		++r.counted[f];
		const float depth = DepthBeforeSheet(sheet, &vertices[i], 1, 0.0f);
		if (depth > -1.0e29f) {
			const float metres = depth * perMetre;
			r.deepest[f] = !r.over[f] || metres > r.deepest[f] ? metres : r.deepest[f];
			r.over[f] = true;
		}
	}
	return r;
}

// Each finger's skin as the limit takes it: the widest reading of the tree
// within the bounds, and the page's clearance (kPageClearMetres) on top; the
// guess where nothing was measured.
struct FingerSkinKept {
	bool measured[4] = {false, false, false, false};
	float radius[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

inline void KeepFingerSkin(FingerSkinKept& k, const FingerSkinRead& r) {
	for (int f = 0; r.valid && f < 4; ++f) {
		if (!r.measured[f]) {
			continue;
		}
		float radius = r.radius[f] < kFingerSkinLeastMetres ? kFingerSkinLeastMetres : r.radius[f];
		radius = radius > kFingerSkinMostMetres ? kFingerSkinMostMetres : radius;
		k.radius[f] = !k.measured[f] || radius > k.radius[f] ? radius : k.radius[f];
		k.measured[f] = true;
	}
}

inline float SkinOf(const FingerSkinKept& k, int finger) {
	return k.measured[finger] ? k.radius[finger] + kPageClearMetres : kFingerSkinMetres;
}

// The log's line of the skin against the page: the first reading of each
// hold, then as the set of fingers through the page changes, and at a mark -
// kSkinLinesPerHold a hold at most.
inline constexpr UInt32 kSkinLinesPerHold = 6;

struct SkinLineState {
	bool said = false;
	UInt32 lines = 0;
	UInt8 throughMask = 0;
};

inline bool SkinLineDue(SkinLineState& s, bool holding, UInt8 throughMask, bool mark) {
	if (!holding) {
		s = SkinLineState{};
		return false;
	}
	const bool due = !s.said || mark || throughMask != s.throughMask;
	s.throughMask = throughMask;
	if (!due || s.lines >= kSkinLinesPerHold) {
		return false;
	}
	s.said = true;
	++s.lines;
	return true;
}

}  // namespace obvr::vr
