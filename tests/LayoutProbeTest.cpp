// Exercises the layout probe's pure half: the pixel criterion and the box
// arithmetic. The readback plumbing needs a device and is left to the game;
// what can be wrong in arithmetic is covered here.

#include <cstdio>

#include "render/LayoutProbe.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s: %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestPixelCovered() {
	std::printf("When one pixel is judged drawn-on\n");

	using obvr::render::PixelCovered;

	// By alpha, for OBVR's own capture texture: any alpha at all is the
	// pass's doing, colour alone is not.
	Check(PixelCovered(0x01000000u, true), "the faintest alpha counts");
	Check(PixelCovered(0xFF804020u, true), "an opaque pixel counts");
	Check(!PixelCovered(0x00FFFFFFu, true), "colour without alpha does not");
	Check(!PixelCovered(0x00000000u, true), "transparent black does not");

	// By colour, for the game's back buffer, whose alpha means nothing:
	// anything above the noise floor counts, black and near-black do not -
	// a film's own letterbox bars must not widen the box.
	Check(PixelCovered(0x00104080u, false), "a visible colour counts");
	Check(PixelCovered(0xFF0000F0u, false), "one bright channel is enough");
	Check(!PixelCovered(0xFF000000u, false), "black does not, whatever its alpha");
	Check(!PixelCovered(0x000A0A0Au, false), "noise-floor grey does not");
}

void TestAccumulateCoveredRow() {
	std::printf("When rows fold into the covered box\n");

	using obvr::render::AccumulateCoveredRow;
	using obvr::render::CoveredRect;

	// A row of nothing leaves the box untouched.
	{
		const UInt32 row[4] = {0, 0, 0, 0};
		CoveredRect box{};
		AccumulateCoveredRow(row, 4, 7, true, box);
		Check(box.covered == 0, "an empty row leaves no box");
	}

	// The first covered pixel opens the box at its own coordinates.
	{
		const UInt32 row[4] = {0, 0xFF000000u, 0, 0};
		CoveredRect box{};
		AccumulateCoveredRow(row, 4, 7, true, box);
		Check(box.covered == 1 && box.minX == 1 && box.maxX == 1 && box.minY == 7 &&
		          box.maxY == 7,
		      "one pixel is a one-pixel box");
	}

	// Later rows widen and lower the box but never raise its top.
	{
		const UInt32 rowA[4] = {0, 0xFF000000u, 0, 0};
		const UInt32 rowB[4] = {0xFF000000u, 0, 0, 0xFF000000u};
		CoveredRect box{};
		AccumulateCoveredRow(rowA, 4, 2, true, box);
		AccumulateCoveredRow(rowB, 4, 5, true, box);
		Check(box.covered == 3 && box.minX == 0 && box.maxX == 3 && box.minY == 2 &&
		          box.maxY == 5,
		      "two rows make the bounding box of both");
	}

	// The criterion flows through: by colour, a black pixel in the row is
	// not part of the picture.
	{
		const UInt32 row[3] = {0xFF000000u, 0x00808080u, 0xFF000000u};
		CoveredRect box{};
		AccumulateCoveredRow(row, 3, 0, false, box);
		Check(box.covered == 1 && box.minX == 1 && box.maxX == 1,
		      "black neighbours stay outside the picture's box");
	}
}

}  // namespace

int main() {
	std::printf("OBVR layout probe test\n\n");

	TestPixelCovered();
	std::printf("\n");
	TestAccumulateCoveredRow();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
