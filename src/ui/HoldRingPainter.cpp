#include "ui/HoldRingPainter.h"

namespace obvr::ui {
namespace {

constexpr render::Pixel Colour(UInt8 r, UInt8 g, UInt8 b, UInt8 a) {
	return SwapRedAndBlue(render::Pixel{r, g, b, a});
}

// The quick menu's and the stow spot's gold (222,190,140).
constexpr render::Pixel kGold = Colour(222, 190, 140, 240);
constexpr render::Pixel kTrack = Colour(222, 190, 140, 70);
constexpr render::Pixel kInside = Colour(20, 16, 10, 40);
constexpr render::Pixel kClear = render::Pixel{0, 0, 0, 0};

}  // namespace

void PaintHoldRing(Canvas& canvas, const HoldRingView& view) {
	canvas.Fill(kClear);
	const SInt32 size = static_cast<SInt32>(kHoldRingCanvas);
	const float centre = (static_cast<float>(size) - 1.0f) * 0.5f;
	const float outer = static_cast<float>(size) * 0.5f - 1.0f;
	const float band = static_cast<float>(size) * 0.16f;
	const float inner = outer - band;
	for (SInt32 y = 0; y < size; ++y) {
		for (SInt32 x = 0; x < size; ++x) {
			const float dx = static_cast<float>(x) - centre;
			const float dy = static_cast<float>(y) - centre;
			const float d2 = dx * dx + dy * dy;
			if (d2 > outer * outer) {
				continue;
			}
			if (d2 < inner * inner) {
				canvas.SetPixel(x, y, kInside);
			} else {
				canvas.SetPixel(x, y, HoldRingArcCovers(dx, dy, view.fill) ? kGold : kTrack);
			}
		}
	}
}

void PaintHoldRingFor(Canvas& canvas, const void* view) {
	PaintHoldRing(canvas, *static_cast<const HoldRingView*>(view));
}

}  // namespace obvr::ui
