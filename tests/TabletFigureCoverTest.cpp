// Checks whether the page's own pictures lie over the figure's place
// (vr/TabletFigureCover.h).
//
// Nadi, 2026-11-28: with the figure behind its tab and the map page chosen,
// the map showed in the figure's place. The numbers: every vanilla page's
// paper at 27,131, 745x792 HUD units, the map's extension 567x613 beside it
// (tablet-tiles.txt, 2026-11-17; its top is taken at the paper's [G]); the
// figure 0.545..0.913 of the screen across (Nadi's run of 2026-11-27, a layer
// of 4036x3376 pixels: 1147.7 units wide) - its height is not logged, taken
// as 0.2..0.95 of the screen [G].

#include <cstdio>

#include "vr/TabletFigureCover.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

UiRect Rect(float left, float top, float right, float bottom) {
	UiRect r;
	r.left = left;
	r.top = top;
	r.right = right;
	r.bottom = bottom;
	r.valid = true;
	return r;
}

constexpr float kScreenWide = 960.0f * 4036.0f / 3376.0f;  // Nadi's layer, HUD units
const UiRect kPaper = Rect(27.0f, 131.0f, 772.0f, 923.0f);
// The map's outline: the paper and its extension beside it, cut to Nadi's screen (vr::OnScreen).
const UiRect kMapOutline = Rect(27.0f, 131.0f, kScreenWide, 923.0f);
const UiRect kFigure = Rect(0.545f * kScreenWide, 0.2f * 960.0f, 0.913f * kScreenWide, 0.95f * 960.0f);

void TestPages() {
	std::printf("The pages\n");
	Check(!PageOverFigure(kPaper, kPaper, kFigure),
	      "the inventory (no picture past its paper): the figure free, though its cut reaches onto the paper");
	Check(PageOverFigure(kPaper, kMapOutline, kFigure), "the map: its extension over the figure's place");
	const float wide = 960.0f * 16.0f / 9.0f;
	const UiRect figureWide = Rect(0.545f * wide, 0.2f * 960.0f, 0.913f * wide, 0.95f * 960.0f);
	const UiRect mapWide = Rect(27.0f, 131.0f, 772.0f + 567.0f, 923.0f);
	Check(PageOverFigure(kPaper, mapWide, figureWide), "the map on a 16:9 screen: over the figure too");
	Check(!PageOverFigure(kPaper, kPaper, figureWide), "the inventory on a 16:9 screen: free");
	const UiRect farFigure = Rect(1400.0f, 192.0f, 1700.0f, 912.0f);
	Check(!PageOverFigure(kPaper, mapWide, farFigure), "a figure right of the map's extension: free");
}

void TestShare() {
	std::printf("How much counts\n");
	const UiRect figure = Rect(800.0f, 200.0f, 1000.0f, 700.0f);  // 200 wide
	Check(!PageOverFigure(kPaper, Rect(27.0f, 131.0f, 803.0f, 923.0f), figure),
	      "a page's picture a few units past its paper: no cover");
	Check(PageOverFigure(kPaper, Rect(27.0f, 131.0f, 821.0f, 923.0f), figure),
	      "over a tenth of the figure under the page: covered");
	Check(!PageOverFigure(kPaper, Rect(27.0f, 131.0f, 818.0f, 923.0f), figure), "under a tenth: free");
	// Only the outline's part outside the paper counts, however far the
	// figure reaches onto the paper.
	const UiRect onPaper = Rect(500.0f, 200.0f, 810.0f, 700.0f);
	Check(!PageOverFigure(kPaper, kPaper, onPaper), "a figure on the paper itself: the paper is no cover");
}

void TestSides() {
	std::printf("Every side of the paper\n");
	const UiRect paper = Rect(400.0f, 300.0f, 800.0f, 700.0f);
	Check(PageOverFigure(paper, Rect(100.0f, 300.0f, 800.0f, 700.0f), Rect(150.0f, 350.0f, 350.0f, 650.0f)),
	      "an extension on the left over the figure");
	Check(PageOverFigure(paper, Rect(400.0f, 50.0f, 800.0f, 700.0f), Rect(450.0f, 60.0f, 750.0f, 280.0f)),
	      "an extension above over the figure");
	Check(PageOverFigure(paper, Rect(400.0f, 300.0f, 800.0f, 950.0f), Rect(450.0f, 720.0f, 750.0f, 940.0f)),
	      "an extension below over the figure");
	Check(PageOverFigure(paper, Rect(400.0f, 300.0f, 1100.0f, 900.0f), Rect(820.0f, 720.0f, 1000.0f, 880.0f)),
	      "the outline's corner below and right, past the paper's foot");
	Check(!PageOverFigure(paper, Rect(100.0f, 300.0f, 800.0f, 700.0f), Rect(850.0f, 350.0f, 1000.0f, 650.0f)),
	      "an extension on the left, the figure on the right: free");
}

void TestRefusals() {
	std::printf("Nothing to tell\n");
	const UiRect none;
	Check(!PageOverFigure(none, kMapOutline, kFigure), "no paper");
	Check(!PageOverFigure(kPaper, none, kFigure), "no outline");
	Check(!PageOverFigure(kPaper, kMapOutline, none), "no figure (not read)");
	Check(!PageOverFigure(kPaper, kMapOutline, Rect(900.0f, 300.0f, 900.0f, 600.0f)), "a figure with no width");
	Check(!PageOverFigure(kPaper, kMapOutline, Rect(900.0f, 600.0f, 1000.0f, 300.0f)), "a figure upside down");
	Check(SharedArea(Rect(0.0f, 0.0f, 10.0f, 10.0f), Rect(10.0f, 0.0f, 20.0f, 10.0f)) == 0.0f,
	      "rectangles that only touch share nothing");
	Check(SharedArea(Rect(0.0f, 0.0f, 10.0f, 10.0f), Rect(5.0f, 5.0f, 20.0f, 20.0f)) == 25.0f,
	      "overlapping rectangles share their common part");
}

}  // namespace

int main() {
	TestPages();
	TestShare();
	TestSides();
	TestRefusals();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
