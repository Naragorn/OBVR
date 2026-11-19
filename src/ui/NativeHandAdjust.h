#pragma once

#include "core/Types.h"

namespace obvr::ui {

// The guided window for adjusting the hands (assets/menus/generic/
// OBVR_AdjustHands.xml): one window, two pages. The guide explains and
// offers Start or Cancel; the finish, opened once both hands have been
// fitted, offers Keep, Try again or Reset. The buttons report these IDs;
// the hidden 9500 is the closing ID OBVR clicks after acting on a choice.
constexpr int kAdjustHandsClose = 9500;
constexpr int kAdjustHandsPrimary = 9501;
constexpr int kAdjustHandsSecondary = 9502;
constexpr int kAdjustHandsReset = 9503;

enum class HandAdjustPage { Guide, Finish };

// The two pages' words (game/NativeMenuPrototype.cpp writes them into the
// window), their lines broken where they read well. The guide says the three
// steps one by one: a Quest 2 tester (2026-11-19) did not understand what it
// wanted ("isnt worded very well"), pressed every button and tried to move
// the hands "as you would a virtual object" - the old words ("Hold a grip and
// that hand stays put. Move the controller into it ...") never said which
// button, nor that the controller goes to the frozen hand and not the hand to
// the controller.
inline constexpr const char* kHandAdjustGuideMessage =
	"Let's line up your hands with your controllers.\n"
	"\n"
	"Squeeze and hold the grip button on one controller.\n"
	"That hand freezes where it is.\n"
	"Keep holding and move the controller into the hand,\n"
	"the way you would hold a sword. Then let go.\n"
	"\n"
	"Do the same with your other hand.\n"
	"Grabbing things is off until you're done.";
// The guide again after a fit that could not be a hand (game::FitReadsLikeAHand
// refused it - the controller ended far from the frozen hand): what went wrong
// and what to do instead. The hand kept the fit it had.
inline constexpr const char* kHandAdjustRefusedMessage =
	"That hand didn't line up: your controller\n"
	"ended up too far from it.\n"
	"\n"
	"Don't drag the hand along. While you hold the grip,\n"
	"the hand stays still - bring your controller to it,\n"
	"so it sits in your grip like a sword hilt. Then let go.\n"
	"\n"
	"Start to try again, or Cancel to stop.";
inline constexpr const char* kHandAdjustFinishMessage =
	"Both hands are lined up with your controllers.\n"
	"Keep them like this, try again,\n"
	"or reset both hands.";

// The text of the page shown.
inline const char* HandAdjustText(bool finish, bool refused) {
	return finish ? kHandAdjustFinishMessage : (refused ? kHandAdjustRefusedMessage : kHandAdjustGuideMessage);
}

enum class HandAdjustChoice {
	None,
	Start,   // guide: close the window, the adjusting begins
	Cancel,  // guide: close it - nothing changes, an adjusting under way (a refused fit's guide) ends
	Keep,    // finish: the fit stays, the adjusting ends
	Again,   // finish: close it and adjust again
	Reset,   // finish: both hands back to their defaults, the adjusting ends
};

inline HandAdjustChoice ChooseHandAdjust(HandAdjustPage page, int button) {
	if (page == HandAdjustPage::Guide) {
		if (button == kAdjustHandsPrimary) {
			return HandAdjustChoice::Start;
		}
		if (button == kAdjustHandsSecondary) {
			return HandAdjustChoice::Cancel;
		}
		return HandAdjustChoice::None;  // the reset button is switched off on the guide
	}
	if (button == kAdjustHandsPrimary) {
		return HandAdjustChoice::Keep;
	}
	if (button == kAdjustHandsSecondary) {
		return HandAdjustChoice::Again;
	}
	if (button == kAdjustHandsReset) {
		return HandAdjustChoice::Reset;
	}
	return HandAdjustChoice::None;
}

// The first fit, asked for by itself: in Full VR, once the player stands in
// the world, until a fit has been kept ([Hands] HandsAdjusted) - once per
// start, so "Later" puts it off to the next one rather than for good. Not
// while another of OBVR's windows or menus is on the way.
inline bool FirstHandFitDue(bool fullVR, bool handsAdjusted, bool inWorld, bool offeredThisStart,
                            bool busy) {
	return fullVR && !handsAdjusted && inWorld && !offeredThisStart && !busy;
}

// Whether a choice settles the fit for good: kept, or put back to the
// defaults on purpose. Cancel/Later and Again do not.
inline bool ChoiceSettlesFit(HandAdjustChoice choice) {
	return choice == HandAdjustChoice::Keep || choice == HandAdjustChoice::Reset;
}

// The session behind it, stepped every frame while the hands are being
// adjusted: a committed fit marks its hand done, and once both are done and
// no grip has been closed for kHandAdjustQuietSeconds, the finish page is
// asked for - once, until the session is restarted.
constexpr float kHandAdjustQuietSeconds = 1.5f;

struct HandAdjustSession {
	bool active = false;
	bool rightDone = false;
	bool leftDone = false;
	float quietSeconds = 0.0f;
	bool finishAsked = false;
};

inline void StartHandAdjustSession(HandAdjustSession& s) {
	s = HandAdjustSession{};
	s.active = true;
}

inline bool StepHandAdjustSession(HandAdjustSession& s, bool rightCommitted, bool leftCommitted,
                                  bool anyGripDown, float dtSeconds) {
	if (!s.active || s.finishAsked) {
		return false;
	}
	if (rightCommitted) {
		s.rightDone = true;
	}
	if (leftCommitted) {
		s.leftDone = true;
	}
	if (anyGripDown) {
		s.quietSeconds = 0.0f;
		return false;
	}
	if (!s.rightDone || !s.leftDone) {
		return false;
	}
	if (dtSeconds > 0.0f) {
		s.quietSeconds += dtSeconds;
	}
	if (s.quietSeconds >= kHandAdjustQuietSeconds) {
		s.finishAsked = true;
		return true;
	}
	return false;
}

}  // namespace obvr::ui
