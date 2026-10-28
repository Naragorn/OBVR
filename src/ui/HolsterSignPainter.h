#pragma once

#include "core/Types.h"
#include "ui/MenuCanvas.h"

namespace obvr::ui {

// The small sign at a place on the body (vr/Holster.h; Nadi, 2026-10-27:
// "Mit kleiner Anzeigetafel wie in B&S"): shown while a hand is at the place -
// what the place keeps ("empty" when nothing), and under it a hint after a
// reach that did nothing there ("Bow, shield, torch: other hand"). White text
// on a quiet, half see-through plate, the hint in red (Nadi, 2026-10-28:
// "dezenter wie B&S"); when and where it shows is vr/HolsterSign.h.
struct HolsterSignView {
	char item[64] = {};
	char hint[48] = {};
};

// The canvas is kHolsterSignWidth by kHolsterSignHeight pixels, hung
// kHolsterSignMetres wide.
inline constexpr UInt32 kHolsterSignWidth = 384;
inline constexpr UInt32 kHolsterSignHeight = 96;
inline constexpr float kHolsterSignMetres = 0.11f;

// The text scale that fits `text` into `width` pixels, at most `largest`
// and at least 1.
SInt32 SignTextScale(const char* text, UInt32 width, SInt32 largest);

void PaintHolsterSign(Canvas& canvas, const HolsterSignView& view);

// CanvasOverlay's painter signature: context is a HolsterSignView.
void PaintHolsterSignFor(Canvas& canvas, const void* view);

}  // namespace obvr::ui
