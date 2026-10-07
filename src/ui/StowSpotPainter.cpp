#include "ui/StowSpotPainter.h"

namespace obvr::ui {
namespace {

constexpr render::Pixel Colour(UInt8 r, UInt8 g, UInt8 b, UInt8 a) {
	return SwapRedAndBlue(render::Pixel{r, g, b, a});
}

// The quick menu's and the teleport ring's gold (222,190,140).
constexpr render::Pixel kGold = Colour(222, 190, 140, 230);
constexpr render::Pixel kGoldFaint = Colour(222, 190, 140, 150);
constexpr render::Pixel kFillLit = Colour(222, 190, 140, 90);
constexpr render::Pixel kFillFaint = Colour(222, 190, 140, 40);
constexpr render::Pixel kClear = render::Pixel{0, 0, 0, 0};

}  // namespace

void PaintStowSpot(Canvas& canvas, const StowSpotView& view) {
	canvas.Fill(kClear);
	const SInt32 size = static_cast<SInt32>(kStowSpotCanvas);
	const float centre = (static_cast<float>(size) - 1.0f) * 0.5f;
	const float outer = static_cast<float>(size) * 0.5f - 1.0f;
	// A circle, not a ring (the tester, 2026-10-07: "eher ein kreis"): a
	// faint fill out to the edge with a thin edge line, the fill firmer and
	// the edge wider while a hand holds it.
	const float ring = view.lit ? 10.0f : 5.0f;
	for (SInt32 y = 0; y < size; ++y) {
		for (SInt32 x = 0; x < size; ++x) {
			const float dx = static_cast<float>(x) - centre;
			const float dy = static_cast<float>(y) - centre;
			const float d2 = dx * dx + dy * dy;
			if (d2 > outer * outer) {
				continue;
			}
			if (d2 >= (outer - ring) * (outer - ring)) {
				canvas.SetPixel(x, y, view.lit ? kGold : kGoldFaint);
			} else {
				canvas.SetPixel(x, y, view.lit ? kFillLit : kFillFaint);
			}
		}
	}
	canvas.FillDiamond(size / 2, size / 2, view.lit ? 16 : 10, view.lit ? kGold : kGoldFaint);
}

void PaintStowSpotFor(Canvas& canvas, const void* view) {
	PaintStowSpot(canvas, *static_cast<const StowSpotView*>(view));
}

}  // namespace obvr::ui
