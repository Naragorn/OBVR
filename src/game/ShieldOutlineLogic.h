#pragma once

// The shield's outline in its own plane (Nadi, 2026-11-27: the arrow rule
// fits each shield's real shape, read from its mesh, not a circle and
// guessed margins).
//
// Collected in the disc read's second walk over the model's vertices
// (game/ShieldShape.cpp, the reach along the axes - no third walk): each
// vertex's reach along the disc's first axis (the larger scatter, PCA) and
// its second goes into one of kOutlineBins fine bins over +-kOutlineReachUnits
// from the walk's centre (the vertices' mean); each bin keeps the least and
// the most reach along the second axis. When the walk is done the bins are
// laid onto kOutlineRows rows over the disc's own extent along the first
// axis, relative to the disc's middle (ShieldDisc::centre), rows without a
// vertex filled between their nearest filled neighbours, none past the
// outermost. Under kOutlineMinRows filled rows the outline is not taken and
// the circle stands in.
//
// The rows lie along the first axis whatever its length: the lookup asks
// both in-plane coordinates, so which spoke is the longer (half[0] or
// half[1]) does not matter to it - only the rows' step does, and the first
// axis has the larger spread, nearly always the longer reach.
//
// Read with the disc (every kShieldDiscReadFrames frames, or when the node or
// its bound changes), never per frame. Pure; shield_outline_test.

#include <cmath>

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

inline constexpr UInt32 kOutlineRows = 24;
inline constexpr UInt32 kOutlineMinRows = 6;  // a quarter of them
inline constexpr UInt32 kOutlineBins = 96;
inline constexpr float kOutlineReachUnits = 120.0f;  // twice the largest spoke a shield may have

// The outline as read: row i covers [start + i * step, start + (i + 1) * step)
// along the disc's first axis, its span [low, high] along the second, both
// from the disc's middle. 24 rows: 24 * 8 bytes of spans, 24 flags, the
// header - about 230 bytes in ShieldDisc.
struct ShieldOutline {
	bool valid = false;
	float start = 0.0f;
	float step = 0.0f;
	float low[kOutlineRows] = {};
	float high[kOutlineRows] = {};
	bool filled[kOutlineRows] = {};
	UInt32 filledRead = 0;  // rows with a vertex of their own, before the filling
	float widest = 0.0f;    // the largest span, for the read's line
	float narrowest = 0.0f; // the smallest filled span
};

// The bins the walk fills (on the stack of the read, about 1.2 KB).
struct OutlineBins {
	float low[kOutlineBins] = {};
	float high[kOutlineBins] = {};
	UInt32 count[kOutlineBins] = {};

	// `a`, `b`: the vertex's reach along the first and the second axis from
	// the walk's centre.
	void Add(float a, float b) {
		const float step = 2.0f * kOutlineReachUnits / static_cast<float>(kOutlineBins);
		const float f = (a + kOutlineReachUnits) / step;
		if (!(f >= 0.0f) || !(f < static_cast<float>(kOutlineBins))) {
			return;
		}
		const UInt32 i = static_cast<UInt32>(f);
		if (count[i] == 0 || b < low[i]) {
			low[i] = b;
		}
		if (count[i] == 0 || b > high[i]) {
			high[i] = b;
		}
		++count[i];
	}
};

// The rows from the bins: the disc's extent along the first axis is
// [lowA, highA] from the walk's centre, its middle along the second axis
// `middleB` from it (the disc's centre moves there, ShieldDiscFrom).
inline ShieldOutline OutlineFromBins(const OutlineBins& bins, float lowA, float highA, float middleB) {
	ShieldOutline o;
	if (!(highA > lowA)) {
		return o;
	}
	const float middleA = (highA + lowA) * 0.5f;
	o.step = (highA - lowA) / static_cast<float>(kOutlineRows);
	o.start = lowA - middleA;
	const float binStep = 2.0f * kOutlineReachUnits / static_cast<float>(kOutlineBins);
	for (UInt32 r = 0; r < kOutlineRows; ++r) {
		const float from = lowA + static_cast<float>(r) * o.step;
		const float to = from + o.step;
		// The bins whose middle falls in the row, or the bin under the row's
		// middle when the row is narrower than a bin.
		const float mid = (from + to) * 0.5f;
		for (UInt32 i = 0; i < kOutlineBins; ++i) {
			const float binFrom = -kOutlineReachUnits + static_cast<float>(i) * binStep;
			const float binMid = binFrom + binStep * 0.5f;
			const bool inRow = (binMid >= from && binMid < to) || (mid >= binFrom && mid < binFrom + binStep);
			if (!inRow || bins.count[i] == 0) {
				continue;
			}
			const float lo = bins.low[i] - middleB;
			const float hi = bins.high[i] - middleB;
			if (!o.filled[r] || lo < o.low[r]) {
				o.low[r] = lo;
			}
			if (!o.filled[r] || hi > o.high[r]) {
				o.high[r] = hi;
			}
			o.filled[r] = true;
		}
		o.filledRead += o.filled[r] ? 1u : 0u;
	}
	if (o.filledRead < kOutlineMinRows) {
		return ShieldOutline{};
	}
	// The holes between filled rows, from their nearest filled neighbours.
	for (UInt32 r = 0; r < kOutlineRows; ++r) {
		if (o.filled[r]) {
			continue;
		}
		int before = static_cast<int>(r) - 1;
		while (before >= 0 && !o.filled[before]) {
			--before;
		}
		UInt32 after = r + 1;
		while (after < kOutlineRows && !o.filled[after]) {
			++after;
		}
		if (before < 0 || after >= kOutlineRows) {
			continue;
		}
		const float t = static_cast<float>(static_cast<int>(r) - before) / static_cast<float>(static_cast<int>(after) - before);
		o.low[r] = o.low[before] + (o.low[after] - o.low[before]) * t;
		o.high[r] = o.high[before] + (o.high[after] - o.high[before]) * t;
		o.filled[r] = true;
	}
	bool first = true;
	for (UInt32 r = 0; r < kOutlineRows; ++r) {
		if (!o.filled[r]) {
			continue;
		}
		const float w = o.high[r] - o.low[r];
		o.widest = first || w > o.widest ? w : o.widest;
		o.narrowest = first || w < o.narrowest ? w : o.narrowest;
		first = false;
	}
	o.valid = true;
	return o;
}

// How far the point (a, b: along the first and the second axis from the
// disc's middle) lies outside the outline, in its plane: negative inside (by
// how much from the nearest side of its row), positive outside (to the
// nearest filled row's span). Large positive for an outline not valid.
inline float OutlineDistance(const ShieldOutline& o, float a, float b) {
	if (!o.valid || !(o.step > 0.0f)) {
		return 1e9f;
	}
	float best = 1e9f;
	for (UInt32 r = 0; r < kOutlineRows; ++r) {
		if (!o.filled[r]) {
			continue;
		}
		const float from = o.start + static_cast<float>(r) * o.step;
		const float to = from + o.step;
		const float gap = a < from ? from - a : (a > to ? a - to : 0.0f);
		const float side = b < o.low[r] ? o.low[r] - b : (b > o.high[r] ? b - o.high[r] : 0.0f);
		float d;
		if (gap == 0.0f && side == 0.0f) {
			const float toLow = b - o.low[r];
			const float toHigh = o.high[r] - b;
			d = -(toLow < toHigh ? toLow : toHigh);
		} else {
			d = std::sqrt(gap * gap + side * side);
		}
		best = d < best ? d : best;
	}
	return best;
}

}  // namespace obvr::game
