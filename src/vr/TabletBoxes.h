#pragma once

// The boxes on the tablet's page (docs/inventory-tablet-spec.md section 23;
// Nadi, 2026-11-25: "warum kommt die box seitlich ans tablet. wenn wir das
// tablet zweigeteilt per option haben (figur auf einem tab und tablet auf
// anderer) dann sollten alle msg boxen auch mittig vom tablet kommen statt
// seitlich. das löst auch das linkshänder problem.", and then "die
// infoboxen nicht mittig, die können seitlich bleiben").
//
// The tablet shows each box where the game draws it on a flat screen. A
// message, a quantity box, a Generic box and a book stand in the screen's
// middle, which lies right of the page: they hung beside the tablet - and,
// held by a physical right hand at the page's right edge, over the forearm
// (section 21). With nothing of the tablet's own beside the page (the
// figure behind a tab, or off), such a box is moved by the game onto the
// page's middle (ShiftOntoPage; game/TabletMenu.cpp shifts its menu's root
// tile): the game draws it there and finds its buttons there, so the beam
// and the finger work it where it is seen. The infos shown while a row is
// pointed at - the item info, the stats page's own box - stay beside the
// page, where the game slides them out.
// Pure; covered by tablet_boxes_test.

#include "vr/TabletLogic.h"

namespace obvr::vr {

// Whether the boxes come onto the page: drawn into the eyes - the overlay
// shows one cut of the layer as the game lays it out - with nothing of the
// tablet's own beside the page: the figure behind a tab, or off. With the
// figure on its own page beside the page a box stands where that page would
// (vr::SetFigurePage), as the game has it.
inline bool BoxesOnPage(bool inScene, TabletFigure figure) {
	return inScene && figure != TabletFigure::Beside;
}

// A box the game raises over a page and that waits for an answer or is read:
// a message, a quantity, a Generic box (the tutorial's help, a quest's,
// OBVR's own native menus) and a book. The item info (MagicPopup) is no box
// but an info, and stays beside the page.
inline bool TabletBoxMoves(UInt32 menuId) {
	switch (menuId) {
	case game::kMenuIdMessage:
	case game::kMenuIdQuantity:
	case game::kMenuIdGeneric:
	case game::kMenuIdBook:
		return true;
	default:
		return false;
	}
}

// How far a box is shifted, in HUD units, to stand on the page's middle: its
// pictures' rectangle centred on the page's, as far as the screen lets it
// (KeptOnScreen). None within half a unit - it stands there, and a rounding
// is no reason to write a frame - nor without a box or a page.
inline constexpr float kBoxShiftLeast = 0.5f;

struct BoxShift {
	bool move = false;
	float dx = 0.0f;
	float dy = 0.0f;
};

// The shift along one axis that centres [lo, hi] on `mid`, stopped where the
// box would leave the screen [0, extent]: what lies past it the game draws
// nowhere, and the tablet shows the layer as drawn (Nadi, Windows
// 2026-11-26: the tutorial's Generic box, 888 units wide, centred on the
// page's middle 399.5 units from the screen's left lost its left 45 units;
// read cut at the edge - vr::OnScreen - it was shifted on to 89 units past
// it; docs/inventory-tablet-spec.md 24.3). Wider than the screen, the near
// edge wins: the box starts at 0, as a page does.
inline float KeptOnScreen(float lo, float hi, float mid, float extent) {
	float d = mid - 0.5f * (lo + hi);
	if (hi + d > extent) {
		d = extent - hi;
	}
	if (lo + d < 0.0f) {
		d = -lo;
	}
	return d;
}

inline BoxShift ShiftOntoPage(const UiRect& box, const UiRect& paper, float screenWidth, float screenHeight) {
	BoxShift s;
	if (!box.valid || !paper.valid) {
		return s;
	}
	const float dx = KeptOnScreen(box.left, box.right, 0.5f * (paper.left + paper.right), screenWidth);
	const float dy = KeptOnScreen(box.top, box.bottom, 0.5f * (paper.top + paper.bottom), screenHeight);
	s.move = dx > kBoxShiftLeast || dx < -kBoxShiftLeast || dy > kBoxShiftLeast || dy < -kBoxShiftLeast;
	if (s.move) {
		s.dx = dx;
		s.dy = dy;
	}
	return s;
}

// A moved box's place as the game had it, to put back when the box is no
// longer on the tablet's page: the value before the first shift; while ours
// stands, still that one; when the game has set its own meanwhile (a formula
// read anew when what it reads changed), the game's.
inline float KeptOriginal(bool known, float original, float written, float current) {
	return known && current == written ? original : current;
}

// Put back off the page only while the value written is still there: one the
// game set since is the game's.
inline bool PutBack(float current, float written) { return current == written; }

// Whether a box that waits for an answer is over the page - an info is not:
// the pages are not turned under a box (vr/TabletPages.h), and the tab shows
// the page while one is up (vr::StepTabletTab). An info is shown while a row
// is pointed at; counted as a box it stopped the right stick turning the
// page whenever the beam rested on a row (the probe of 2026-11-25,
// 20261125-150011-slot3: the stick's flick from the stats page did nothing
// with an attribute's box up). More popups than kept: counted as a box.
inline bool BoxOverPage(const TabletRects& r) {
	if (r.popupsOverflow) {
		return true;
	}
	for (UInt32 i = 0; i < r.popupCount && i < kTabletPopupsMost; ++i) {
		if (!r.popupInfo[i]) {
			return true;
		}
	}
	return false;
}

}  // namespace obvr::vr
