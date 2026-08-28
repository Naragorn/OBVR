#pragma once

namespace obvr::game {

// What the dialogue-zoom patch should do this tick, given what the INI wants
// and what has already been done. Pure, so every flow is testable without a
// process to patch: the patch is applied once, restored once, and a wish
// already granted costs nothing.
enum class DialogZoomAction {
	Nothing,
	Patch,
	Restore,
};

constexpr DialogZoomAction DecideDialogZoom(bool zoomWanted, bool patched) {
	if (!zoomWanted && !patched) {
		return DialogZoomAction::Patch;
	}
	if (zoomWanted && patched) {
		return DialogZoomAction::Restore;
	}
	return DialogZoomAction::Nothing;
}

// Applies Look.DialogZoom. Off, SetDialogCamera's entry jumps to a shim that
// keeps exactly one of the function's two jobs: a third-person player is
// still flipped into first person when a conversation starts and back when
// it ends - the vanilla dance, asked back after a bare ret removed it - but
// the camera transition whose distance fDlgFocus sets is never started, so
// there is no zoom and no spent transition. On restores the vanilla bytes.
// Verifies the bytes before the first patch and refuses - once, out loud -
// when they are not the ones this build knows. Safe to call every frame; it
// only acts on a change.
void ApplyDialogZoom(bool zoomWanted);

}  // namespace obvr::game
