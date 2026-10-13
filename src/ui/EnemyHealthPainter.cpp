#include "ui/EnemyHealthPainter.h"

namespace obvr::ui {
namespace {

constexpr render::Pixel Colour(UInt8 r, UInt8 g, UInt8 b, UInt8 a) {
	return SwapRedAndBlue(render::Pixel{r, g, b, a});
}

// The quick menu's and the stow spot's gold (222,190,140).
constexpr render::Pixel kGold = Colour(222, 190, 140, 220);
// A dark bed, a little see-through.
constexpr render::Pixel kBed = Colour(24, 14, 8, 170);
// The health left: the game's own bar red.
constexpr render::Pixel kBlood = Colour(188, 34, 22, 235);
constexpr render::Pixel kClear = render::Pixel{0, 0, 0, 0};

}  // namespace

void PaintEnemyHealth(Canvas& canvas, const EnemyHealthView& view) {
	canvas.Fill(kClear);
	const SInt32 w = static_cast<SInt32>(canvas.Width());
	const SInt32 h = static_cast<SInt32>(canvas.Height());
	if (view.frame != nullptr && view.frameWidth > 0 && view.frameHeight > 0) {
		// The game's frame, scaled to the canvas pixel by pixel (the nearest
		// of the frame's), its own alpha kept.
		for (SInt32 y = 0; y < h; ++y) {
			const UInt32 sy = FrameSource(static_cast<UInt32>(y), canvas.Height(), view.frameHeight);
			for (SInt32 x = 0; x < w; ++x) {
				const UInt32 sx = FrameSource(static_cast<UInt32>(x), canvas.Width(), view.frameWidth);
				canvas.SetPixel(x, y, view.frame[sy * view.frameWidth + sx]);
			}
		}
		return;
	}
	canvas.FillRect(0, 0, w, h, kBed);
	canvas.DrawFrame(0, 0, w, h, kEnemyHealthFrame, kGold);
	const SInt32 innerX = kEnemyHealthInset;
	const SInt32 innerY = kEnemyHealthInset;
	const SInt32 innerWidth = w - 2 * kEnemyHealthInset;
	const SInt32 innerHeight = h - 2 * kEnemyHealthInset;
	canvas.FillRect(innerX, innerY, EnemyHealthFillPixels(view.fraction, innerWidth), innerHeight, kBlood);
}

void PaintEnemyHealthFor(Canvas& canvas, const void* view) {
	PaintEnemyHealth(canvas, *static_cast<const EnemyHealthView*>(view));
}

}  // namespace obvr::ui
