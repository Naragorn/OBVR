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
};

// The canvas is kHoldRingCanvas pixels square, hung kHoldRingMetres wide.
inline constexpr UInt32 kHoldRingCanvas = 128;
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

void PaintHoldRing(Canvas& canvas, const HoldRingView& view);

// CanvasOverlay's painter signature: context is a HoldRingView.
void PaintHoldRingFor(Canvas& canvas, const void* view);

}  // namespace obvr::ui
