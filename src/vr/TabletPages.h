#pragma once

// The tablet's pages by the right stick (Nadi, 2026-11-19: with the inventory
// open, the right stick left and right changes the tab): a flick to the side
// goes to the page beside the one shown, in the order of the page buttons at
// the page's foot - stats, inventory, magic, map - round from the last to the
// first. The page is opened by its own key (F1-F4, the keys the game binds to
// the four pages; F1 the stats page, read in tablet-open.txt). The page shown
// is the one the tablet reads (TabletFrame::page; the F1-F4 menus stand in the
// menu stack as one entry, so the stack cannot tell). Up and down
// still scroll the list. Pure; covered by tablet_pages_test.

#include "core/Types.h"
#include "game/MenuType.h"

namespace obvr::vr {

inline constexpr UInt32 kTabletPageCount = 4;
inline constexpr UInt32 kTabletPages[kTabletPageCount] = {game::kMenuIdStats, game::kMenuIdInventory,
                                                          game::kMenuIdMagic, game::kMenuIdMap};
inline constexpr UInt32 kTabletPageKeys[kTabletPageCount] = {0x70, 0x71, 0x72, 0x73};  // F1-F4

// The key of the page `step` places from `shown` (-1 left, +1 right), round;
// 0 for no step or no page shown.
inline UInt32 TabletPageKeyAfter(UInt32 shown, int step) {
	if (step == 0) {
		return 0;
	}
	for (UInt32 p = 0; p < kTabletPageCount; ++p) {
		if (kTabletPages[p] == shown) {
			const UInt32 next = (p + kTabletPageCount + (step > 0 ? 1u : kTabletPageCount - 1u)) % kTabletPageCount;
			return kTabletPageKeys[next];
		}
	}
	return 0;
}

// The flick: one step as the stick passes the dead zone to a side, the next
// only once it has come back under half of it.
struct TabletFlickState {
	bool armed = true;
};

inline int StepTabletFlick(TabletFlickState& s, bool allowed, float thumbX, float deadZone) {
	if (!allowed) {
		s.armed = true;
		return 0;
	}
	if (s.armed && (thumbX >= deadZone || thumbX <= -deadZone)) {
		s.armed = false;
		return thumbX > 0.0f ? 1 : -1;
	}
	if (thumbX < deadZone * 0.5f && thumbX > -deadZone * 0.5f) {
		s.armed = true;
	}
	return 0;
}

}  // namespace obvr::vr
