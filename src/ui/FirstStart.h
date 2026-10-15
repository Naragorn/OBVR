#pragma once

#include "core/Types.h"
#include "ui/NativeControlsCard.h"
#include "ui/NativeHandAdjust.h"

namespace obvr::ui {

// The first start in Full VR as a chain of native pages (docs/onboarding-spec.md
// 3.2 and 5; the tester, 2026-10-15: "wir brauchen die erststart kette, bei
// jedem start whl und pflicht beim new game dungeon"): Recenter, Which hand,
// the controls card, then the comfort page - at every start while
// [Onboarding] StartGuide is on, in the main menu, each page skippable, Esc
// ends the chain. Recenter and Which hand borrow the adjust-hands window
// (OBVR_AdjustHands.xml: a title, a text, up to three buttons), Controls is
// the controls card's own window, Comfort the settings' comfort view. Pure;
// the host (game/NativeMenuPrototype.cpp) opens the windows, saves the hand
// and steps the comfort page. Covered by native_settings_test.
enum class FirstStartPage { Recenter, WhichHand, Controls, Comfort, Done };

inline FirstStartPage FirstStartNext(FirstStartPage page) {
	switch (page) {
	case FirstStartPage::Recenter:
		return FirstStartPage::WhichHand;
	case FirstStartPage::WhichHand:
		return FirstStartPage::Controls;
	case FirstStartPage::Controls:
		return FirstStartPage::Comfort;
	default:
		return FirstStartPage::Done;
	}
}

// When the chain begins: Full VR on, the guide on, not yet this start, its
// place in front (the main menu; the world for a hand script's run), no
// generic menu up and nothing else of OBVR's on the way.
inline bool FirstStartDue(bool fullVR, bool guideOn, bool placeInFront, bool genericRoot, bool busy,
                          bool ranThisStart) {
	return fullVR && guideOn && placeInFront && !genericRoot && !busy && !ranThisStart;
}

struct FirstStartText {
	const char* title = "";
	const char* message = "";
	const char* primary = "";
	const char* secondary = "";
	const char* reset = " ";
	bool resetShown = false;
};

// The words of the two pages on the adjust-hands window. The recenter page
// changes once the recenter has fired.
inline FirstStartText FirstStartTextFor(FirstStartPage page, bool recentred) {
	FirstStartText t;
	switch (page) {
	case FirstStartPage::Recenter:
		if (recentred) {
			t.title = "Recentred";
			t.message =
				"Your view is set. Both stick buttons held in for three seconds recenter it again any "
				"time, and the settings menu has a row for it (Recenter view).";
		} else {
			t.title = "Recenter your view";
			t.message =
				"Stand or sit the way you will play and look straight ahead. Then hold both stick "
				"buttons in for three seconds, or press Delete: that way becomes forward and level. "
				"Do it again any time. Esc ends this guide; the settings menu's Help brings it back.";
		}
		t.primary = "Next";
		t.secondary = "Skip";
		break;
	case FirstStartPage::WhichHand:
		t.title = "Which hand holds the weapon?";
		t.message =
			"Right or left. Left swaps the controllers' roles: the weapon, the swing and the laser "
			"on the left hand, the menus on the right wrist. The settings row Left-handed changes "
			"it later.";
		t.primary = "Right";
		t.secondary = "Left";
		t.reset = "Skip";
		t.resetShown = true;
		break;
	default:
		break;
	}
	return t;
}

enum class FirstStartChoice { None, Next, Right, Left, Turn };

// The buttons: the adjust-hands window's on Recenter and Which hand, the
// controls card's on Controls. Skip is Next without a change; the card's
// Close is Next too, its Next and Back turn its page.
inline FirstStartChoice ChooseFirstStart(FirstStartPage page, int button) {
	switch (page) {
	case FirstStartPage::Recenter:
		if (button == kAdjustHandsPrimary || button == kAdjustHandsSecondary) {
			return FirstStartChoice::Next;
		}
		return FirstStartChoice::None;
	case FirstStartPage::WhichHand:
		if (button == kAdjustHandsPrimary) {
			return FirstStartChoice::Right;
		}
		if (button == kAdjustHandsSecondary) {
			return FirstStartChoice::Left;
		}
		if (button == kAdjustHandsReset) {
			return FirstStartChoice::Next;
		}
		return FirstStartChoice::None;
	case FirstStartPage::Controls:
		if (button == kControlsCardPrimary) {
			return FirstStartChoice::Turn;
		}
		if (button == kControlsCardSecondary) {
			return FirstStartChoice::Next;
		}
		return FirstStartChoice::None;
	default:
		return FirstStartChoice::None;
	}
}

// The XML a page's window comes from (under the game's folder), null for a
// page without a window of its own.
inline const char* FirstStartWindow(FirstStartPage page) {
	switch (page) {
	case FirstStartPage::Recenter:
	case FirstStartPage::WhichHand:
		return "Data\\Menus\\Generic\\OBVR_AdjustHands.xml";
	case FirstStartPage::Controls:
		return "Data\\Menus\\Generic\\OBVR_Controls.xml";
	default:
		return nullptr;
	}
}

}  // namespace obvr::ui
