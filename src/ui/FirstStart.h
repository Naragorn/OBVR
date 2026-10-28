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
// auch"). A measurement goes on to Which hand by itself; only a refusal
// stays, with its reason (Nadi, 2026-10-22: "die nach dem t pose, diese
// seite kann weg").
// Seated (2026-10-22, Nadi: "wenn man i'm seated drückt dann lass machen dass
// hands off ist also full vr aus und wir zum seated + gamepad + keyboard mode
// zurückkehren ... mit back button falls user sich nicht entscheiden
// können"): reached only by the measure page's "I'm seated"; it says what
// seated play is, Keep seated turns Full VR off and ends the guide, Back
// returns to the measure page with Full VR as it was.
// RunBy (2026-10-28, Nadi: running by swung arms "default off und es kommt
// ins onboarding user können dort entscheiden"): after Which hand, Swing arms
// or Left stick, saved as [Hands] ArmSwingRun; Skip keeps what is set.
enum class FirstStartPage { Measure, Seated, WhichHand, RunBy, Controls, Comfort, Done };

inline FirstStartPage FirstStartNext(FirstStartPage page) {
	switch (page) {
	case FirstStartPage::Measure:
		return FirstStartPage::WhichHand;
	case FirstStartPage::Seated:
		return FirstStartPage::Done;  // kept seated: Full VR is off, the guide ends
	case FirstStartPage::WhichHand:
		return FirstStartPage::RunBy;
	case FirstStartPage::RunBy:
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
	// The T-pose's drawing beside a narrower text (Nadi, 2026-10-22: "beim t
	// pose im onboarding füge noch ein bild hinzu welches dieses
	// anschaulicher macht"): the measure page's alone.
	bool picture = false;
};

// The words of the pages on the adjust-hands window. The measure page says
// why a pose was refused (`measured`, the frame's words, null before).
inline FirstStartText FirstStartTextFor(FirstStartPage page, const char* measured = nullptr) {
	FirstStartText t;
	switch (page) {
	case FirstStartPage::Measure:
		t.title = measured != nullptr ? "Measure again" : "Measure your body";
		t.message = measured != nullptr
		                ? measured
		                : "Stand straight, look ahead, hold both arms out to the sides like a T and pull both "
		                  "triggers. That way becomes forward, your weapons' places follow your height - replacing "
		                  "places fitted before - and the body's arms your span. Playing seated? I'm seated.";
		t.primary = "Next";
		t.secondary = "I'm seated";
		t.picture = true;
		break;
	case FirstStartPage::Seated:
		t.title = "Seated with a gamepad";
		t.message =
			"Full VR is for standing with motion controllers. Seated, you play with a gamepad or mouse and "
			"keyboard and look around with the headset. Keep seated turns Full VR off and ends this guide; "
			"Back returns to standing. The settings (Insert) switch it any time.";
		t.primary = "Keep seated";
		t.secondary = "Back";
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
	case FirstStartPage::RunBy:
		t.title = "How do you run?";
		t.message =
			"The left stick walks. Swing arms: swing both arms forward and back as when running, and you "
			"run. Left stick: push the stick in to run. The settings row Run by swinging arms changes it "
			"later.";
		t.primary = "Swing arms";
		t.secondary = "Left stick";
		t.reset = "Skip";
		t.resetShown = true;
		break;
	default:
		break;
	}
	return t;
}

enum class FirstStartChoice { None, Next, Right, Left, Turn, Seated, KeepSeated, Back, RunBySwing, RunByStick };

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
			return FirstStartChoice::Seated;  // to the seated page, Full VR untouched yet
		}
		return FirstStartChoice::None;
	case FirstStartPage::Seated:
		if (button == kAdjustHandsPrimary) {
			return FirstStartChoice::KeepSeated;
		}
		if (button == kAdjustHandsSecondary) {
			return FirstStartChoice::Back;
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
	case FirstStartPage::RunBy:
		if (button == kAdjustHandsPrimary) {
			return FirstStartChoice::RunBySwing;
		}
		if (button == kAdjustHandsSecondary) {
			return FirstStartChoice::RunByStick;
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

// What the frame's measurement does on the measure page: measured, the chain
// goes on as Next does; refused, nothing - the page shows the reason.
inline FirstStartChoice ChooseFirstStartMeasured(bool measured) {
	return measured ? FirstStartChoice::Next : FirstStartChoice::None;
}

// The XML a page's window comes from (under the game's folder), null for a
// page without a window of its own.
inline const char* FirstStartWindow(FirstStartPage page) {
	switch (page) {
	case FirstStartPage::Measure:
	case FirstStartPage::Seated:
	case FirstStartPage::WhichHand:
	case FirstStartPage::RunBy:
		return "Data\\Menus\\Generic\\OBVR_AdjustHands.xml";
	case FirstStartPage::Controls:
		return "Data\\Menus\\Generic\\OBVR_Controls.xml";
	default:
		return nullptr;
	}
}

}  // namespace obvr::ui
