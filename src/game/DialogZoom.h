#pragma once

namespace obvr::game {

// What the fDlgFocus override should do this tick. Pure, so every flow is
// testable without a game to write into.
//
// The override is held rather than applied once, because the order of things
// at startup is not OBVR's to choose: the plugin loads early, the game reads
// its INI into the setting slot at its own moment, and a value written before
// that read would be read over. Asking every frame costs one comparison and
// makes the order irrelevant.
//
// The original is captured the first time the override is wanted - by then
// the INI has long been read, so what is captured is the person's own value,
// and turning the zoom back on returns exactly that.
enum class DialogFocusAction {
	Nothing,

	// First time: remember what the slot holds, then write the override.
	CaptureAndHold,

	// The slot has drifted off the override (the game's INI read, or a
	// restore that was later reversed) - write it again.
	Hold,

	// The zoom is wanted again and the slot still carries the override -
	// put the captured value back.
	Restore,
};

constexpr DialogFocusAction DecideDialogFocus(bool zoomWanted, bool originalKnown,
                                              bool slotHoldsOverride) {
	if (!zoomWanted) {
		if (!originalKnown) {
			return DialogFocusAction::CaptureAndHold;
		}
		if (!slotHoldsOverride) {
			return DialogFocusAction::Hold;
		}
		return DialogFocusAction::Nothing;
	}
	if (originalKnown && slotHoldsOverride) {
		return DialogFocusAction::Restore;
	}
	return DialogFocusAction::Nothing;
}

// Applies Look.DialogZoom by holding the fDlgFocus setting slot at the
// no-zoom figure while the zoom is unwanted, and restoring the captured
// value when it is wanted again. Called once per frame and on every config
// reload; on the frames where nothing changed it is one float comparison.
void ApplyDialogZoom(bool zoomWanted);

}  // namespace obvr::game
