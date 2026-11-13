#include <cstdio>
#include <vector>

#include "ui/HoldRingPainter.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::ui;

void TestArc() {
	std::printf("the arc grows clockwise from the top\n");
	// Canvas y grows downwards: up is -y.
	Check(!HoldRingArcCovers(0.0f, -1.0f, 0.0f), "empty: not even the top");
	Check(HoldRingArcCovers(0.01f, -1.0f, 0.1f), "a tenth: just right of the top");
	Check(!HoldRingArcCovers(1.0f, 0.0f, 0.1f), "but not the right side");
	Check(HoldRingArcCovers(1.0f, 0.0f, 0.3f), "three tenths: the right side (a quarter)");
	Check(!HoldRingArcCovers(0.0f, 1.0f, 0.3f), "not the bottom");
	Check(HoldRingArcCovers(0.0f, 1.0f, 0.55f), "past half: the bottom");
	Check(!HoldRingArcCovers(-1.0f, 0.0f, 0.55f), "not the left");
	Check(HoldRingArcCovers(-1.0f, 0.0f, 0.8f), "four fifths: the left");
	Check(!HoldRingArcCovers(-0.01f, -1.0f, 0.95f), "just left of the top: the last bit, not yet");
	Check(HoldRingArcCovers(-0.01f, -1.0f, 1.0f), "full: all of it");
}

int Alpha(const std::vector<render::Pixel>& pixels, UInt32 x, UInt32 y) {
	return pixels[y * kHoldRingCanvas + x].a;
}

int Bright(const std::vector<render::Pixel>& pixels, UInt32 x, UInt32 y) {
	const render::Pixel& p = pixels[y * kHoldRingCanvas + x];
	return p.r + p.g + p.b;
}

void TestPaint() {
	std::printf("the picture: Oblivion's look - a bronze-rimmed ring, gold where filled, the menu sign inside\n");
	std::vector<render::Pixel> pixels(kHoldRingCanvas * kHoldRingCanvas);
	Canvas canvas(pixels.data(), kHoldRingCanvas, kHoldRingCanvas);
	HoldRingView view;
	view.fill = 0.3f;
	PaintHoldRing(canvas, view);
	const UInt32 mid = kHoldRingCanvas / 2;
	const UInt32 band = static_cast<UInt32>(kHoldRingCanvas * 0.40f);  // the band's middle, from the centre
	Check(Bright(pixels, mid + band, mid) > Bright(pixels, mid - band, mid) + 200,
	      "the right side (filled) gold, far brighter than the left (the track)");
	Check(Alpha(pixels, mid - band, mid) > 100, "the track shown, dark");
	Check(Alpha(pixels, 0, 0) == 0, "outside the ring: clear");
	Check(Alpha(pixels, mid + band, mid) > 200, "the fill nearly opaque");
	// The menu sign: three dark bars on the parchment.
	const UInt32 between = mid - static_cast<UInt32>(kHoldRingCanvas * 0.045f);
	Check(Alpha(pixels, mid, between) > 200 && Bright(pixels, mid, between) > 500, "the parchment between the bars");
	Check(Bright(pixels, mid, mid) < 200, "the middle bar: dark ink");
	const UInt32 upperBar = mid - static_cast<UInt32>(kHoldRingCanvas * 0.085f);
	Check(Bright(pixels, mid, upperBar) < 250, "the upper bar: dark ink");
	Check(Bright(pixels, mid, kHoldRingCanvas - 1 - upperBar) < 250, "the lower bar: dark ink");
	// The gap between the ring and the parchment: see-through.
	const UInt32 gap = static_cast<UInt32>(kHoldRingCanvas * 0.30f);
	Check(Alpha(pixels, mid + gap, mid) < 80, "between the ring and the parchment: see-through");
	view.fill = 0.0f;
	PaintHoldRing(canvas, view);
	Check(Bright(pixels, mid + band, mid) == Bright(pixels, mid - band, mid), "empty: the track alone, all round alike");
	view.fill = 1.0f;
	PaintHoldRing(canvas, view);
	Check(Bright(pixels, mid - band, mid) - Bright(pixels, mid + band, mid) < 40 &&
	          Bright(pixels, mid + band, mid) - Bright(pixels, mid - band, mid) < 40 &&
	          Bright(pixels, mid - band, mid) > 400,
	      "full: gold all round");
}

void TestFloppy() {
	std::printf("the quick save's sign: a floppy disk instead of the bars\n");
	const float c = 95.5f;
	const float disc = 54.0f;
	Check(FloppyCover(72.0f, 90.0f, c, disc) > 0.99f, "the body beside the label: covered");
	Check(FloppyCover(95.0f, 110.0f, c, disc) < 0.01f, "the label: cut out");
	Check(FloppyCover(93.0f, 78.0f, c, disc) < 0.01f, "the shutter's slot: cut out");
	Check(FloppyCover(122.0f, 69.0f, c, disc) < 0.01f, "the top right corner: cut off");
	Check(FloppyCover(110.0f, 72.0f, c, disc) > 0.99f, "beside the corner and the slot: covered");
	Check(FloppyCover(c, c - disc * 0.9f, c, disc) < 0.01f && FloppyCover(c + disc * 0.9f, c, c, disc) < 0.01f,
	      "outside the square: nothing");

	std::vector<render::Pixel> pixels(kHoldRingCanvas * kHoldRingCanvas);
	Canvas canvas(pixels.data(), kHoldRingCanvas, kHoldRingCanvas);
	HoldRingView view;
	view.fill = 0.5f;
	view.save = true;
	PaintHoldRing(canvas, view);
	Check(Bright(pixels, 72, 90) < 250, "painted: the disk's body dark ink");
	Check(Bright(pixels, 95, 110) > 500, "painted: its label parchment");
	const UInt32 mid = kHoldRingCanvas / 2;
	const UInt32 between = mid - static_cast<UInt32>(kHoldRingCanvas * 0.045f);
	view.save = false;
	PaintHoldRing(canvas, view);
	const int barsBetween = Bright(pixels, 72, between);
	view.save = true;
	PaintHoldRing(canvas, view);
	Check(Bright(pixels, 72, between) < 250 && barsBetween > 500,
	      "the menu's parchment between its bars is the disk's body here: a different sign");

	std::printf("the blink after the save\n");
	Check(SaveFlashShown(kSaveFlashSeconds) && SaveFlashShown(kSaveFlashSeconds - 0.1f), "shown first");
	Check(!SaveFlashShown(kSaveFlashSeconds - 0.2f), "then gone");
	Check(SaveFlashShown(kSaveFlashSeconds - 0.35f), "then shown again");
	Check(!SaveFlashShown(0.0f) && !SaveFlashShown(-1.0f) && !SaveFlashShown(kSaveFlashSeconds + 0.01f),
	      "outside the flash: not shown");
}

}  // namespace

int main() {
	TestArc();
	TestPaint();
	TestFloppy();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
