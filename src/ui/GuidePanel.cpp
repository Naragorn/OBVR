#include "ui/GuidePanel.h"

#include "ui/MenuFont.h"

namespace obvr::ui {
namespace {

constexpr render::Pixel Colour(UInt8 r, UInt8 g, UInt8 b, UInt8 a) {
	return SwapRedAndBlue(render::Pixel{r, g, b, a});
}

constexpr render::Pixel kGold = Colour(222, 190, 140, 255);
constexpr render::Pixel kPanel = Colour(40, 30, 20, 225);
constexpr render::Pixel kText = Colour(235, 222, 196, 255);

}  // namespace

void PaintGuidePanel(Canvas& canvas, const GuidePanelText& text) {
	canvas.Fill(kPanel);
	canvas.DrawFrame(0, 0, static_cast<SInt32>(canvas.Width()),
	                 static_cast<SInt32>(canvas.Height()), 3, kGold);
	canvas.DrawText(20, 18, text.title, 3, kGold);
	SInt32 y = 18 + static_cast<SInt32>(kLineAdvance) * 3 + 8;
	for (const char* line : text.lines) {
		if (line != nullptr && line[0] != '\0') {
			canvas.DrawText(20, y, line, 2, kText);
		}
		y += static_cast<SInt32>(kLineAdvance) * 2 + 4;
	}
}

void PaintGuidePanelFor(Canvas& canvas, const void* text) {
	PaintGuidePanel(canvas, *static_cast<const GuidePanelText*>(text));
}

}  // namespace obvr::ui
