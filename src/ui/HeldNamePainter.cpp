#include "ui/HeldNamePainter.h"

#include "ui/MenuFont.h"

namespace obvr::ui {
namespace {

constexpr render::Pixel Colour(UInt8 r, UInt8 g, UInt8 b, UInt8 a) {
	return SwapRedAndBlue(render::Pixel{r, g, b, a});
}

// The quick menu's parchment gold on a dark strip, as its labels are.
constexpr render::Pixel kText = Colour(222, 190, 140, 255);
constexpr render::Pixel kStrip = Colour(20, 16, 12, 190);
constexpr render::Pixel kClear = Colour(0, 0, 0, 0);
constexpr SInt32 kPad = 8;

}  // namespace

void PaintHeldName(Canvas& canvas, const HeldNameView& view) {
	canvas.Fill(kClear);
	char fit[kHeldNameMaxChars + 4];
	HeldNameFit(view.name, fit, sizeof(fit));
	const SInt32 chars = static_cast<SInt32>(HeldNameLength(fit));
	if (chars == 0) {
		return;
	}
	const SInt32 textWidth = chars * static_cast<SInt32>(kGlyphAdvance) * kHeldNameTextScale;
	const SInt32 textHeight = static_cast<SInt32>(kGlyphHeight) * kHeldNameTextScale;
	const SInt32 width = static_cast<SInt32>(canvas.Width());
	const SInt32 height = static_cast<SInt32>(canvas.Height());
	const SInt32 x = (width - textWidth) / 2;
	const SInt32 y = (height - textHeight) / 2;
	canvas.FillRect(x - kPad, y - kPad / 2, textWidth + 2 * kPad, textHeight + kPad, kStrip);
	canvas.DrawText(x, y, fit, kHeldNameTextScale, kText);
}

void PaintHeldNameFor(Canvas& canvas, const void* view) {
	PaintHeldName(canvas, *static_cast<const HeldNameView*>(view));
}

}  // namespace obvr::ui
