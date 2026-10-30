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

int Lit(const std::vector<render::Pixel>& pixels, UInt32 x, UInt32 y) {
	return pixels[y * kHoldRingCanvas + x].a;
}

void TestPaint() {
	std::printf("the picture: a faint track, the filled part bright\n");
	std::vector<render::Pixel> pixels(kHoldRingCanvas * kHoldRingCanvas);
	Canvas canvas(pixels.data(), kHoldRingCanvas, kHoldRingCanvas);
	HoldRingView view;
	view.fill = 0.3f;
	PaintHoldRing(canvas, view);
	const UInt32 mid = kHoldRingCanvas / 2;
	const UInt32 band = kHoldRingCanvas / 2 - 10;  // inside the ring's band, from the centre
	const int right = Lit(pixels, mid + band, mid);
	const int left = Lit(pixels, mid - band, mid);
	const int centre = Lit(pixels, mid, mid);
	const int corner = Lit(pixels, 0, 0);
	Check(right > left, "the right side (filled) brighter than the left (the track)");
	Check(left > 0, "the track shown faintly");
	Check(corner == 0, "outside the ring: clear");
	Check(centre < left, "the middle: clearer than the track");
	view.fill = 0.0f;
	PaintHoldRing(canvas, view);
	Check(Lit(pixels, mid + band, mid) == Lit(pixels, mid - band, mid), "empty: the track alone, all round alike");
}

}  // namespace

int main() {
	TestArc();
	TestPaint();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
