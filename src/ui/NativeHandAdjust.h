#pragma once

#include "core/Types.h"

namespace obvr::ui {

// The guided window for adjusting the hands (assets/menus/generic/
// OBVR_AdjustHands.xml): one window, two pages. The guide explains and
// offers Start or Cancel; the finish, opened once both hands have been
// fitted, offers Keep, Adjust again or Reset. The buttons report these IDs;
// the hidden 9500 is the closing ID OBVR clicks after acting on a choice.
constexpr int kAdjustHandsClose = 9500;
constexpr int kAdjustHandsPrimary = 9501;
constexpr int kAdjustHandsSecondary = 9502;
constexpr int kAdjustHandsReset = 9503;

enum class HandAdjustPage { Guide, Finish };

enum class HandAdjustChoice {
	None,
	Start,   // guide: close the window, the adjusting begins
	Cancel,  // guide: close it, nothing changes
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
