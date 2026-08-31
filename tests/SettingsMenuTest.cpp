// Checks the settings menu as a thing with a state.
//
// The layer that draws it needs a texture and a headset; this does not, because
// every decision was kept on this side of the seam. What key does what is the
// caller's business and arrives here as a MenuAction, so opening, moving,
// changing a value and the revision the layer repaints on can all be driven
// from here.
//
// The revision gets particular attention. It is what tells the layer the
// picture would differ, and it fails in two opposite directions, both of which
// look like something else: never advancing reads as a menu that has stopped
// responding, and advancing on every keypress reads as a menu that is slow,
// because a quarter of a million pixels are being repainted to produce an
// identical image.
//
// No Windows API here, so this one builds and runs on Linux as well.

#include <cstdio>

#include "ui/SettingsMenu.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::Config;
using obvr::ui::MenuAction;
using obvr::ui::SettingDefinitionCount;
using obvr::ui::SettingDefinitions;
using obvr::ui::SettingsMenu;

void TestOpening() {
	std::printf("Opening and closing\n");

	SettingsMenu menu;
	Config config;

	Check(!menu.IsOpen(), "the menu starts closed");

	const UInt32 before = menu.Revision();
	menu.Toggle();
	Check(menu.IsOpen(), "and opens");
	Check(menu.Revision() != before, "and says the picture changed");

	menu.Toggle();
	Check(!menu.IsOpen(), "and closes again");

	// A closed menu must not act on keys. The arrows are read from the keyboard
	// directly, so a menu that acted while closed would change settings while
	// somebody was playing.
	menu.Toggle();
	menu.Apply(MenuAction::Down, config);
	const auto opened = menu.State();
	menu.Toggle();

	const UInt32 closedRevision = menu.Revision();
	menu.Apply(MenuAction::Down, config);
	menu.Apply(MenuAction::Increase, config);
	Check(menu.State().selected == opened.selected, "a closed menu ignores movement");
	Check(menu.Revision() == closedRevision, "and repaints nothing");

	// Reopening keeps the place. Somebody who closes the menu to look at
	// something is almost always coming back to the same row.
	menu.Toggle();
	Check(menu.State().selected == opened.selected, "reopening keeps the selected row");
}

void TestMoving() {
	std::printf("Moving the highlight\n");

	SettingsMenu menu;
	Config config;
	menu.Toggle();
	menu.SetVisibleRows(5);

	const UInt32 start = menu.State().selected;
	const UInt32 revision = menu.Revision();

	menu.Apply(MenuAction::Down, config);
	Check(menu.State().selected != start, "down moves the highlight");
	Check(menu.Revision() != revision, "and asks for a repaint");

	// Nothing at all must not repaint.
	const UInt32 still = menu.Revision();
	menu.Apply(MenuAction::None, config);
	Check(menu.Revision() == still, "doing nothing repaints nothing");
}

void TestChangingValues() {
	std::printf("Changing a value\n");

	SettingsMenu menu;
	Config config;
	menu.Toggle();

	// Find a row whose value can actually be raised, so the test does not
	// depend on which setting happens to be first in the table.
	const auto* const settings = SettingDefinitions();
	UInt32 movable = SettingDefinitionCount();
	for (UInt32 at = 0; at < SettingDefinitionCount(); ++at) {
		if (settings[at].Read(config) < settings[at].maximum) {
			movable = at;
			break;
		}
	}
	Check(movable < SettingDefinitionCount(), "some row can be raised");
	if (movable >= SettingDefinitionCount()) {
		return;
	}

	for (UInt32 at = 0; at < movable; ++at) {
		menu.Apply(MenuAction::Down, config);
	}
	Check(menu.State().selected == movable, "and the highlight reached it");

	const float before = settings[movable].Read(config);
	const UInt32 revision = menu.Revision();

	menu.Apply(MenuAction::Increase, config);
	Check(settings[movable].Read(config) != before, "the setting changed");
	Check(menu.Revision() != revision, "and the picture with it");

	// Only the selected setting changed. The table's own test proves no row
	// writes another's field; this proves the menu writes the row it is on.
	UInt32 disturbed = 0;
	Config fresh;
	for (UInt32 at = 0; at < SettingDefinitionCount(); ++at) {
		if (at == movable) {
			continue;
		}
		if (settings[at].Read(config) != settings[at].Read(fresh)) {
			++disturbed;
		}
	}
	Check(disturbed == 0, "and nothing else did");

	// A value pinned at the end of its range changes nothing, and must not
	// repaint - otherwise holding a key at the limit repaints for ever.
	for (int press = 0; press < 200; ++press) {
		menu.Apply(MenuAction::Increase, config);
	}
	const float pinned = settings[movable].Read(config);
	Check(pinned == settings[movable].maximum, "a value can be driven to its limit");

	const UInt32 atLimit = menu.Revision();
	menu.Apply(MenuAction::Increase, config);
	Check(menu.Revision() == atLimit, "and pressing on at the limit repaints nothing");

	// Down from the limit works, which is what says the limit was not sticky.
	menu.Apply(MenuAction::Decrease, config);
	Check(settings[movable].Read(config) < pinned, "and it comes back down");
}

void TestRows() {
	std::printf("Building the rows\n");

	SettingsMenu menu;
	Config config;

	obvr::ui::MenuItem items[64];
	const char* categories[64];

	const UInt32 count = menu.BuildRows(config, items, categories, 64);
	Check(count == SettingDefinitionCount(), "every setting becomes a row");
	Check(count > 0, "and there is at least one");

	bool labelled = true;
	for (UInt32 at = 0; at < count; ++at) {
		if (items[at].label == nullptr || categories[at] == nullptr) {
			labelled = false;
			break;
		}
	}
	Check(labelled, "each row has a label and a category");

	// A capacity smaller than the table. Writing past the caller's array is the
	// fault this guards, and the caller's array is on a stack in the frame
	// hook.
	obvr::ui::MenuItem few[3];
	const char* fewCategories[3];
	const UInt32 limited = menu.BuildRows(config, few, fewCategories, 3);
	Check(limited == 3, "a small array is filled to its capacity and no further");

	const UInt32 none = menu.BuildRows(config, few, fewCategories, 0);
	Check(none == 0, "and a capacity of nothing writes nothing");

	Check(menu.BuildRows(config, nullptr, fewCategories, 3) == 0, "no items array, no rows");
	Check(menu.BuildRows(config, few, nullptr, 3) == 0, "and no categories array either");
}

void TestVisibleRows() {
	std::printf("How many rows the layer says fit\n");

	SettingsMenu menu;
	Config config;
	menu.Toggle();

	// Scroll down into a long list with a big window, then shrink the window.
	// The window has to come back into range, or it shows blank space below the
	// last row - which reads as the menu having lost its contents.
	menu.SetVisibleRows(20);
	for (UInt32 press = 0; press < SettingDefinitionCount(); ++press) {
		menu.Apply(MenuAction::Down, config);
	}
	menu.SetVisibleRows(4);

	const auto state = menu.State();
	Check(state.firstVisible <= state.selected, "the window is not below the selection");
	Check(state.selected < state.firstVisible + 4, "and the selection is inside the window");

	// A window of nothing would divide the scrolling by zero.
	menu.SetVisibleRows(0);
	Check(true, "a window of no rows is survivable");
}

}  // namespace

int main() {
	std::printf("OBVR settings menu test\n\n");

	TestOpening();
	std::printf("\n");
	TestMoving();
	std::printf("\n");
	TestChangingValues();
	std::printf("\n");
	TestRows();
	std::printf("\n");
	TestVisibleRows();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
