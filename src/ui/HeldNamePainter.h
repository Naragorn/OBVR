#pragma once

#include "core/Types.h"
#include "ui/MenuCanvas.h"

namespace obvr::ui {

// The name of the thing in the hand, hung under it while it is held (the
// tester, 2026-10-07: "objekte die ich vor mir halte zeigen keinen namen
// an"). The game's own info text (HUDInfoMenu) names what the pick is on,
// and the pick does not name a thing the grab holds, so OBVR paints the name
// itself: the form's TESFullName in the menu font on a dark strip, the way
// the quick menu's labels are drawn.
struct HeldNameView {
	char name[64] = "";
};

inline constexpr UInt32 kHeldNameCanvasWidth = 512;
inline constexpr UInt32 kHeldNameCanvasHeight = 48;
inline constexpr SInt32 kHeldNameTextScale = 4;
inline constexpr UInt32 kHeldNameMaxChars = 20;  // what fits the strip at that scale

// How wide the canvas hangs, metres; the strip behind the text is only as
// wide as the text, the rest of the canvas is clear.
inline constexpr float kHeldNameWidthMetres = 0.36f;

inline UInt32 HeldNameLength(const char* name) {
	UInt32 n = 0;
	while (name != nullptr && name[n] != '\0') {
		++n;
	}
	return n;
}

// Shown while something is held and it has a name.
inline bool HeldNameShown(bool holding, const char* name) {
	return holding && HeldNameLength(name) > 0;
}

// The name cut to what fits, with "..." when it was longer.
inline void HeldNameFit(const char* name, char* out, UInt32 size) {
	const UInt32 n = HeldNameLength(name);
	UInt32 keep = n > kHeldNameMaxChars ? kHeldNameMaxChars - 3 : n;
	if (keep + 4 > size) {
		keep = size > 4 ? size - 4 : 0;
	}
	UInt32 i = 0;
	for (; i < keep; ++i) {
		out[i] = name[i];
	}
	if (n > kHeldNameMaxChars && i + 3 < size) {
		out[i++] = '.';
		out[i++] = '.';
		out[i++] = '.';
	}
	out[i] = '\0';
}

void PaintHeldName(Canvas& canvas, const HeldNameView& view);

// CanvasOverlay's painter signature: context is a HeldNameView.
void PaintHeldNameFor(Canvas& canvas, const void* view);

}  // namespace obvr::ui
