#pragma once

#include "vr/ButtonBindings.h"
#include "vr/ChordHold.h"
#include "vr/HandMode.h"

namespace obvr::vr {

// The quick save's chord in a hand frame (vr/ChordHold.h): the other hand's A
// and B held together, a second in all - the ring up from the chord's first
// moment (kQuickSaveRingSeconds), over that controller, with the quick save's
// sign. The pause menu's ring waits MenuRingSeconds so that a tap of the B
// (Tab) does not flash it; a short A and B together means nothing else, so
// there is nothing to keep apart and the whole second shows. While the chord has them the two buttons are nobody
// else's: HandMode::StepButtons blocks their own taps and holds. In a menu,
// or with nothing bound to the chord (OBVR_Controls.ini QuickSave=None), there
// is no chord and the buttons are as they were.
inline constexpr float kQuickSaveHoldSeconds = 1.0f;
inline constexpr float kChordWindowSeconds = 0.15f;
inline constexpr float kQuickSaveRingSeconds = 0.0f;

// True while the chord owns the left A and B this frame.
inline bool StepQuickSaveChord(ChordHoldState& c, bool aDown, bool bDown, bool menuMode, float dtSeconds,
                               const ControlBindings& bindings, ControlSourceFrame& sources, HandModeResult& r) {
	const ControlAction action = ActionOfSource(bindings, ControlSource::LeftABHold);
	if (menuMode || action == ControlAction::Count) {
		// Kept as held, so a press that outlasts a menu is no new press.
		c = ChordHoldState{};
		c.aDown = aDown;
		c.bDown = bDown;
		return false;
	}
	const ChordHoldVerdict v =
		StepChordHold(c, aDown, bDown, kQuickSaveHoldSeconds, kQuickSaveRingSeconds, kChordWindowSeconds, dtSeconds);
	if (v.ringShown && (!r.menuHoldRing || v.fill > r.menuHoldFill)) {
		r.menuHoldRing = true;
		r.menuHoldFill = v.fill;
		r.menuHoldRight = false;
		r.menuHoldButton = HoldButton::LeftB;
		r.menuHoldAction = action;
	}
	if (v.cancelled) {
		r.menuHoldCancelled = true;
		if (!r.menuHoldRing) {
			r.menuHoldButton = HoldButton::LeftB;
			r.menuHoldAction = action;  // whose ring was let go, for the log
		}
	}
	if (v.reached) {
		r.holdReached = HoldButton::LeftB;
		r.holdReachedAction = action;
	}
	sources.Fire(ControlSource::LeftABHold, v.reached);
	return v.active;
}

}  // namespace obvr::vr
