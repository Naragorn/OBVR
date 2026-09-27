#pragma once

#include "core/Types.h"
#include "ui/MenuCanvas.h"

namespace obvr::ui {

// The stow spot at the chest (vr/Stow.h): a gold ring in Oblivion's
// parchment gold, shown while an item is held, filled when the hand is in
// it - let go there and the item goes into the inventory.
struct StowSpotView {
	bool lit = false;
};

// The canvas is kStowSpotCanvas pixels square and hung twice the spot's
// radius wide, so the ring's outer edge is the edge of the zone.
inline constexpr UInt32 kStowSpotCanvas = 256;

void PaintStowSpot(Canvas& canvas, const StowSpotView& view);

// CanvasOverlay's painter signature: context is a StowSpotView.
void PaintStowSpotFor(Canvas& canvas, const void* view);

}  // namespace obvr::ui
