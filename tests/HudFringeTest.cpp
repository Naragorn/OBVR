// Checks the fringe probe (vr/HudFringe.h): how far drawn pixels reach past
// a cut out of the HUD panel, side by side, ring by ring.

#include <cstdio>

#include "vr/HudFringe.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr UInt32 kWidth = 40;
constexpr UInt32 kHeight = 30;
constexpr UInt32 kDrawn = 0x40FFFFFFu;     // a faint white pixel: any alpha counts
constexpr UInt32 kColourOnly = 0x00FFFFFFu;  // colour without alpha: clear

struct Panel {
	UInt32 pixel[kHeight][kWidth] = {};
};

CaptureRect Rect(SInt32 left, SInt32 top, SInt32 right, SInt32 bottom) {
	CaptureRect r;
	r.left = left;
	r.top = top;
	r.right = right;
	r.bottom = bottom;
	r.valid = true;
	return r;
}

HudFringe Probe(const Panel& p, const CaptureRect& r, UInt32 band) {
	HudFringe f = StartHudFringe(r, band);
	for (UInt32 y = 0; y < kHeight; ++y) {
		AccumulateHudFringeRow(f, p.pixel[y], kWidth, y);
	}
	return f;
}

bool Depths(const HudFringe& f, UInt32 left, UInt32 top, UInt32 right, UInt32 bottom) {
	return HudFringeDepth(f, kFringeLeft) == left && HudFringeDepth(f, kFringeTop) == top &&
	       HudFringeDepth(f, kFringeRight) == right && HudFringeDepth(f, kFringeBottom) == bottom;
}

void TestClean() {
	std::printf("A clean cut\n");
	Panel p;
	const HudFringe f = Probe(p, Rect(10, 10, 20, 20), 6);
	Check(Depths(f, 0, 0, 0, 0) && f.pixels[0] + f.pixels[1] + f.pixels[2] + f.pixels[3] == 0 && f.inside == 0,
	      "nothing drawn round it: 0 on every side");
	p.pixel[12][25] = kColourOnly;
	Check(Depths(Probe(p, Rect(10, 10, 20, 20), 6), 0, 0, 0, 0), "a colour without alpha: clear");
}

void TestFringes() {
	std::printf("Fringes on the letters\n");
	Panel p;
	p.pixel[12][20] = kDrawn;  // right, ring 1
	p.pixel[13][21] = kDrawn;  // right, ring 2
	p.pixel[9][15] = kDrawn;   // top, ring 1
	p.pixel[20][11] = kDrawn;  // bottom, ring 1
	p.pixel[21][12] = kDrawn;  // bottom, ring 2
	p.pixel[22][13] = kDrawn;  // bottom, ring 3
	const HudFringe f = Probe(p, Rect(10, 10, 20, 20), 6);
	Check(Depths(f, 0, 1, 2, 3), "each side as deep as its drawn rings next to the edge");
	Check(f.pixels[kFringeRight] == 2 && f.pixels[kFringeTop] == 1 && f.pixels[kFringeBottom] == 3,
	      "the drawn pixels counted per side");
}

void TestNeighbour() {
	std::printf("A neighbour, not a fringe\n");
	Panel p;
	p.pixel[15][6] = kDrawn;  // left, ring 4, rings 1 to 3 clear
	const HudFringe f = Probe(p, Rect(10, 10, 20, 20), 6);
	Check(HudFringeDepth(f, kFringeLeft) == 0 && f.pixels[kFringeLeft] == 1,
	      "behind a clear ring: counted, but no depth");
	p.pixel[15][2] = kDrawn;  // ring 8, outside the band
	Check(Probe(p, Rect(10, 10, 20, 20), 6).pixels[kFringeLeft] == 1, "outside the band: not looked at");
}

void TestCornerAndInside() {
	std::printf("Corners and the inside\n");
	Panel p;
	p.pixel[8][21] = kDrawn;  // 2 right, 2 up: the side it is farther out on, a tie to the side's left or right
	p.pixel[15][15] = kDrawn;
	const HudFringe f = Probe(p, Rect(10, 10, 20, 20), 6);
	Check(f.pixels[kFringeRight] == 1 && f.pixels[kFringeTop] == 0 && HudFringeDepth(f, kFringeRight) == 0,
	      "a corner pixel two out both ways: the right side's second ring");
	Check(f.inside == 1, "a pixel inside the cut: counted as left inside");
	p.pixel[7][21] = kDrawn;  // 2 right, 3 up: the top's
	Check(Probe(p, Rect(10, 10, 20, 20), 6).pixels[kFringeTop] == 1, "farther out upwards: the top's");
}

void TestEdges() {
	std::printf("At the panel's edges, and refusals\n");
	Panel p;
	p.pixel[5][0] = kDrawn;
	p.pixel[5][39] = kDrawn;
	const HudFringe wide = Probe(p, Rect(0, 0, kWidth, 5), 6);
	Check(HudFringeDepth(wide, kFringeBottom) == 1 && wide.pixels[kFringeBottom] == 2,
	      "a cut as wide as the panel: its band kept inside the row");
	const HudFringe none = Probe(p, CaptureRect{}, 6);
	Check(Depths(none, 0, 0, 0, 0) && none.pixels[kFringeBottom] == 0 && none.inside == 0, "no rectangle: nothing");
	Check(StartHudFringe(Rect(0, 0, 1, 1), 1000).band == kHudFringeBandMax, "the band held to its most");
	HudFringe f = StartHudFringe(Rect(10, 10, 20, 20), 6);
	AccumulateHudFringeRow(f, nullptr, kWidth, 12);
	Check(f.inside == 0 && HudFringeDepth(f, 9) == 0, "no row, or a side that is none: nothing");
	Panel full;
	for (UInt32 y = 0; y < kHeight; ++y) {
		for (UInt32 x = 0; x < kWidth; ++x) {
			full.pixel[y][x] = kDrawn;
		}
	}
	Check(Depths(Probe(full, Rect(10, 10, 20, 20), 6), 6, 6, 6, 6), "drawn all round: as deep as the band");
}

}  // namespace

int main() {
	TestClean();
	TestFringes();
	TestNeighbour();
	TestCornerAndInside();
	TestEdges();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
