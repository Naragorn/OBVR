// Checks the boxes on the tablet's page (vr/TabletBoxes.h; docs/inventory-
// tablet-spec.md section 23): when they come onto the page, which boxes the
// game moves onto its middle and by how much, what of a moved box is put
// back, a box told from an info, and a moved box on the page held by either
// hand.

#include <cstdio>

#include "vr/TabletBoxes.h"

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

bool Near(float a, float b, float tolerance = 0.01f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
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

// The vanilla page as read in the game, in HUD units (tablet-tiles.txt,
// 20261117-190432): 745 by 792.
const UiRect kPaper = Rect(27.0f, 131.0f, 772.0f, 923.0f);
// The screen the game lays its menus out on, in HUD units: 960 high, as wide
// as the layer's 4036 by 2270 pixels (Nadi's run of 2026-11-26).
const float kScreenWide = 960.0f * 4036.0f / 2270.0f;
const float kScreenHigh = 960.0f;

void TestWhen() {
	std::printf("When the boxes come onto the page:\n");
	Check(BoxesOnPage(true, TabletFigure::Tab), "drawn into the eyes, the figure behind a tab");
	Check(BoxesOnPage(true, TabletFigure::Off), "drawn into the eyes, no figure");
	Check(!BoxesOnPage(true, TabletFigure::Beside), "the figure on its own page beside the page: as the game has them");
	Check(!BoxesOnPage(false, TabletFigure::Tab) && !BoxesOnPage(false, TabletFigure::Off),
	      "the overlay (one cut of the layer): as the game has them");

	Check(TabletBoxMoves(game::kMenuIdMessage) && TabletBoxMoves(game::kMenuIdQuantity) &&
	          TabletBoxMoves(game::kMenuIdGeneric) && TabletBoxMoves(game::kMenuIdBook),
	      "a message, a quantity, a Generic box and a book are moved onto the page's middle");
	Check(!TabletBoxMoves(game::kMenuIdMagicPopup), "the item info is no box: it stays beside the page");
	Check(!TabletBoxMoves(game::kMenuIdInventory) && !TabletBoxMoves(game::kMenuIdStats) &&
	          !TabletBoxMoves(game::kMenuIdBigFour) && !TabletBoxMoves(game::kMenuIdHudMain) &&
	          !TabletBoxMoves(game::kMenuIdNone),
	      "the pages, their entry, the HUD, none: not moved");
}

void TestShift() {
	std::printf("A box moved onto the page's middle:\n");
	// The message box of the left-handed run: the screen's middle, right of the page.
	const UiRect message = Rect(459.0f, 362.0f, 1247.0f, 598.0f);
	BoxShift s = ShiftOntoPage(message, kPaper, kScreenWide, kScreenHigh);
	Check(s.move && Near(s.dx, 399.5f - 853.0f) && Near(s.dy, 527.0f - 480.0f),
	      "the screen's middle to the page's: left by 453.5, down by 47 units");
	const UiRect centred = Rect(message.left + s.dx, message.top + s.dy, message.right + s.dx, message.bottom + s.dy);
	Check(!ShiftOntoPage(centred, kPaper, kScreenWide, kScreenHigh).move, "there already: no shift");
	const UiRect nearly = Rect(centred.left + 0.4f, centred.top - 0.4f, centred.right + 0.4f, centred.bottom - 0.4f);
	s = ShiftOntoPage(nearly, kPaper, kScreenWide, kScreenHigh);
	Check(!s.move && s.dx == 0.0f && s.dy == 0.0f, "within half a unit: no shift (no write a frame for a rounding)");
	Check(!ShiftOntoPage(UiRect{}, kPaper, kScreenWide, kScreenHigh).move &&
	          !ShiftOntoPage(message, UiRect{}, kScreenWide, kScreenHigh).move,
	      "no box or no page: none");

	// What lies past the screen the game draws nowhere (Nadi, Windows
	// 2026-11-26: the tutorial's Generic box, 888 units wide, centred on the
	// page's middle lost its left 45 units, and read cut at the screen's edge
	// it was shifted further, to 89 units past it; docs/inventory-tablet-spec.md 24.3).
	std::printf("A box kept on the screen:\n");
	const UiRect generic = Rect(409.0f, 249.0f, 1297.0f, 711.0f);
	s = ShiftOntoPage(generic, kPaper, kScreenWide, kScreenHigh);
	Check(s.move && Near(s.dx, -409.0f) && Near(s.dy, 47.0f),
	      "the run's Generic box, 888 units: its left edge at the screen's, not 45 units past it; centred up and down");
	const UiRect atEdge = Rect(0.0f, 296.0f, 888.0f, 758.0f);
	Check(!ShiftOntoPage(atEdge, kPaper, kScreenWide, kScreenHigh).move,
	      "there: it stays - not pushed on past the edge");
	const UiRect cut = Rect(0.0f, 296.0f, 844.0f, 758.0f);  // the old shift's box as read, cut at the edge
	Check(!ShiftOntoPage(cut, kPaper, kScreenWide, kScreenHigh).move,
	      "a box read cut at the screen's edge is not shifted further off it");
	const UiRect book = Rect(277.0f, 52.0f, 1429.0f, 908.0f);  // 1152 by 856, the screen's middle
	s = ShiftOntoPage(book, kPaper, kScreenWide, kScreenHigh);
	Check(s.move && Near(book.left + s.dx, 0.0f) && Near(0.5f * (book.top + book.bottom) + s.dy, 527.0f),
	      "a book larger than the page: as near its middle as the screen lets it - its left at the screen's");
	const UiRect tall = Rect(459.0f, 30.0f, 1247.0f, 930.0f);  // 900 high
	s = ShiftOntoPage(tall, kPaper, kScreenWide, kScreenHigh);
	Check(s.move && Near(tall.bottom + s.dy, kScreenHigh) && Near(0.5f * (tall.left + tall.right) + s.dx, 399.5f),
	      "a box too tall for the page's middle: its foot at the screen's, centred across");
	const UiRect rightPaper = Rect(1100.0f, 131.0f, 1650.0f, 923.0f);
	const UiRect wide = Rect(100.0f, 300.0f, 900.0f, 600.0f);  // 800 wide, for a page near the right edge
	s = ShiftOntoPage(wide, rightPaper, kScreenWide, kScreenHigh);
	Check(s.move && Near(wide.right + s.dx, kScreenWide), "a page near the right edge: the box's right at the screen's");
	const UiRect huge = Rect(0.0f, 0.0f, kScreenWide, kScreenHigh);
	Check(!ShiftOntoPage(huge, kPaper, kScreenWide, kScreenHigh).move,
	      "a box as large as the screen: where it is - the near edges win");
	Check(Near(KeptOnScreen(-10.0f, 2000.0f, 400.0f, kScreenWide), 10.0f),
	      "wider than the screen and past both edges: its left at the screen's");

	std::printf("What of a moved box is put back:\n");
	Check(KeptOriginal(false, 0.0f, 0.0f, 853.0f) == 853.0f, "the first move: the game's own value is the one to put back");
	Check(KeptOriginal(true, 853.0f, 399.5f, 399.5f) == 853.0f, "moved again while ours stands: still the game's first");
	Check(KeptOriginal(true, 853.0f, 399.5f, 860.0f) == 860.0f,
	      "the game set its own meanwhile (a formula read anew): that one is the game's now");
	Check(PutBack(399.5f, 399.5f), "off the page with our value still there: put back");
	Check(!PutBack(860.0f, 399.5f), "the game set its own meanwhile: left as the game has it");
}

void TestBoxOver() {
	std::printf("A box over the page, not an info:\n");
	TabletRects r;
	r.valid = true;
	Check(!BoxOverPage(r), "no popup: none");
	JoinPopup(r, Rect(772.0f, 300.0f, 1150.0f, 360.0f), true);
	Check(!BoxOverPage(r), "the item info alone: no box - the stick turns the page");
	JoinPopup(r, Rect(5.5f, 409.0f, 793.5f, 645.0f));
	Check(BoxOverPage(r), "and a message: a box");
	TabletRects many;
	many.valid = true;
	for (UInt32 i = 0; i < kTabletPopupsMost + 1; ++i) {
		JoinPopup(many, Rect(800.0f, 10.0f, 900.0f, 20.0f), true);
	}
	Check(BoxOverPage(many), "more popups than kept: counted as a box");
}

// The tablet's face as the hand holds it, metres along its right from the
// hand, for a column of the face.
float AcrossFromHand(const TabletPlace& p, const TabletFrame& f, const NiPoint3& hand, float x) {
	const NiPoint3 right{p.transform.m[0][0], p.transform.m[1][0], p.transform.m[2][0]};
	const NiPoint3 centre{p.transform.m[0][3], p.transform.m[1][3], p.transform.m[2][3]};
	const NiPoint3 from = centre - hand;
	const float along = from.x * right.x + from.y * right.y + from.z * right.z;
	return along + ((x - f.crop.left) / (f.crop.right - f.crop.left) - 0.5f) * p.width;
}

// The page with a message box joined where the game draws it.
TabletFrame FrameWithBox(const UiRect& box) {
	TabletFrame f;
	f.up = true;
	f.paper = kPaper;
	f.outline = kPaper;
	f.popups[0] = box;
	f.popupCount = 1;
	f.pageCrop = UnionRect(kPaper, box);
	f.crop = f.pageCrop;
	f.popupUp = true;
	return f;
}

void TestHeld() {
	std::printf("Held with a box on the page:\n");
	TabletSettings s;
	s.widthMetres = 0.745f;  // a millimetre a unit
	const NiPoint3 hand{0.02f, -0.01f, 0.10f};
	// Left-handed: the physical right hand holds the page by its right edge
	// (Nadi: "das löst auch das linkshänder problem" - the box hung over the
	// forearm, section 21).
	const UiRect message = Rect(460.0f, 347.0f, 1248.0f, 614.0f);
	const BoxShift shift = ShiftOntoPage(message, kPaper, kScreenWide, kScreenHigh);
	const TabletFrame game = FrameWithBox(message);
	TabletPlace p = PlaceTablet(game, s, false, hand, TabletHold::Side);
	Check(Near(AcrossFromHand(p, game, hand, 772.0f), 0.0f, 0.0005f) && Near(AcrossFromHand(p, game, hand, 1248.0f), 0.476f, 0.001f),
	      "(where the game draws it: the box reaches 0.48 m out past the right hand, over the forearm)");
	const TabletFrame moved = FrameWithBox(
		Rect(message.left + shift.dx, message.top + shift.dy, message.right + shift.dx, message.bottom + shift.dy));
	p = PlaceTablet(moved, s, false, hand, TabletHold::Side);
	Check(Near(AcrossFromHand(p, moved, hand, 772.0f), 0.0f, 0.0005f), "moved: the page's right edge stays in the hand");
	Check(Near(AcrossFromHand(p, moved, hand, moved.crop.right), 0.0215f, 0.001f),
	      "and the box, 788 units for the page's 745, reaches 2 cm past it, as far as on the left");
	Check(Near(AcrossFromHand(p, moved, hand, 27.0f) - AcrossFromHand(p, moved, hand, moved.crop.left), 0.0215f, 0.001f),
	      "(evenly on both sides)");
	p = PlaceTablet(moved, s, true, hand, TabletHold::Side);
	Check(Near(AcrossFromHand(p, moved, hand, 27.0f), 0.0f, 0.0005f), "right-handed: the left hand keeps the page's left edge");
}

}  // namespace

int main() {
	TestWhen();
	TestShift();
	TestBoxOver();
	TestHeld();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
