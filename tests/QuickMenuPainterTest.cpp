// Checks the quick menu's picture (ui/QuickMenuPainter.h): names cut into
// lines, and the ring drawn where the hand chooses its slots.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ui/QuickMenuPainter.h"
#include "ui/StowSpotPainter.h"
#include "vr/QuickMenu.h"

using namespace obvr;
using namespace obvr::ui;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Same(render::Pixel a, render::Pixel b) {
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

void TestSplit() {
	std::printf("Names on two lines\n");
	char a[40];
	char b[40];
	SplitName("Dagger", 12, a, b);
	Check(std::strcmp(a, "Dagger") == 0 && b[0] == '\0', "a short name: one line");
	SplitName("Iron Longsword", 12, a, b);
	Check(std::strcmp(a, "Iron") == 0 && std::strcmp(b, "Longsword") == 0, "broken at the space");
	SplitName("Potion of Healing", 12, a, b);
	Check(std::strcmp(a, "Potion of") == 0 && std::strcmp(b, "Healing") == 0,
	      "at the last space that fits");
	SplitName("Umbraswordofdoom", 12, a, b);
	Check(std::strcmp(a, "Umbraswordof") == 0 && std::strcmp(b, "doom") == 0,
	      "no space: cut at the width");
	SplitName("Scroll of Ancient Fire Dragon Breath", 12, a, b);
	Check(std::strcmp(a, "Scroll of") == 0 && std::strcmp(b, "Ancient Fir.") == 0,
	      "a second line too long ends in a dot");
	SplitName("Exactly Twel", 12, a, b);
	Check(std::strcmp(a, "Exactly Twel") == 0 && b[0] == '\0', "exactly the width: one line");
	SplitName(nullptr, 12, a, b);
	Check(a[0] == '\0' && b[0] == '\0', "no name: nothing");
	SplitName("x", 0, a, b);
	Check(a[0] == '\0', "no room: nothing");
}

void TestPaint() {
	std::printf("The ring\n");
	std::vector<render::Pixel> pixels(kQuickMenuCanvas * kQuickMenuCanvas);
	Canvas canvas(pixels.data(), kQuickMenuCanvas, kQuickMenuCanvas);
	QuickMenuView view;
	for (int i = 0; i < 8; ++i) {
		view.filled[i] = i != 4;
		std::snprintf(view.names[i], sizeof(view.names[i]), "Item %d", i + 1);
	}
	view.highlighted = 2;
	PaintQuickMenu(canvas, view);
	const SInt32 c = static_cast<SInt32>(kQuickMenuCanvas) / 2;
	const render::Pixel gold = SwapRedAndBlue(render::Pixel{222, 190, 140, 255});
	Check(Same(canvas.GetPixel(c, c), gold), "a gold diamond in the middle");
	Check(canvas.GetPixel(c + 40, c + 40).a == 0, "between the slots: clear");
	// Halfway round from slot 1 to slot 2, on the ring: the boxes do not
	// reach each other (they overlapped at the first size, 2026-09-27).
	Check(canvas.GetPixel(c + 81, c - 196).a == 0, "neighbouring slots do not overlap");
	// Slot 3 (index 2) is at the right; a point in its box away from text.
	Check(Same(canvas.GetPixel(c + kQuickMenuRingPixels + 70, c + 30), gold),
	      "the chosen slot filled gold");
	// Slot 1 at the top, filled, not chosen.
	const render::Pixel top = canvas.GetPixel(c + 70, c - kQuickMenuRingPixels + 30);
	Check(top.a == 215, "a filled slot: the dark panel");
	// Slot 5 at the bottom, empty.
	const render::Pixel bottom = canvas.GetPixel(c + 70, c + kQuickMenuRingPixels + 30);
	Check(bottom.a == 120, "an empty slot: fainter");
	// Where the hand chooses slot 3 is where it is drawn: the ring radius on
	// the canvas is the ring in metres once hung at QuickMenuWidthMetres.
	const float width = QuickMenuWidthMetres(0.1f);
	Check(std::fabs(width * kQuickMenuRingPixels / kQuickMenuCanvas - 0.1f) < 1e-4f,
	      "hung so the drawn ring is the chosen ring");
	float u = 0.0f;
	float v = 0.0f;
	vr::QuickSlotCentre(2, 0.1f, u, v);
	Check(u > 0.09f && std::fabs(v) < 1e-3f, "slot 3 chosen at the right, drawn at the right");

	view.highlighted = -1;
	PaintQuickMenu(canvas, view);
	Check(canvas.GetPixel(c + kQuickMenuRingPixels + 70, c + 30).a == 215,
	      "nothing chosen: every filled slot dark");
	PaintQuickMenuFor(canvas, &view);
	Check(Same(canvas.GetPixel(c, c), gold), "the overlay's painter paints the same");

	// The frame of the empty slot 5, at the bottom: its top-left corner.
	const SInt32 edgeX = c - 80;
	const SInt32 edgeY = c + kQuickMenuRingPixels - 40;
	Check(!Same(canvas.GetPixel(edgeX, edgeY), gold), "using: an empty slot's edge is grey");
	bool labelInk = false;
	for (SInt32 y = c + 24; y < c + 24 + 20 && !labelInk; ++y) {
		for (SInt32 x = c - 80; x < c + 80; ++x) {
			labelInk = labelInk || Same(canvas.GetPixel(x, y), gold);
		}
	}
	Check(!labelInk, "using: nothing written in the middle");
	view.assigning = true;
	PaintQuickMenu(canvas, view);
	Check(Same(canvas.GetPixel(edgeX, edgeY), gold), "setting: the empty slot's edge is gold too");
	labelInk = false;
	for (SInt32 y = c + 24; y < c + 24 + 20 && !labelInk; ++y) {
		for (SInt32 x = c - 80; x < c + 80; ++x) {
			labelInk = labelInk || Same(canvas.GetPixel(x, y), gold);
		}
	}
	Check(labelInk, "setting: \"Set hotkey\" written under the middle");
}

void TestStowSpot() {
	std::printf("The stow spot\n");
	std::vector<render::Pixel> pixels(kStowSpotCanvas * kStowSpotCanvas);
	Canvas canvas(pixels.data(), kStowSpotCanvas, kStowSpotCanvas);
	const SInt32 c = static_cast<SInt32>(kStowSpotCanvas) / 2;
	StowSpotView view;
	PaintStowSpot(canvas, view);
	Check(canvas.GetPixel(0, 0).a == 0, "outside the circle: clear");
	Check(canvas.GetPixel(c, 3).a > 0, "the ring at the edge");
	Check(canvas.GetPixel(c, c / 2).a == 0, "inside the ring, not lit: clear");
	Check(canvas.GetPixel(c, c).a > 0, "the diamond in the middle");
	const UInt8 faint = canvas.GetPixel(c, 3).a;
	view.lit = true;
	PaintStowSpotFor(canvas, &view);
	Check(canvas.GetPixel(c, c / 2).a > 0, "lit: filled");
	Check(canvas.GetPixel(c, 3).a > faint, "lit: the ring brighter");
	Check(canvas.GetPixel(0, 0).a == 0, "lit, outside the circle: still clear");
}

}  // namespace

int main() {
	TestStowSpot();
	TestSplit();
	TestPaint();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
