#pragma once

// The tab strip on the tablet's face (docs/mirror-spec.md 7): the tabs
// standing on the page's top edge, in the order of vr/TabletTabs.h, and what
// a press on the mirror's page reaches. Pure; covered by tablet_tabs_test.

#include "vr/TabletLogic.h"

namespace obvr::vr {

inline constexpr float kTabWidthShare = 0.28f;   // of the page's width
inline constexpr float kTabHeightShare = 0.09f;  // of the page's width
inline constexpr float kTabInsetShare = 0.05f;   // from the page's left edge, and between the tabs

// The tabs of `set` standing on the page's top edge at its left, first to
// last, each the same size; three fit inside the page's width. The face grows
// up by them - what is there already (the figure's page beside the page) is
// kept. Nothing without the page, or for fewer than two tabs.
inline void SetTabs(TabletFrame& f, const TabSet& set) {
	f.tabCount = 0;
	const float pageWidth = f.paper.right - f.paper.left;
	if (!f.up || !f.paper.valid || !(pageWidth > 0.0f) || set.count < 2 || set.count > kTabletTabsMost) {
		return;
	}
	const float tabWidth = kTabWidthShare * pageWidth;
	const float tabHeight = kTabHeightShare * pageWidth;
	const float inset = kTabInsetShare * pageWidth;
	f.tabCount = set.count;
	for (UInt32 i = 0; i < set.count; ++i) {
		f.tabKind[i] = set.kind[i];
		UiRect& tab = f.tabs[i];
		tab.left = f.paper.left + inset + static_cast<float>(i) * (tabWidth + inset);
		tab.right = tab.left + tabWidth;
		tab.bottom = f.paper.top;
		tab.top = f.paper.top - tabHeight;
		tab.valid = true;
		f.crop = UnionRect(f.crop.valid ? f.crop : f.paper, tab);
	}
}

// The strip a frame has.
inline TabSet TabsOf(const TabletFrame& f) {
	TabSet set;
	for (UInt32 i = 0; i < f.tabCount && i < kTabletTabsMost; ++i) {
		set.kind[set.count++] = f.tabKind[i];
	}
	return set;
}

// With the mirror chosen, a point on the face to the game's pixel under it:
// nothing on a tab or on the page's rectangle - the glass is OBVR's own and
// the game's page underneath gets no touch - the point itself off the page (a
// popup's place; a popup shows the page, not the mirror).
inline bool MirrorTabToLayer(const TabletFrame& f, float& x, float& y) {
	if (TabletTabAt(f, x, y) >= 0) {
		return false;
	}
	return !(x >= f.paper.left && x <= f.paper.right && y >= f.paper.top && y <= f.paper.bottom);
}

}  // namespace obvr::vr
