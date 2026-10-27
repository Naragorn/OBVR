// Checks the places' sign (ui/HolsterSignPainter.h): the text scale that fits,
// a name alone centred, a name with a hint on two lines, nothing outside the
// plate.

#include <cstdio>
#include <cstring>
#include <vector>

#include "ui/HolsterSignPainter.h"
#include "ui/MenuFont.h"

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

// The rows that carry the gold or the hint's colour: the text's lines.
void InkRows(const Canvas& c, SInt32& first, SInt32& last, UInt32& inkPixels) {
	first = -1;
	last = -1;
	inkPixels = 0;
	const render::Pixel plate = c.GetPixel(static_cast<SInt32>(c.Width()) / 2, 4 + 1);
	for (SInt32 y = 6; y < static_cast<SInt32>(c.Height()) - 6; ++y) {
		for (SInt32 x = 6; x < static_cast<SInt32>(c.Width()) - 6; ++x) {
			const render::Pixel p = c.GetPixel(x, y);
			if (p.a == 255 && std::memcmp(&p, &plate, sizeof(p)) != 0) {
				++inkPixels;
				if (first < 0) {
					first = y;
				}
				last = y;
			}
		}
	}
}

}  // namespace

int main() {
	std::printf("The text's scale\n");
	Check(SignTextScale("Iron Longsword", 368, 5) == 4, "a name too wide at 5: 4");
	Check(SignTextScale("Torch", 368, 5) == 5, "a short one: the largest");
	Check(SignTextScale("A name far, far too long to fit on the little sign at the hip", 368, 5) == 1,
	      "far too long: the smallest");
	Check(SignTextScale("x", 368, 0) == 1, "a nonsense largest: 1");

	std::printf("The sign\n");
	std::vector<render::Pixel> pixels(kHolsterSignWidth * kHolsterSignHeight);
	Canvas canvas(pixels.data(), kHolsterSignWidth, kHolsterSignHeight);
	HolsterSignView view;
	std::snprintf(view.item, sizeof(view.item), "%s", "Iron Longsword");
	PaintHolsterSign(canvas, view);
	SInt32 first = 0;
	SInt32 last = 0;
	UInt32 ink = 0;
	InkRows(canvas, first, last, ink);
	const SInt32 one = static_cast<SInt32>(kGlyphHeight) * 4;
	Check(ink > 0 && last - first + 1 <= one && first > 10, "a name alone: one line, about the middle");
	Check(canvas.GetPixel(1, 1).a != 0 && canvas.GetPixel(static_cast<SInt32>(kHolsterSignWidth) / 2, 40).a != 0,
	      "the plate and its edge drawn, not see-through");
	std::snprintf(view.hint, sizeof(view.hint), "%s", "Bow, shield, torch: other hand");
	PaintHolsterSign(canvas, view);
	InkRows(canvas, first, last, ink);
	Check(last - first + 1 > one, "a hint: a second line under the name");
	Check(first >= 6 && last < static_cast<SInt32>(kHolsterSignHeight) - 6, "both lines inside the plate");
	PaintHolsterSignFor(canvas, &view);
	UInt32 again = 0;
	InkRows(canvas, first, last, again);
	Check(again == ink, "the overlay's painter paints the same");
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
