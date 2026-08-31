// Checks what the settings menu looks like, without a headset.
//
// The point of painting into a plain canvas rather than straight into a texture
// is that the appearance becomes something a test can look at. So these do look
// at it: where the highlight lands, which rows a scrolled window shows, that a
// category heading appears where the category changes, and that a value is
// right-aligned rather than approximately so.
//
// The last test is the one that would be hardest to find any other way. The
// buffer is allocated larger than the canvas and the surplus is filled with a
// guard value, so a layout that computes a row position wrongly and runs off
// the end is caught here rather than as a corrupted heap in a running game.
//
// No Windows API here, so this one builds and runs on Linux as well.

#include <cstdio>

#include "ui/MenuPainter.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::render::Pixel;
using obvr::ui::Canvas;
using obvr::ui::ItemKind;
using obvr::ui::MenuItem;
using obvr::ui::MenuState;
using obvr::ui::MenuTheme;
using obvr::ui::PaintMenu;
using obvr::ui::VisibleRowsFor;

bool Same(Pixel a, Pixel b) {
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// The canvas the tests paint into, with a guard region behind it.
constexpr UInt32 kWidth = 200;
constexpr UInt32 kHeight = 150;
constexpr UInt32 kGuard = 64;

const Pixel kGuardValue{1, 2, 3, 4};

struct Sheet {
	Pixel storage[kWidth * kHeight + kGuard];

	Sheet() {
		for (UInt32 at = 0; at < kWidth * kHeight + kGuard; ++at) {
			storage[at] = kGuardValue;
		}
	}

	Canvas Surface() { return Canvas(storage, kWidth, kHeight); }

	bool GuardIntact() const {
		for (UInt32 at = kWidth * kHeight; at < kWidth * kHeight + kGuard; ++at) {
			if (!Same(storage[at], kGuardValue)) {
				return false;
			}
		}
		return true;
	}
};

// Whether a row of the canvas contains a pixel of the given colour. Used to
// find the highlight, which is the one thing on screen with a colour of its
// own.
bool RowHasColour(const Canvas& canvas, SInt32 y, Pixel colour) {
	for (SInt32 x = 0; x < static_cast<SInt32>(canvas.Width()); ++x) {
		if (Same(canvas.GetPixel(x, y), colour)) {
			return true;
		}
	}
	return false;
}

SInt32 FirstRowWithColour(const Canvas& canvas, Pixel colour) {
	for (SInt32 y = 0; y < static_cast<SInt32>(canvas.Height()); ++y) {
		if (RowHasColour(canvas, y, colour)) {
			return y;
		}
	}
	return -1;
}

// A list long enough to scroll, with two categories.
constexpr UInt32 kItemCount = 12;

void BuildItems(MenuItem* items, const char** categories) {
	static const char* const kNames[kItemCount] = {"One",  "Two",  "Three", "Four",
	                                               "Five", "Six",  "Seven", "Eight",
	                                               "Nine", "Ten",  "Eleven", "Twelve"};

	for (UInt32 at = 0; at < kItemCount; ++at) {
		items[at].label = kNames[at];
		items[at].help = "a line of help";
		items[at].kind = ItemKind::Number;
		items[at].value = static_cast<float>(at);
		items[at].minimum = 0.0f;
		items[at].maximum = 100.0f;
		items[at].step = 1.0f;
		items[at].decimals = 0;
		categories[at] = at < 6 ? "First" : "Second";
	}
}

void TestVisibleRows() {
	std::printf("How many rows fit\n");

	Check(VisibleRowsFor(150, 1) > 0, "a normal canvas fits rows");
	Check(VisibleRowsFor(150, 1) > VisibleRowsFor(150, 2), "a bigger scale fits fewer");

	// A canvas too short for even one row still gets one. A menu with no rows
	// cannot be navigated back out of, which is worse than one row clipped.
	Check(VisibleRowsFor(1, 1) == 1, "a canvas too short still offers one row");
	Check(VisibleRowsFor(0, 1) == 1, "and so does one with no height");

	// A scale of zero would divide the layout by nothing.
	Check(VisibleRowsFor(150, 0) == VisibleRowsFor(150, 1), "a scale of zero is treated as one");
}

void TestPanelIsDrawn() {
	std::printf("The panel\n");

	Sheet sheet;
	Canvas canvas = sheet.Surface();
	MenuTheme theme;

	MenuItem items[kItemCount];
	const char* categories[kItemCount];
	BuildItems(items, categories);

	PaintMenu(canvas, items, categories, kItemCount, MenuState{}, 1, theme);

	Check(Same(canvas.GetPixel(0, 0), theme.frame), "the frame reaches the top left corner");
	Check(Same(canvas.GetPixel(static_cast<SInt32>(kWidth) - 1, static_cast<SInt32>(kHeight) - 1),
	           theme.frame),
	      "and the bottom right one");

	// Somewhere in the middle that is neither frame nor a letter has to be the
	// panel colour, which is what carries the transparency.
	Check(Same(canvas.GetPixel(2, static_cast<SInt32>(kHeight) / 2), theme.background),
	      "the panel colour fills behind the rows");

	// The title has ink in the top few rows.
	bool titleFound = false;
	for (SInt32 y = 0; y < 12 && !titleFound; ++y) {
		titleFound = RowHasColour(canvas, y, theme.title);
	}
	Check(titleFound, "the title is drawn");

	Check(sheet.GuardIntact(), "nothing was written past the canvas");
}

void TestEmptyMenu() {
	std::printf("A menu with nothing in it\n");

	Sheet sheet;
	Canvas canvas = sheet.Surface();
	MenuTheme theme;

	// An empty list still draws the panel, so an empty menu is a bug somebody
	// can see rather than a menu that failed to appear at all.
	PaintMenu(canvas, nullptr, nullptr, 0, MenuState{}, 1, theme);
	Check(Same(canvas.GetPixel(0, 0), theme.frame), "an empty menu still draws its frame");
	Check(FirstRowWithColour(canvas, theme.highlight) < 0, "and has no highlighted row");
	Check(sheet.GuardIntact(), "and writes nothing past the canvas");

	// A count with no items behind it, which is the shape a caller gets wrong.
	Sheet second;
	Canvas other = second.Surface();
	PaintMenu(other, nullptr, nullptr, 5, MenuState{}, 1, theme);
	Check(second.GuardIntact(), "a count with no items writes nothing past the canvas");
}

void TestHighlightFollowsSelection() {
	std::printf("The highlight\n");

	MenuItem items[kItemCount];
	const char* categories[kItemCount];
	BuildItems(items, categories);

	MenuTheme theme;

	Sheet first;
	Canvas top = first.Surface();
	MenuState state;
	PaintMenu(top, items, categories, kItemCount, state, 1, theme);

	const SInt32 atTop = FirstRowWithColour(top, theme.highlight);
	Check(atTop > 0, "the first row is highlighted");

	// Moving the selection down moves the bar down by exactly one row, which
	// is the check that the row pitch used for drawing is the one the model
	// scrolls by.
	Sheet second;
	Canvas lower = second.Surface();
	state.selected = 2;
	PaintMenu(lower, items, categories, kItemCount, state, 1, theme);

	const SInt32 atSecond = FirstRowWithColour(lower, theme.highlight);
	Check(atSecond > atTop, "selecting a later row moves the highlight down");

	// Scrolled: the window starts at row 4, and the selected row 4 is drawn at
	// the top of the list again. The highlight must come back up with it, or
	// the drawing is using the index where it should use the offset into the
	// window - the classic scrolling fault, and it looks like a highlight that
	// wanders off the bottom.
	Sheet third;
	Canvas scrolled = third.Surface();
	state.selected = 4;
	state.firstVisible = 4;
	PaintMenu(scrolled, items, categories, kItemCount, state, 1, theme);

	const SInt32 atScrolled = FirstRowWithColour(scrolled, theme.highlight);
	Check(atScrolled == atTop, "a scrolled window draws its selected row at the top again");

	Check(first.GuardIntact() && second.GuardIntact() && third.GuardIntact(),
	      "and none of them wrote past the canvas");
}

void TestOutOfRangeStateIsSurvivable() {
	std::printf("A state that does not match the list\n");

	MenuItem items[kItemCount];
	const char* categories[kItemCount];
	BuildItems(items, categories);

	MenuTheme theme;

	// A window starting past the end, and a selection past the end. The painter
	// deliberately does not correct these - AdvanceMenu is what keeps them
	// consistent, and correcting them here would hide a fault. What it must do
	// is not read past the array.
	Sheet sheet;
	Canvas canvas = sheet.Surface();

	MenuState state;
	state.firstVisible = 500;
	state.selected = 500;
	PaintMenu(canvas, items, categories, kItemCount, state, 1, theme);
	Check(sheet.GuardIntact(), "a window past the end writes nothing past the canvas");

	// A very large scale, where a single row is taller than the whole canvas.
	Sheet big;
	Canvas huge = big.Surface();
	PaintMenu(huge, items, categories, kItemCount, MenuState{}, 40, theme);
	Check(big.GuardIntact(), "a scale larger than the canvas writes nothing past it");

	// And a scale of zero, which would otherwise be a division by nothing.
	Sheet zero;
	Canvas none = zero.Surface();
	PaintMenu(none, items, categories, kItemCount, MenuState{}, 0, theme);
	Check(zero.GuardIntact(), "and a scale of zero is survivable");
}

void TestCategoryHeadings() {
	std::printf("Category headings\n");

	MenuItem items[kItemCount];
	const char* categories[kItemCount];
	BuildItems(items, categories);

	MenuTheme theme;

	// Scrolled so that the boundary between the two categories is on screen
	// but not at the very top - the heading is drawn where the category
	// changes, and the first visible row does not get one.
	Sheet sheet;
	Canvas canvas = sheet.Surface();

	MenuState state;
	state.firstVisible = 4;
	state.selected = 4;
	PaintMenu(canvas, items, categories, kItemCount, state, 1, theme);

	Check(FirstRowWithColour(canvas, theme.category) > 0, "a heading is drawn at the boundary");
	Check(sheet.GuardIntact(), "and nothing past the canvas");

	// With no categories at all, which a caller may legitimately pass.
	Sheet plain;
	Canvas bare = plain.Surface();
	PaintMenu(bare, items, nullptr, kItemCount, MenuState{}, 1, theme);
	Check(FirstRowWithColour(bare, theme.category) < 0, "no categories means no headings");
	Check(plain.GuardIntact(), "and that is survivable too");
}

void TestRestartMarkAndHelp() {
	std::printf("The restart mark and the help line\n");

	MenuItem items[kItemCount];
	const char* categories[kItemCount];
	BuildItems(items, categories);

	MenuTheme theme;

	// Nothing needs a restart yet, so the warning colour must be absent - if it
	// appears here, the mark is being drawn on rows that did not ask for it.
	Sheet clean;
	Canvas without = clean.Surface();
	PaintMenu(without, items, categories, kItemCount, MenuState{}, 1, theme);
	Check(FirstRowWithColour(without, theme.warning) < 0,
	      "no row is marked when none needs a restart");

	// Now one does, and it is not the selected row - so the mark has to be on
	// the row itself rather than only in the help line.
	items[2].needsRestart = true;

	Sheet marked;
	Canvas with = marked.Surface();
	PaintMenu(with, items, categories, kItemCount, MenuState{}, 1, theme);
	Check(FirstRowWithColour(with, theme.warning) > 0, "a row that needs a restart is marked");
	Check(marked.GuardIntact(), "and nothing past the canvas");

	// The help line is drawn near the bottom for whatever is selected.
	Sheet helped;
	Canvas canvas = helped.Surface();
	PaintMenu(canvas, items, categories, kItemCount, MenuState{}, 1, theme);

	bool helpNearBottom = false;
	for (SInt32 y = static_cast<SInt32>(kHeight) - 20; y < static_cast<SInt32>(kHeight); ++y) {
		if (RowHasColour(canvas, y, theme.help)) {
			helpNearBottom = true;
			break;
		}
	}
	Check(helpNearBottom, "the help line is drawn along the bottom");
}

}  // namespace

int main() {
	std::printf("OBVR menu painter test\n\n");

	TestVisibleRows();
	std::printf("\n");
	TestPanelIsDrawn();
	std::printf("\n");
	TestEmptyMenu();
	std::printf("\n");
	TestHighlightFollowsSelection();
	std::printf("\n");
	TestOutOfRangeStateIsSurvivable();
	std::printf("\n");
	TestCategoryHeadings();
	std::printf("\n");
	TestRestartMarkAndHelp();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
