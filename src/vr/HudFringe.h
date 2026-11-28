#pragma once

// The fringe left round a lifted HUD element: how far the pixels the game
// drew still reach past the rectangle the HUD on the hands erased from the
// main panel (docs/hud-on-hands-spec.md section 13). Read from the panel at a
// hand script's mark only (render/HandHudMarks.cpp reads it back), never per
// frame. Pure; covered by hand_hud_test.
//
// Each side of the rectangle is measured in rings: ring k is the pixels k
// steps outside it (the larger of the two steps, so a corner pixel counts
// for the side it is farther out on). A side's depth is the number of rings
// next to the edge that each hold at least one drawn pixel - a letter's edge
// cut in two reaches the first ring and the next, a neighbour further off
// behind a clear ring does not count.

#include "core/Types.h"
#include "vr/HandHud.h"

namespace obvr::vr {

inline constexpr UInt32 kHudFringeBandMax = 32;

enum HudFringeSide : UInt32 { kFringeLeft = 0, kFringeTop = 1, kFringeRight = 2, kFringeBottom = 3 };
inline constexpr const char* kHudFringeSideNames[4] = {"left", "top", "right", "bottom"};

struct HudFringe {
	CaptureRect rect;
	UInt32 band = 0;
	bool ring[4][kHudFringeBandMax + 1] = {};
	UInt32 pixels[4] = {};  // drawn pixels within the band on each side
	UInt32 inside = 0;      // drawn pixels inside the rectangle (0 once erased)
};

// A probe round `rect` reaching `band` pixels out (at most kHudFringeBandMax).
inline HudFringe StartHudFringe(const CaptureRect& rect, UInt32 band) {
	HudFringe f;
	f.rect = rect;
	f.band = band > kHudFringeBandMax ? kHudFringeBandMax : band;
	return f;
}

// One row of the panel (A8R8G8B8; drawn is any alpha) folded into the probe.
inline void AccumulateHudFringeRow(HudFringe& f, const UInt32* row, UInt32 width, UInt32 y) {
	const CaptureRect& r = f.rect;
	const SInt32 sy = static_cast<SInt32>(y);
	const SInt32 band = static_cast<SInt32>(f.band);
	if (!r.valid || row == nullptr || sy < r.top - band || sy >= r.bottom + band) {
		return;
	}
	const SInt32 dy = sy < r.top ? r.top - sy : (sy >= r.bottom ? sy - r.bottom + 1 : 0);
	const SInt32 from = r.left - band < 0 ? 0 : r.left - band;
	SInt32 to = r.right + band;
	to = to > static_cast<SInt32>(width) ? static_cast<SInt32>(width) : to;
	for (SInt32 x = from; x < to; ++x) {
		if ((row[x] >> 24) == 0) {
			continue;
		}
		const SInt32 dx = x < r.left ? r.left - x : (x >= r.right ? x - r.right + 1 : 0);
		if (dx == 0 && dy == 0) {
			++f.inside;
			continue;
		}
		const UInt32 side = dx >= dy ? (x < r.left ? kFringeLeft : kFringeRight) : (sy < r.top ? kFringeTop : kFringeBottom);
		const SInt32 ringIndex = dx >= dy ? dx : dy;
		f.ring[side][ringIndex] = true;
		++f.pixels[side];
	}
}

// How many rings next to the side's edge are each drawn on: 0 when the cut
// took everything there.
inline UInt32 HudFringeDepth(const HudFringe& f, UInt32 side) {
	UInt32 depth = 0;
	while (side < 4 && depth < f.band && f.ring[side][depth + 1]) {
		++depth;
	}
	return depth;
}

}  // namespace obvr::vr
