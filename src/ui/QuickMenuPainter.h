#pragma once

#include "core/Types.h"
#include "ui/MenuCanvas.h"

namespace obvr::ui {

// What the quick menu's ring shows (vr/QuickMenu.h): the eight hotkeys'
// names, which hold something, which one the hand has chosen.
struct QuickMenuView {
	char names[8][40] = {};
	bool filled[8] = {};
	int highlighted = -1;
	// Opened in the inventory or the magic menu: the ring sets a hotkey, so
	// every slot can be chosen and the middle says so.
	bool assigning = false;
	// Each slot's icon, kQuickMenuIconSide square in the texture's channel
	// order (game/ItemIcons.h), or null: the slot shows its number and name
	// only. Borrowed, not owned.
	const render::Pixel* icons[8] = {};
};

// The icon's side in pixels, drawn one to one.
inline constexpr UInt32 kQuickMenuIconSide = 64;
// A slot's box: the number at its top left, the icon at the top in the
// middle, the name in two lines under it.
inline constexpr SInt32 kQuickMenuBoxWidth = 160;
inline constexpr SInt32 kQuickMenuBoxHeight = 116;

// The words in the middle of the setting ring.
inline constexpr const char* kQuickMenuAssignLabel = "Set hotkey";

// The ring's canvas: kQuickMenuCanvas pixels square, the slots on a circle of
// kQuickMenuRingPixels around the centre in the order vr::QuickSlotCentre
// gives (the "1" slot at the top, then round to the right). Colours as the
// texture keeps them (A8R8G8B8, see CanvasOverlay).
inline constexpr UInt32 kQuickMenuCanvas = 640;
inline constexpr SInt32 kQuickMenuRingPixels = 230;

// The width the canvas is hung at so that its slots sit where the hand
// chooses them: the ring's radius on the canvas is ringMetres.
inline float QuickMenuWidthMetres(float ringMetres) {
	return ringMetres * static_cast<float>(kQuickMenuCanvas) /
	       static_cast<float>(kQuickMenuRingPixels);
}

// Cuts a name into at most two lines of `perLine` characters, at a space
// where there is one. Pure, for the painter and its test.
void SplitName(const char* name, UInt32 perLine, char (&first)[40], char (&second)[40]);

void PaintQuickMenu(Canvas& canvas, const QuickMenuView& view);

// CanvasOverlay's painter signature: context is a QuickMenuView.
void PaintQuickMenuFor(Canvas& canvas, const void* view);

}  // namespace obvr::ui
