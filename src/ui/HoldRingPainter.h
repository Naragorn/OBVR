#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "ui/MenuCanvas.h"

namespace obvr::ui {

// The ring over the controller while the left B is held toward the pause
// menu (vr::StepButtonHold; Nadi, 2026-10-30: "mit kreis über dem controller
// der sich füllt, wenn voll -> Esc menü"): a faint track all round, the
// hold's share of it bright, filling clockwise from the top - the quick
// menu's gold.
struct HoldRingView {
	float fill = 0.0f;  // 0..1
	// The sign in the middle: the menu's three bars, or for the quick save
	// (the other A and B held together, vr/QuickSaveChord.h) a floppy disk.
	bool save = false;
};

// Whether the canvas point (x, y) lies on the floppy disk drawn in a disc of
// radius `disc` about `centre`, 0..1 with a soft edge: the body a square with
// one corner cut, the shutter a slot at the top, the label a band below -
// those two cut out of the body so the parchment shows through.
float FloppyCover(float x, float y, float centre, float disc);

// The canvas is kHoldRingCanvas pixels square, hung kHoldRingMetres wide.
inline constexpr UInt32 kHoldRingCanvas = 192;
inline constexpr float kHoldRingMetres = 0.06f;

// Whether the filled arc covers the direction (dx, dy) from the centre, the
// canvas's y growing downwards: clockwise from the top by `fill` of a turn.
inline bool HoldRingArcCovers(float dx, float dy, float fill) {
	if (fill <= 0.0f) {
		return false;
	}
	if (fill >= 1.0f) {
		return true;
	}
	// Clockwise from up (-y) on the screen: atan2 of x against -y.
	float turn = math::Atan2(dx, -dy) / (2.0f * math::kPi);
	if (turn < 0.0f) {
		turn += 1.0f;
	}
	return turn <= fill;
}

// After the quick save the ring stays full over the controller for
// kSaveFlashSeconds and blinks: shown kSaveBlinkSeconds, gone as long, from
// the save on - the "saved" a hand can see while the game's own
// "Quicksaving..." line is on the HUD.
inline constexpr float kSaveFlashSeconds = 0.6f;
inline constexpr float kSaveBlinkSeconds = 0.15f;
inline bool SaveFlashShown(float secondsLeft) {
	if (!(secondsLeft > 0.0f) || secondsLeft > kSaveFlashSeconds) {
		return false;
	}
	const int phase = static_cast<int>((kSaveFlashSeconds - secondsLeft) / kSaveBlinkSeconds);
	return phase % 2 == 0;
}

void PaintHoldRing(Canvas& canvas, const HoldRingView& view);

// CanvasOverlay's painter signature: context is a HoldRingView.
void PaintHoldRingFor(Canvas& canvas, const void* view);

}  // namespace obvr::ui
