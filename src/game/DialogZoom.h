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

// Applies Look.DialogZoom: patches PlayerCharacter::SetDialogCamera to return
// immediately when the zoom is unwanted, and puts the original bytes back when
// it is wanted again. Verifies the bytes before the first patch and refuses -
// once, out loud - when they are not the ones this build knows. Safe to call
// every reload; it only acts on a change.
void ApplyDialogZoom(bool zoomWanted);

}  // namespace obvr::game
