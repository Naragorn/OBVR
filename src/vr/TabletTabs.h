#pragma once

// The tablet's tabs (docs/mirror-spec.md 7 and 8, stage 1; docs/inventory-
// tablet-spec.md 9): which tabs stand over the page's top edge and which one
// a click chose. A strip of OBVR's own, drawn into the eyes with the tablet's
// pieces, so it exists in every [Tablet] Figure mode:
//   page, [figure], mirror
// The page's tab is always first, the figure's is there with Figure=tab and a
// figure to show, the mirror's is always last ([Tablet] Mirror).
//
// The tab shown is OBVR's own choice: the game's F1-F4 page underneath stays
// what it was (the game's menus see nothing of a tab), and a press on a tab
// or on the mirror's page reaches nothing of the game's (vr/TabletTabStrip.h).
// Pure; covered by tablet_tabs_test.

#include "core/Types.h"

namespace obvr::vr {

enum class TabletTab : UInt8 { Page = 0, Figure = 1, Mirror = 2 };
inline constexpr UInt32 kTabletTabsMost = 3;

// The tabs of a strip, left to right. Fewer than two is no strip.
struct TabSet {
	UInt32 count = 0;
	TabletTab kind[kTabletTabsMost] = {};

	bool Has(TabletTab k) const {
		for (UInt32 i = 0; i < count && i < kTabletTabsMost; ++i) {
			if (kind[i] == k) {
				return true;
			}
		}
		return false;
	}
};

// What decides the strip. `figureTab`: [Tablet] Figure=tab and the figure was
// fitted behind a tab (vr::SetFigureTab); `mirror`: [Tablet] Mirror.
struct TabsWanted {
	bool up = false;       // the tablet's menus are up ([Tablet] Enabled=0 never gets here)
	bool inScene = false;  // drawn into the eyes: the strip is made of the scene's pieces
	bool figureTab = false;
	bool mirror = false;
};

inline TabSet TabsFor(const TabsWanted& w) {
	TabSet set;
	if (!w.up || !w.inScene) {
		return set;
	}
	set.kind[set.count++] = TabletTab::Page;
	if (w.figureTab) {
		set.kind[set.count++] = TabletTab::Figure;
	}
	if (w.mirror) {
		set.kind[set.count++] = TabletTab::Mirror;
	}
	if (set.count < 2) {
		return TabSet{};
	}
	return set;
}

// The tab chosen: the click's rising edge on a tab picks it. A popup over the
// page shows the page whatever was chosen (the game draws it there), and the
// choice waits. A tab the strip no longer has - the figure's on a page with no
// figure, the mirror's switched off, the tablet closed - is forgotten.
struct TabletTabState {
	TabletTab chosen = TabletTab::Page;
	bool wasDown = false;
};

// `under` is the tab the finger or the beam is on (a TabletTab's value, -1
// none). Returns the tab to show this frame.
inline TabletTab StepTabletTab(TabletTabState& s, const TabSet& set, int under, bool clickDown, bool popupUp) {
	const bool edge = clickDown && !s.wasDown;
	s.wasDown = clickDown;
	if (set.count == 0) {
		s.chosen = TabletTab::Page;
		return TabletTab::Page;
	}
	if (edge && under >= 0 && under < static_cast<int>(kTabletTabsMost) && set.Has(static_cast<TabletTab>(under))) {
		s.chosen = static_cast<TabletTab>(under);
	}
	if (!set.Has(s.chosen)) {
		s.chosen = TabletTab::Page;
	}
	return popupUp ? TabletTab::Page : s.chosen;
}

// The tab's name for the log: "the page's tab chosen", as the harness scenarios expect.
inline const char* TabletTabName(TabletTab tab) {
	return tab == TabletTab::Figure ? "figure's" : tab == TabletTab::Mirror ? "mirror's" : "page's";
}

// A page turn (the stick's flick or the finger's swipe, vr/TabletPages.h)
// leaves the mirror, and the turn goes on to the game as it always did. The
// figure's tab is left chosen, as it was. Returns `step`.
inline int TurnTabletPage(TabletTabState& s, int step) {
	if (step != 0 && s.chosen == TabletTab::Mirror) {
		s.chosen = TabletTab::Page;
	}
	return step;
}

}  // namespace obvr::vr
