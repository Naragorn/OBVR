#pragma once

#include "core/Types.h"
#include "ui/MenuCanvas.h"

namespace obvr::ui {

// A small panel of instructions hung in front of the player while a guided
// step waits for them (the weapon places' fit): a title and up to four
// lines, in the quick menu's colours. Colours as the texture keeps them
// (A8R8G8B8, see CanvasOverlay).
inline constexpr UInt32 kGuidePanelWidth = 720;
inline constexpr UInt32 kGuidePanelHeight = 200;

struct GuidePanelText {
	const char* title = "";
	const char* lines[4] = {"", "", "", ""};
};

void PaintGuidePanel(Canvas& canvas, const GuidePanelText& text);

// CanvasOverlay's painter signature: context is a GuidePanelText.
void PaintGuidePanelFor(Canvas& canvas, const void* text);

}  // namespace obvr::ui
