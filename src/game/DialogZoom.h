#pragma once

namespace obvr::game {

// A hot reload cannot switch camera implementations halfway through a
// conversation: the implementation that opened it must also close it.
struct DialogZoomRoute {
	bool inConversation = false;
	bool zoom = false;
	bool UseOriginal(bool hasActor, bool wanted) {
		if (!inConversation) zoom = wanted;
		inConversation = hasActor;
		return zoom;
	}
};

// What the shim does to the point of view on one SetDialogCamera call. Pure,
// because the first version of this decision had a latch bug a test would
// have caught: the start path was guarded on "not flipped yet" as well as on
// being in third person, and when the game's call pattern left the latch
// set, every dialogue after the first went unflipped. Whether to flip is the
// player's current point of view's business alone; the latch only says
// whether there is a flip to undo.
enum class DialogPovAction {
	Nothing,
	FlipToFirst,
	FlipBack,
};

constexpr DialogPovAction DecideDialogPov(bool conversationStarting, bool isThirdPerson,
                                          bool flippedForDialog, bool flipWanted) {
	if (conversationStarting) {
		return (flipWanted && isThirdPerson) ? DialogPovAction::FlipToFirst
		                                     : DialogPovAction::Nothing;
	}
	// The end of a conversation undoes OBVR's own flip and nothing else -
	// including when the feature was switched off mid-dialogue, because a
	// flip this shim made is this shim's to clean up.
	return flippedForDialog ? DialogPovAction::FlipBack : DialogPovAction::Nothing;
}

// Installs a verified observer once. Look.DialogZoom chooses whether its
// original trampoline runs; the speaker/eyes observation is always retained.
void ApplyDialogZoom(bool zoomWanted);

// The approach before a conversation: the engine calls SetDialogCamera with
// the actor once a frame while the speaker turns to the player - a dozen
// frames and more in the 2026-09-25 log - and only then opens the dialogue
// menu. The hands, hidden in menus, were still up for that stretch and
// vanished only with the menu. So they go with the first call instead, and
// stay gone for kDialogApproachHoldFrames after the last one, which bridges
// the frame between the last call and the menu.
constexpr int kDialogApproachHoldFrames = 3;

struct DialogApproachState {
	int holdFrames = 0;
};

// Once per frame: whether the dialogue approach is on, so the hands hide.
inline bool StepDialogApproach(DialogApproachState& s, bool calledWithActor, bool menuIsUp) {
	if (calledWithActor) {
		s.holdFrames = kDialogApproachHoldFrames;
	} else if (s.holdFrames > 0) {
		--s.holdFrames;
	}
	if (menuIsUp) {
		s.holdFrames = 0;  // the menu hides them from here on
	}
	return s.holdFrames > 0;
}

// Whether SetDialogCamera was called with an actor since the last ask -
// consumed by the ask. The observer sees both zoom modes.
bool TakeDialogCameraCall();
// The same, without consuming it: for the render that comes before the ask.
bool DialogCameraCallPending();

}  // namespace obvr::game
