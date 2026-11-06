#pragma once

// The lock's minigame, traced ([Debug] LockpickTrace=<lines>): one line per
// interface frame while the LockPickMenu is loaded, with the cursor as the
// interface holds it, the engine's own cursor, the pixel the laser wants,
// and the pick's place - to read how the pick follows the cursor (a tester,
// 2026-11-06: "lock picking is very finicky when you increase the lock
// picking UI"; docs/controls-spec.md).
//
// From the game's own lockpick_menu.xml (Oblivion - Misc.bsa): the pick is
// the tile "lockpick_pick" under "lockpick_background" (853 units wide,
// centred on the screen); its x is its user2 - "set by code - position" -
// plus 153, and the tumblers' centres in user2's units are 313, 365, 416,
// 467 and 518. A UI mod's file may have other numbers.
//
// Pure apart from the step; covered by lockpick_trace_test.

#include "core/Types.h"

namespace obvr::game {

inline constexpr float kPickXOffset = 153.0f;
inline constexpr float kPickTumblerCentres[5] = {313.0f, 365.0f, 416.0f, 467.0f, 518.0f};

// The pick's place (the XML's user2) from its tile's x.
inline float PickPlaceFromTileX(float tileX) { return tileX - kPickXOffset; }

// The tumbler the pick stands under, 1 to 5: the nearest centre to its
// place. 0 for no number.
inline UInt32 PickTumblerFor(float place) {
	if (!(place == place)) {
		return 0;
	}
	UInt32 best = 1;
	float least = -1.0f;
	for (UInt32 i = 0; i < 5; ++i) {
		const float off = place > kPickTumblerCentres[i] ? place - kPickTumblerCentres[i] : kPickTumblerCentres[i] - place;
		if (least < 0.0f || off < least) {
			least = off;
			best = i + 1;
		}
	}
	return best;
}

// What the step carries from frame to frame; its caller owns it.
struct LockpickTraceState {
	UInt32 written = 0;
};

// Once per interface frame: a line while the LockPickMenu is loaded and
// fewer than `wantedLines` are written.
void StepLockpickTrace(LockpickTraceState& state, UInt32 wantedLines);

}  // namespace obvr::game
