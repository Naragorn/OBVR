// Checks the name hung under the thing in the hand (ui/HeldNamePainter.h).

#include <cstdio>
#include <cstring>

#include "ui/HeldNamePainter.h"

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

UInt32 CountOpaque(const Canvas& canvas) {
	UInt32 n = 0;
	for (UInt32 y = 0; y < canvas.Height(); ++y) {
		for (UInt32 x = 0; x < canvas.Width(); ++x) {
			if (canvas.GetPixel(static_cast<SInt32>(x), static_cast<SInt32>(y)).a != 0) {
				++n;
			}
		}
	}
	return n;
}

}  // namespace

int main() {
	std::printf("The held thing's name\n");
	Check(HeldNameShown(true, "Iron Longsword"), "held, with a name: shown");
	Check(!HeldNameShown(false, "Iron Longsword"), "not held: not shown");
	Check(!HeldNameShown(true, ""), "held, no name: not shown");
	Check(HeldNameLength("Apple") == 5 && HeldNameLength("") == 0 && HeldNameLength(nullptr) == 0,
	      "the name's length, none for none");

	char fit[kHeldNameMaxChars + 4];
	HeldNameFit("Apple", fit, sizeof(fit));
	Check(std::strcmp(fit, "Apple") == 0, "a short name as it is");
	HeldNameFit("Ayleid Statue of Grandeur", fit, sizeof(fit));
	Check(HeldNameLength(fit) == kHeldNameMaxChars && std::strcmp(fit + kHeldNameMaxChars - 3, "...") == 0,
	      "a long name cut to what fits, ending in dots");
	HeldNameFit("", fit, sizeof(fit));
	Check(fit[0] == '\0', "no name: nothing");
	char tiny[6];
	HeldNameFit("Ayleid Statue of Grandeur", tiny, sizeof(tiny));
	Check(HeldNameLength(tiny) < sizeof(tiny), "a small buffer is never overrun");

	static render::Pixel pixels[kHeldNameCanvasWidth * kHeldNameCanvasHeight];
	Canvas canvas(pixels, kHeldNameCanvasWidth, kHeldNameCanvasHeight);
	HeldNameView view;
	std::strcpy(view.name, "Apple");
	PaintHeldName(canvas, view);
	const UInt32 shortOpaque = CountOpaque(canvas);
	Check(shortOpaque > 0 && shortOpaque < kHeldNameCanvasWidth * kHeldNameCanvasHeight / 2,
	      "a short name: a strip, most of the canvas clear");
	Check(canvas.GetPixel(0, 0).a == 0 && canvas.GetPixel(static_cast<SInt32>(kHeldNameCanvasWidth) - 1, 0).a == 0,
	      "the corners stay clear");
	std::strcpy(view.name, "Iron Longsword");
	PaintHeldName(canvas, view);
	Check(CountOpaque(canvas) > shortOpaque, "a longer name: a wider strip");
	view.name[0] = '\0';
	PaintHeldNameFor(canvas, &view);
	Check(CountOpaque(canvas) == 0, "no name: the canvas clear");

	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
