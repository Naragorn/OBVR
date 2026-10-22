#pragma once

#include "core/Types.h"
#include "ui/NativeControlsCard.h"
#include "ui/NativeHandAdjust.h"

namespace obvr::ui {

// The first start in Full VR as a chain of native pages (docs/onboarding-spec.md
// 3.2 and 5; the tester, 2026-10-15: "wir brauchen die erststart kette, bei
// jedem start whl und pflicht beim new game dungeon"): Measure, Which hand,
// the controls card, then the comfort page - at every start while
// [Onboarding] StartGuide is on, in the main menu, each page skippable, Esc
// ends the chain. Measure and Which hand borrow the adjust-hands window
// (OBVR_AdjustHands.xml: a title, a text, up to three buttons), Controls is
// the controls card's own window, Comfort the settings' comfort view. Pure;
// the host (game/NativeMenuPrototype.cpp) opens the windows, saves the hand
// and steps the comfort page. Covered by native_settings_test.
// Measure (since 2026-10-22, vr/BodyMeasure.h): the T-pose that recenters -
// the way the head looks becomes forward - scales the weapons' places to the
// eye height and sets the arms' stretch from the span. It took the place of
// the recenter page, which asked for the stick chord alone (Nadi,
// 2026-10-22: "sobald wir t pose haben können wir natürlich dann den
// recenter prompt im onboarding entfernen ... die t pose macht das ja dann
// auch").
enum class FirstStartPage { Measure, WhichHand, Controls, Comfort, Done };

inline FirstStartPage FirstStartNext(FirstStartPage page) {
	switch (page) {
	case FirstStartPage::Measure:
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

// The words of the pages on the adjust-hands window. The measure page says
// seated or standing, and what was measured (`measured`, the frame's words,
// null before) - its primary button reads Next either way, the secondary
// switches between seated and standing.
inline FirstStartText FirstStartTextFor(FirstStartPage page, const char* measured = nullptr, bool seated = false) {
	FirstStartText t;
	switch (page) {
	case FirstStartPage::Measure:
		t.title = measured != nullptr ? "Measured" : (seated ? "Measure your arms" : "Measure your body");
		if (measured != nullptr) {
			t.message = measured;
		} else if (seated) {
			t.message =
				"Seated: look straight ahead, hold both arms out to the sides, level with your shoulders, and "
				"pull both triggers. That way becomes forward and the body's arms follow your span; the "
				"weapons' places stay as they are.";
		} else {
			t.message =
				"Stand straight, look ahead, hold both arms out to the sides like a T and pull both triggers. "
				"That way becomes forward, your weapons' places follow your height - replacing places fitted "
				"before - and the body's arms your span.";
		}
		t.primary = "Next";
		t.secondary = seated ? "Standing" : "I'm seated";
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

enum class FirstStartChoice { None, Next, Right, Left, Turn, Seated };

// The buttons: the adjust-hands window's on Measure and Which hand, the
// controls card's on Controls. Skip is Next without a change; the card's
// Close is Next too, its Next and Back turn its page.
inline FirstStartChoice ChooseFirstStart(FirstStartPage page, int button) {
	switch (page) {
	case FirstStartPage::Measure:
		if (button == kAdjustHandsPrimary) {
			return FirstStartChoice::Next;
		}
		if (button == kAdjustHandsSecondary) {
			return FirstStartChoice::Seated;  // seated and standing, in turn
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
	case FirstStartPage::Measure:
	case FirstStartPage::WhichHand:
		return "Data\\Menus\\Generic\\OBVR_AdjustHands.xml";
	case FirstStartPage::Controls:
		return "Data\\Menus\\Generic\\OBVR_Controls.xml";
	default:
		return nullptr;
	}
}

}  // namespace obvr::ui
