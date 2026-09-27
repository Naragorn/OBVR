#include "ui/QuickMenuPainter.h"

#include <cmath>
#include <cstring>

#include "ui/MenuFont.h"

namespace obvr::ui {
namespace {

// A colour given as red, green, blue, alpha, stored the way the A8R8G8B8
// texture reads it (blue first).
constexpr render::Pixel Colour(UInt8 r, UInt8 g, UInt8 b, UInt8 a) {
	return SwapRedAndBlue(render::Pixel{r, g, b, a});
}

// Oblivion's parchment gold, the teleport ring's colour (222,190,140), on the
// dark brown of its menus.
constexpr render::Pixel kGold = Colour(222, 190, 140, 255);
constexpr render::Pixel kPanel = Colour(40, 30, 20, 215);
constexpr render::Pixel kPanelEmpty = Colour(40, 30, 20, 120);
constexpr render::Pixel kEdgeEmpty = Colour(120, 110, 95, 200);
constexpr render::Pixel kText = Colour(235, 222, 196, 255);
constexpr render::Pixel kTextDark = Colour(30, 22, 14, 255);
constexpr render::Pixel kTextDim = Colour(140, 130, 115, 255);

constexpr SInt32 kBoxWidth = 160;
constexpr SInt32 kBoxHeight = 80;
constexpr SInt32 kPad = 8;
constexpr SInt32 kNameScale = 2;
constexpr SInt32 kNumberScale = 3;

}  // namespace

void SplitName(const char* name, UInt32 perLine, char (&first)[40], char (&second)[40]) {
	first[0] = '\0';
	second[0] = '\0';
	if (name == nullptr || perLine == 0) {
		return;
	}
	if (perLine > 39) {
		perLine = 39;
	}
	const UInt32 length = static_cast<UInt32>(std::strlen(name));
	if (length <= perLine) {
		std::memcpy(first, name, length + 1);
		return;
	}
	// Break at the last space that keeps the first line within perLine.
	UInt32 cut = perLine;
	for (UInt32 i = perLine; i > 0; --i) {
		if (name[i] == ' ') {
			cut = i;
			break;
		}
	}
	std::memcpy(first, name, cut);
	first[cut] = '\0';
	const char* rest = name + cut;
	while (*rest == ' ') {
		++rest;
	}
	const UInt32 restLength = static_cast<UInt32>(std::strlen(rest));
	if (restLength <= perLine) {
		std::memcpy(second, rest, restLength + 1);
		return;
	}
	std::memcpy(second, rest, perLine - 1);
	second[perLine - 1] = '.';
	second[perLine] = '\0';
}

void PaintQuickMenu(Canvas& canvas, const QuickMenuView& view) {
	canvas.Fill(render::Pixel{0, 0, 0, 0});
	const SInt32 centre = static_cast<SInt32>(kQuickMenuCanvas) / 2;
	canvas.FillDiamond(centre, centre, 10, kGold);
	const UInt32 perLine =
		static_cast<UInt32>((kBoxWidth - 2 * kPad) / (static_cast<SInt32>(kGlyphAdvance) * kNameScale));
	for (int slot = 0; slot < 8; ++slot) {
		const float angle = static_cast<float>(slot) * 6.28318531f / 8.0f;
		const SInt32 cx = centre + static_cast<SInt32>(std::lround(kQuickMenuRingPixels * std::sin(angle)));
		const SInt32 cy = centre - static_cast<SInt32>(std::lround(kQuickMenuRingPixels * std::cos(angle)));
		const SInt32 x = cx - kBoxWidth / 2;
		const SInt32 y = cy - kBoxHeight / 2;
		const bool lit = view.highlighted == slot;
		const bool filled = view.filled[slot];
		canvas.FillRect(x, y, kBoxWidth, kBoxHeight, lit ? kGold : (filled ? kPanel : kPanelEmpty));
		canvas.DrawFrame(x, y, kBoxWidth, kBoxHeight, lit ? 3 : 2,
		                 filled || lit ? kGold : kEdgeEmpty);
		const char number[2] = {static_cast<char>('1' + slot), '\0'};
		const render::Pixel ink = lit ? kTextDark : (filled ? kText : kTextDim);
		canvas.DrawText(x + kPad, y + kPad, number, kNumberScale, ink);
		char first[40];
		char second[40];
		SplitName(filled ? view.names[slot] : "-", perLine, first, second);
		const SInt32 lineY = y + kPad + static_cast<SInt32>(kGlyphHeight) * kNumberScale + 6;
		canvas.DrawText(x + kPad, lineY, first, kNameScale, ink);
		canvas.DrawText(x + kPad, lineY + static_cast<SInt32>(kLineAdvance) * kNameScale, second,
		                kNameScale, ink);
	}
}

void PaintQuickMenuFor(Canvas& canvas, const void* view) {
	PaintQuickMenu(canvas, *static_cast<const QuickMenuView*>(view));
}

}  // namespace obvr::ui
