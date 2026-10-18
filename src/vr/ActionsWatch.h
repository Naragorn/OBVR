#pragma once

// SteamVR's action input going dead on a hand: every action of the set
// inactive - the hand's "bound mask 00" - while the controller stays
// tracked. A tester's log of 2026-10-17 (Quest 3 over Virtual Desktop,
// xOBSE 22) read it four times, twice in the main menu and once in the game,
// each time back to 6F later; the same tester's "sprint and jump stop
// working, a restart helps" is unexplained and this is the candidate: while
// the mask stays 00 the sticks and buttons do nothing, and the one line the
// log had for it was written on the change alone, so how long it lasted and
// whether it was the moment his sprint went is not in the log. The watch
// counts the seconds a hand's actions have been inactive and says when to
// log: once at a second, then every ten, and once when they come back after
// at least that second - a blip of a frame or two says nothing.
//
// Pure, covered by controller_actions_test.

#include "core/Types.h"

namespace obvr::vr {

inline constexpr float kActionsReportFirstSeconds = 1.0f;
inline constexpr float kActionsReportEverySeconds = 10.0f;

struct ActionsInactiveWatch {
	bool inactive = false;    // the actions were dead on the last frame
	float seconds = 0.0f;     // for how long
	float nextReport = 0.0f;  // the seconds at which the next line is due
};

struct ActionsInactiveVerdict {
	bool reportInactive = false;  // say they have been inactive for `seconds`
	bool reportBack = false;      // say they are back after `seconds`
	float seconds = 0.0f;
};

// One frame: whether the hand is tracked, whether its controls come from
// the action set at all (legacy input has no mask), the mask of actions
// active this frame, and the frame's seconds.
inline ActionsInactiveVerdict StepActionsInactive(ActionsInactiveWatch& w, bool tracked, bool actionInput,
                                                  UInt32 activeMask, float dt) {
	ActionsInactiveVerdict v;
	// Untracked, or controls that never came from the action set: the mask
	// means nothing, and a controller that dropped out of tracking is not
	// "back" - cleared without a word.
	if (!tracked || !actionInput) {
		w = ActionsInactiveWatch{};
		return v;
	}
	if (activeMask != 0) {
		if (w.inactive && w.seconds >= kActionsReportFirstSeconds) {
			v.reportBack = true;
			v.seconds = w.seconds;
		}
		w = ActionsInactiveWatch{};
		return v;
	}
	if (!w.inactive) {
		w.inactive = true;
		w.seconds = 0.0f;
		w.nextReport = kActionsReportFirstSeconds;
	}
	if (dt > 0.0f) {
		w.seconds += dt;
	}
	if (w.seconds >= w.nextReport) {
		v.reportInactive = true;
		v.seconds = w.seconds;
		w.nextReport += kActionsReportEverySeconds;
	}
	return v;
}

}  // namespace obvr::vr
