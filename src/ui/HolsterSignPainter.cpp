#include "ui/HolsterSignPainter.h"

#include "ui/MenuFont.h"

namespace obvr::ui {
namespace {

constexpr render::Pixel Colour(UInt8 r, UInt8 g, UInt8 b, UInt8 a) {
	return SwapRedAndBlue(render::Pixel{r, g, b, a});
}

// Quiet like Blade & Sorcery's holster prompt ("A white icon that says
// "Holster weapon" should appear. If it's red the slot is already full or the
// weapon is incompatible", r/BladeAndSorcery sbo54y): white text on a
// half see-through plate without an edge, the hint in a soft red.
constexpr render::Pixel kItem = Colour(240, 236, 228, 255);
constexpr render::Pixel kHint = Colour(236, 120, 104, 255);
constexpr render::Pixel kPlate = Colour(12, 10, 8, 120);
constexpr SInt32 kMargin = 8;

void DrawCentred(Canvas& canvas, SInt32 top, const char* text, SInt32 scale, render::Pixel colour) {
	const SInt32 width = static_cast<SInt32>(TextWidth(text)) * scale;
	canvas.DrawText((static_cast<SInt32>(canvas.Width()) - width) / 2, top, text, scale, colour);
}

}  // namespace

SInt32 SignTextScale(const char* text, UInt32 width, SInt32 largest) {
	const UInt32 pixels = TextWidth(text);
	SInt32 scale = largest < 1 ? 1 : largest;
	while (scale > 1 && pixels * static_cast<UInt32>(scale) > width) {
		--scale;
	}
	return scale;
}

void PaintHolsterSign(Canvas& canvas, const HolsterSignView& view) {
	canvas.Fill(kPlate);
	const SInt32 w = static_cast<SInt32>(canvas.Width());
	const SInt32 h = static_cast<SInt32>(canvas.Height());
	const UInt32 inner = static_cast<UInt32>(w - 2 * kMargin);
	const bool hinted = view.hint[0] != '\0';
	const SInt32 itemScale = SignTextScale(view.item, inner, hinted ? 4 : 5);
	const SInt32 itemHeight = static_cast<SInt32>(kGlyphHeight) * itemScale;
	if (!hinted) {
		DrawCentred(canvas, (h - itemHeight) / 2, view.item, itemScale, kItem);
		return;
	}
	const SInt32 hintScale = SignTextScale(view.hint, inner, 3);
	const SInt32 hintHeight = static_cast<SInt32>(kGlyphHeight) * hintScale;
	const SInt32 gap = 8;
	const SInt32 top = (h - itemHeight - gap - hintHeight) / 2;
	DrawCentred(canvas, top, view.item, itemScale, kItem);
	DrawCentred(canvas, top + itemHeight + gap, view.hint, hintScale, kHint);
}

void PaintHolsterSignFor(Canvas& canvas, const void* view) {
	PaintHolsterSign(canvas, *static_cast<const HolsterSignView*>(view));
}

}  // namespace obvr::ui
