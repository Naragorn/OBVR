#pragma once

#include "core/Types.h"
#include "ui/MenuCanvas.h"

namespace obvr::ui {

// A fighter's health bar over their head (game/EnemyHealthLogic.h): a dark
// translucent bed with a thin gold frame - Oblivion's parchment gold, the
// quick menu's - and a red fill as wide as the health left. Nothing else on
// it: a bar, not a tooltip.
struct EnemyHealthView {
	float fraction = 1.0f;  // 0 to 1
};

// The canvas is kEnemyHealthCanvasWidth by kEnemyHealthCanvasHeight pixels
// (8:1); the overlay keeps the shape at whatever width it hangs.
inline constexpr UInt32 kEnemyHealthCanvasWidth = 256;
inline constexpr UInt32 kEnemyHealthCanvasHeight = 32;
// The frame's thickness and the bed's inset, pixels.
inline constexpr SInt32 kEnemyHealthFrame = 2;
inline constexpr SInt32 kEnemyHealthInset = 4;

// How many pixels of the fill a fraction is: none at 0, the whole inner
// width at 1, at least one while anything is left.
inline SInt32 EnemyHealthFillPixels(float fraction, SInt32 innerWidth) {
	if (!(fraction > 0.0f) || innerWidth <= 0) {
		return 0;
	}
	const float f = fraction > 1.0f ? 1.0f : fraction;
	const SInt32 pixels = static_cast<SInt32>(f * static_cast<float>(innerWidth) + 0.5f);
	return pixels < 1 ? 1 : pixels;
}

void PaintEnemyHealth(Canvas& canvas, const EnemyHealthView& view);

// CanvasOverlay's painter signature: context is an EnemyHealthView.
void PaintEnemyHealthFor(Canvas& canvas, const void* view);

}  // namespace obvr::ui
