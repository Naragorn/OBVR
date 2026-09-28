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

constexpr SInt32 kBoxWidth = kQuickMenuBoxWidth;
constexpr SInt32 kBoxHeight = kQuickMenuBoxHeight;
constexpr SInt32 kPad = 8;
constexpr SInt32 kIconTop = 6;
constexpr SInt32 kIcon = static_cast<SInt32>(kQuickMenuIconSide);

void FillDot(Canvas& canvas, SInt32 cx, SInt32 cy, SInt32 radius, render::Pixel colour) {
	for (SInt32 dy = -radius; dy <= radius; ++dy) {
		for (SInt32 dx = -radius; dx <= radius; ++dx) {
			if (dx * dx + dy * dy <= radius * radius) {
				canvas.SetPixel(cx + dx, cy + dy, colour);
			}
		}
	}
}
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
	if (view.pageCount > 1) {
		const SInt32 first = centre - (view.pageCount - 1) * kQuickMenuDotSpacing / 2;
		for (int p = 0; p < view.pageCount; ++p) {
			const SInt32 x = first + p * kQuickMenuDotSpacing;
			const SInt32 y = centre - kQuickMenuDotsAbove;
			// Round, so they do not read as the diamond in the middle: the page
			// shown filled gold, the others a gold ring on the dark panel.
			FillDot(canvas, x, y, kQuickMenuDotRadius, kGold);
			if (p != view.page) {
				FillDot(canvas, x, y, kQuickMenuDotRadius - 3, kPanel);
			}
		}
	}
	if (view.assigning) {
		const SInt32 width = static_cast<SInt32>(TextWidth(kQuickMenuAssignLabel)) * kNameScale;
		canvas.DrawText(centre - width / 2, centre + 24, kQuickMenuAssignLabel, kNameScale, kGold);
	}
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
		// Setting, an empty slot is as good a choice as a filled one: gold edged.
		canvas.DrawFrame(x, y, kBoxWidth, kBoxHeight, lit ? 3 : 2,
		                 filled || lit || view.assigning ? kGold : kEdgeEmpty);
		const char number[2] = {static_cast<char>('1' + slot), '\0'};
		const render::Pixel ink = lit ? kTextDark : (filled ? kText : kTextDim);
		canvas.DrawText(x + kPad, y + kPad, number, kNumberScale, ink);
		if (filled) {
			canvas.BlendImage(x + (kBoxWidth - kIcon) / 2, y + kIconTop, view.icons[slot], kQuickMenuIconSide,
			                  kQuickMenuIconSide);
		}
		char first[40];
		char second[40];
		SplitName(filled ? view.names[slot] : "-", perLine, first, second);
		// The name under the icon's place, each line centred; with no icon the
		// place stays empty, so every slot reads the same way.
		const SInt32 lineY = y + kIconTop + kIcon + 6;
		const char* const lines[2] = {first, second};
		for (int line = 0; line < 2; ++line) {
			const SInt32 width = static_cast<SInt32>(TextWidth(lines[line])) * kNameScale;
			canvas.DrawText(x + (kBoxWidth - width) / 2,
			                lineY + line * static_cast<SInt32>(kLineAdvance) * kNameScale, lines[line],
			                kNameScale, ink);
		}
	}
}

void PaintQuickMenuFor(Canvas& canvas, const void* view) {
	PaintQuickMenu(canvas, *static_cast<const QuickMenuView*>(view));
}

}  // namespace obvr::ui
