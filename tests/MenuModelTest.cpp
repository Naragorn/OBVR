// Checks what a settings menu does, apart from the drawing of it.
//
// Every decision the menu makes is here rather than in the layer that renders
// it, which is what lets all of it be checked at a desk: where the highlight
// goes at the end of a list, what a step does at the end of a range, and how a
// number becomes the text of a number.
//
// That last one gets the most attention, and deliberately. The plugin builds
// without the CRT, so there is no snprintf to lean on and the conversion is
// written out by hand - and a number formatter is the classic piece of code
// that works for every value anyone tries by hand and then prints "0.100" for
// 0.999 in front of the one person who is trying to read a setting.
//
// No Windows API here, so this one builds and runs on Linux as well.

#include <cstdio>

#include "ui/MenuModel.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool TextIs(const char* a, const char* b) {
	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

using obvr::ui::AdjustValue;
using obvr::ui::AdvanceMenu;
using obvr::ui::FormatInteger;
using obvr::ui::FormatValue;
using obvr::ui::ItemKind;
using obvr::ui::MenuAction;
using obvr::ui::MenuItem;
using obvr::ui::MenuState;

// Formats a value and compares it, so each case below is one readable line
// instead of four.
bool Formats(float value, UInt32 decimals, const char* expected) {
	MenuItem item;
	item.kind = ItemKind::Number;
	item.value = value;
	item.decimals = decimals;

	char text[32];
	FormatValue(item, text, sizeof(text));

	const bool matched = TextIs(text, expected);
	if (!matched) {
		std::printf("        got \"%s\", wanted \"%s\"\n", text, expected);
	}
	return matched;
}

void TestMoving() {
	std::printf("Moving through a list\n");

	MenuState state;

	// An empty list. A menu with nothing in it is a bug in whoever built the
	// list, but it must not be a bug that indexes into nothing.
	state.selected = 5;
	state = AdvanceMenu(state, MenuAction::Down, 0, 8);
	Check(state.selected == 0 && state.firstVisible == 0, "an empty list stays at the top");

	// Ordinary movement.
	state = MenuState{};
	state = AdvanceMenu(state, MenuAction::Down, 10, 5);
	Check(state.selected == 1, "down moves one row");
	state = AdvanceMenu(state, MenuAction::Up, 10, 5);
	Check(state.selected == 0, "and up moves back");

	// Wrapping, both ways. Holding a key and watching the highlight stop dead
	// reads as lost input in a headset.
	state = AdvanceMenu(state, MenuAction::Up, 10, 5);
	Check(state.selected == 9, "up from the top wraps to the bottom");
	state = AdvanceMenu(state, MenuAction::Down, 10, 5);
	Check(state.selected == 0, "and down from the bottom wraps to the top");

	// Moving the selection must never disturb the setting it lands on, and the
	// actions that change a value must never move the selection.
	state = MenuState{};
	state = AdvanceMenu(state, MenuAction::Down, 10, 5);
	const MenuState before = state;
	state = AdvanceMenu(state, MenuAction::Increase, 10, 5);
	Check(state.selected == before.selected, "increasing a value does not move the selection");
	state = AdvanceMenu(state, MenuAction::Decrease, 10, 5);
	Check(state.selected == before.selected, "nor does decreasing it");
	state = AdvanceMenu(state, MenuAction::None, 10, 5);
	Check(state.selected == before.selected, "nor does nothing at all");

	// A selection past the end, which a caller whose list got shorter can hand
	// in. It has to be brought back before anything moves.
	state = MenuState{};
	state.selected = 50;
	state = AdvanceMenu(state, MenuAction::None, 10, 5);
	Check(state.selected == 9, "a selection past the end comes back to the last row");

	// A window of no rows would divide the scrolling by nothing.
	state = MenuState{};
	state = AdvanceMenu(state, MenuAction::Down, 10, 0);
	Check(state.selected == 1, "a window of no rows still moves");
}

void TestPaging() {
	std::printf("Paging through a list\n");

	MenuState state;

	state = AdvanceMenu(state, MenuAction::PageDown, 40, 10);
	Check(state.selected == 8, "a page is the window less its context rows");

	// A page stops rather than wrapping - see the note in the header on why
	// the two behave differently.
	state.selected = 38;
	state = AdvanceMenu(state, MenuAction::PageDown, 40, 10);
	Check(state.selected == 39, "a page down at the end stops at the last row");

	state.selected = 3;
	state = AdvanceMenu(state, MenuAction::PageUp, 40, 10);
	Check(state.selected == 0, "and a page up at the start stops at the first");

	// Exactly at the boundary, which is where an off-by-one lives.
	state.selected = 8;
	state = AdvanceMenu(state, MenuAction::PageUp, 40, 10);
	Check(state.selected == 0, "a page up from exactly one page in lands on the first row");
}

void TestScrolling() {
	std::printf("The window following the selection\n");

	MenuState state;

	// Down past the bottom scrolls by one, not by a page: a list that jumps
	// under a moving highlight cannot be tracked.
	for (int step = 0; step < 5; ++step) {
		state = AdvanceMenu(state, MenuAction::Down, 20, 5);
	}
	Check(state.selected == 5, "five moves down reach row five");
	Check(state.firstVisible == 1, "and the window has scrolled by exactly one");

	// Back up, and the window comes with it.
	for (int step = 0; step < 5; ++step) {
		state = AdvanceMenu(state, MenuAction::Up, 20, 5);
	}
	Check(state.selected == 0 && state.firstVisible == 0, "and back up again");

	// The wrap is the case that needs the window moved in both directions in
	// one action: the selection ends up below the window having gone up.
	state = MenuState{};
	state = AdvanceMenu(state, MenuAction::Up, 20, 5);
	Check(state.selected == 19, "a wrap to the bottom");
	Check(state.firstVisible == 15, "brings the window with it");

	state = AdvanceMenu(state, MenuAction::Down, 20, 5);
	Check(state.selected == 0 && state.firstVisible == 0, "and a wrap back to the top");

	// A list shorter than the window must not leave blank rows above the
	// items, which is what a window still scrolled from a longer list gives.
	state = MenuState{};
	state.firstVisible = 3;
	state = AdvanceMenu(state, MenuAction::None, 4, 10);
	Check(state.firstVisible == 0, "a list shorter than the window sits at the top");

	// And a window hanging past the end of a longer list.
	state = MenuState{};
	state.selected = 19;
	state.firstVisible = 18;
	state = AdvanceMenu(state, MenuAction::None, 20, 5);
	Check(state.firstVisible == 15, "a window past the end is pulled back to it");
}

void TestAdjusting() {
	std::printf("Changing a value\n");

	MenuItem toggle;
	toggle.kind = ItemKind::Toggle;
	toggle.value = 0.0f;

	Check(AdjustValue(toggle, MenuAction::Increase) == 1.0f, "a toggle turns on");
	toggle.value = 1.0f;
	Check(AdjustValue(toggle, MenuAction::Increase) == 0.0f, "and off again");

	// Both directions do the same thing, so whichever key is nearer works.
	toggle.value = 0.0f;
	Check(AdjustValue(toggle, MenuAction::Decrease) == 1.0f, "and either direction flips it");

	// A click on a row, for a laser: a toggle flips whichever way, a number
	// steps by the half it was clicked on, a button fires, text is nothing.
	using obvr::ui::ClickActionFor;
	toggle.value = 0.0f;
	Check(ClickActionFor(toggle, 0.9f) == MenuAction::Increase, "a click turns a toggle on");
	toggle.value = 1.0f;
	Check(ClickActionFor(toggle, 0.1f) == MenuAction::Decrease, "and off");
	MenuItem clicked;
	clicked.kind = ItemKind::Number;
	Check(ClickActionFor(clicked, 0.25f) == MenuAction::Decrease, "the left half steps down");
	Check(ClickActionFor(clicked, 0.75f) == MenuAction::Increase, "the right half steps up");
	MenuItem button;
	button.kind = ItemKind::Action;
	Check(ClickActionFor(button, 0.1f) == MenuAction::Increase, "a button fires");
	MenuItem text;
	text.kind = ItemKind::Text;
	Check(ClickActionFor(text, 0.5f) == MenuAction::None, "text is nothing");

	// A value from an INI that is neither 0 nor 1. Anything not clearly off is
	// on, so the switch never sits in a third state nobody can name.
	toggle.value = 2.0f;
	Check(AdjustValue(toggle, MenuAction::Increase) == 0.0f, "a toggle at 2 counts as on");

	Check(AdjustValue(toggle, MenuAction::None) == 2.0f, "and nothing at all leaves it alone");
	Check(AdjustValue(toggle, MenuAction::Down) == 2.0f, "as does moving the selection");

	MenuItem number;
	number.kind = ItemKind::Number;
	number.value = 1.0f;
	number.minimum = 0.0f;
	number.maximum = 2.0f;
	number.step = 0.5f;

	Check(AdjustValue(number, MenuAction::Increase) == 1.5f, "a number steps up");
	Check(AdjustValue(number, MenuAction::Decrease) == 0.5f, "and down");

	// The ends of the range. A step that would go past stops exactly on the
	// limit rather than somewhere short of it.
	number.value = 1.8f;
	Check(AdjustValue(number, MenuAction::Increase) == 2.0f, "a step past the top stops at it");
	number.value = 0.2f;
	Check(AdjustValue(number, MenuAction::Decrease) == 0.0f, "and past the bottom, at that");

	number.value = 2.0f;
	Check(AdjustValue(number, MenuAction::Increase) == 2.0f, "and at the top it stays");
	number.value = 0.0f;
	Check(AdjustValue(number, MenuAction::Decrease) == 0.0f, "and at the bottom too");

	// A negative range, which the eye separation and the look ranges use.
	MenuItem signedItem;
	signedItem.kind = ItemKind::Number;
	signedItem.value = 0.0f;
	signedItem.minimum = -1.0f;
	signedItem.maximum = 1.0f;
	signedItem.step = 0.25f;
	Check(AdjustValue(signedItem, MenuAction::Decrease) == -0.25f, "a value can go negative");
}

void TestFormattingNumbers() {
	std::printf("A number as text\n");

	Check(Formats(0.0f, 0, "0"), "zero with no decimals");
	Check(Formats(0.0f, 2, "0.00"), "and with two");
	Check(Formats(1.0f, 0, "1"), "a whole number");
	Check(Formats(42.0f, 0, "42"), "and a bigger one");
	Check(Formats(1.5f, 1, "1.5"), "one decimal");
	Check(Formats(1.05f, 2, "1.05"), "two decimals");

	// The leading zero in a fraction. Without it, 0.05 prints as "0.5" - a
	// value ten times what it is, in a menu somebody is using to set it.
	Check(Formats(0.05f, 2, "0.05"), "a fraction keeps its leading zero");
	Check(Formats(0.005f, 3, "0.005"), "and two of them");

	// Carrying into the whole part. This is the case a formatter that rounds
	// the fraction on its own gets wrong, and it gets it wrong as "0.100".
	Check(Formats(0.999f, 2, "1.00"), "a fraction that rounds up carries");
	Check(Formats(1.999f, 2, "2.00"), "and so does one past a whole number");
	Check(Formats(9.99f, 1, "10.0"), "and it can add a digit");

	// Half away from zero, in both directions.
	Check(Formats(0.125f, 2, "0.13"), "a half rounds away from zero");
	Check(Formats(-0.125f, 2, "-0.13"), "and the same going down");

	// Negatives generally.
	Check(Formats(-1.5f, 1, "-1.5"), "a negative number");
	Check(Formats(-0.05f, 2, "-0.05"), "and a negative fraction");

	// Truncation, not overrun. A row that shows a short number is cosmetic; a
	// write past a stack buffer is not.
	MenuItem item;
	item.kind = ItemKind::Number;
	item.value = 123.456f;
	item.decimals = 2;

	char small[4];
	for (UInt32 at = 0; at < sizeof(small); ++at) {
		small[at] = 'X';
	}
	FormatValue(item, small, sizeof(small));
	Check(TextIs(small, "123"), "a value too long for the buffer is truncated");

	char terminated[8];
	FormatValue(item, terminated, sizeof(terminated));
	bool hasEnd = false;
	for (UInt32 at = 0; at < sizeof(terminated); ++at) {
		if (terminated[at] == '\0') {
			hasEnd = true;
			break;
		}
	}
	Check(hasEnd, "and always terminated");

	// No buffer at all, and a buffer of no size. Both have to be survivable.
	FormatValue(item, nullptr, 0);
	FormatValue(item, small, 0);
	Check(true, "no buffer at all is survivable");

	// A value nothing sensible can be said about. Better a dash than digits
	// from an undefined conversion.
	item.value = 1.0e30f;
	char huge[16];
	FormatValue(item, huge, sizeof(huge));
	Check(TextIs(huge, "-"), "a value beyond an integer prints a dash");
}

void TestFormattingToggles() {
	std::printf("A switch as text\n");

	MenuItem item;
	item.kind = ItemKind::Toggle;

	char text[8];

	item.value = 1.0f;
	FormatValue(item, text, sizeof(text));
	Check(TextIs(text, "on"), "a switch that is on says so");

	item.value = 0.0f;
	FormatValue(item, text, sizeof(text));
	Check(TextIs(text, "off"), "and one that is off");

	item.value = 2.0f;
	FormatValue(item, text, sizeof(text));
	Check(TextIs(text, "on"), "and anything not off reads as on");

	// The decimals of a toggle are meaningless and must not leak into the
	// text - "on.00" would be a strange thing to read.
	item.decimals = 2;
	FormatValue(item, text, sizeof(text));
	Check(TextIs(text, "on"), "a switch ignores its decimals");
}

void TestFormattingIntegers() {
	std::printf("A whole number as text\n");

	char text[16];

	FormatInteger(0, text, sizeof(text));
	Check(TextIs(text, "0"), "zero");

	FormatInteger(7, text, sizeof(text));
	Check(TextIs(text, "7"), "one digit");

	FormatInteger(1234, text, sizeof(text));
	Check(TextIs(text, "1234"), "several");

	FormatInteger(-42, text, sizeof(text));
	Check(TextIs(text, "-42"), "and a negative one");

	// The most negative integer has no positive counterpart, and negating it
	// lands back on itself with the sign still set - which prints as a run of
	// nonsense digits rather than as an error.
	FormatInteger(-2147483647 - 1, text, sizeof(text));
	Check(TextIs(text, "-2147483648"), "the most negative integer prints correctly");

	FormatInteger(2147483647, text, sizeof(text));
	Check(TextIs(text, "2147483647"), "and the most positive one");

	char small[3];
	FormatInteger(1234, small, sizeof(small));
	Check(TextIs(small, "12"), "a number too long for the buffer is truncated");

	FormatInteger(5, nullptr, 0);
	Check(true, "and no buffer at all is survivable");
}

}  // namespace

int main() {
	std::printf("OBVR menu model test\n\n");

	TestMoving();
	std::printf("\n");
	TestPaging();
	std::printf("\n");
	TestScrolling();
	std::printf("\n");
	TestAdjusting();
	std::printf("\n");
	TestFormattingNumbers();
	std::printf("\n");
	TestFormattingToggles();
	std::printf("\n");
	TestFormattingIntegers();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
