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

	// A pointer over a row takes the highlight there without scrolling.
	const UInt32 window = menu.State().firstVisible;
	Check(menu.Hover(3) && menu.State().selected == 3 && menu.Revision() != still,
	      "hovering a row selects it and repaints");
	Check(menu.State().firstVisible == window, "without moving the window");
	const UInt32 hovered = menu.Revision();
	Check(!menu.Hover(3) && menu.Revision() == hovered, "hovering the same row again is nothing");
	Check(!menu.Hover(SettingDefinitionCount()) && menu.State().selected == 3,
	      "a row past the end is refused");
	menu.Toggle();
	Check(!menu.Hover(1) && menu.State().selected == 3, "closed, a hover does nothing");
	menu.Toggle();
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

void TestActionRow() {
	std::printf("A button row\n");

	SettingsMenu menu;
	Config config;
	menu.Toggle();

	const auto* const settings = SettingDefinitions();
	UInt32 button = SettingDefinitionCount();
	for (UInt32 at = 0; at < SettingDefinitionCount(); ++at) {
		if (settings[at].kind == obvr::ui::ItemKind::Action) {
			button = at;
			break;
		}
	}
	Check(button == 0, "the recenter button is the first row");
	if (button >= SettingDefinitionCount()) {
		return;
	}
	Check(settings[button].action == obvr::ui::SettingAction::Recenter,
	      "and says which button it is");

	for (UInt32 at = 0; at < button; ++at) {
		menu.Apply(MenuAction::Down, config);
	}
	const UInt32 revision = menu.Revision();
	const Config before = config;
	const auto* fired = menu.Apply(MenuAction::Increase, config);
	Check(fired == &settings[button], "right on it hands the row back");
	Check(menu.Apply(MenuAction::Decrease, config) == &settings[button], "so does left");
	Check(menu.Revision() == revision, "nothing to repaint");

	UInt32 disturbed = 0;
	for (UInt32 at = 0; at < SettingDefinitionCount(); ++at) {
		if (settings[at].Read(config) != settings[at].Read(before)) {
			++disturbed;
		}
	}
	Check(disturbed == 0, "and no setting changed");
}

void TestRows() {
	std::printf("Building the rows\n");

	SettingsMenu menu;
	Config config;

	obvr::ui::MenuItem items[96];
	const char* categories[96];

	const UInt32 count = menu.BuildRows(config, items, categories, 96);
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

// The painter's own arithmetic, repeated here so the test can say what the
// menu should be doing rather than ask the menu whether it did it.
UInt32 DrawnLines(UInt32 first, UInt32 last) {
	const auto* const settings = SettingDefinitions();

	const auto same = [](const char* a, const char* b) {
		while (*a != '\0' && *b != '\0') {
			if (*a != *b) {
				return false;
			}
			++a;
			++b;
		}
		return *a == *b;
	};

	UInt32 lines = 0;
	for (UInt32 at = first; at <= last; ++at) {
		if (at > first && !same(settings[at].category, settings[at - 1].category)) {
			++lines;
		}
		++lines;
	}
	return lines;
}

void TestSelectionStaysVisible() {
	std::printf("The highlight never leaves the window\n");

	// A category heading takes a drawn line, so a window with room for ten rows
	// shows fewer than ten settings whenever a boundary falls inside it. Count
	// the selection in rows and it can end up below the last line the painter
	// has room for - and then the highlight is simply not drawn, which from
	// inside a headset looks like the list has stopped responding to the down
	// key.
	//
	// So: walk the whole list, twice round, and check after every single press
	// that the selection is still among the lines that will actually be drawn.
	for (UInt32 window = 3; window <= 12; ++window) {
		SettingsMenu menu;
		Config config;
		menu.Toggle();
		menu.SetVisibleRows(window);

		for (UInt32 press = 0; press < SettingDefinitionCount() * 2; ++press) {
			menu.Apply(MenuAction::Down, config);

			const auto state = menu.State();
			if (state.firstVisible > state.selected) {
				std::printf("        window %u: the window is below the selection\n", window);
				Check(false, "the window never sits below the selection");
				return;
			}
			if (DrawnLines(state.firstVisible, state.selected) > window) {
				std::printf("        window %u: row %u needs %u lines of %u\n", window,
				            state.selected, DrawnLines(state.firstVisible, state.selected),
				            window);
				Check(false, "the selection always fits in the lines available");
				return;
			}
		}
	}
	Check(true, "the selection always fits in the lines available, at every window size");

	// And the same going up, where the window moves the other way.
	for (UInt32 window = 3; window <= 12; ++window) {
		SettingsMenu menu;
		Config config;
		menu.Toggle();
		menu.SetVisibleRows(window);

		for (UInt32 press = 0; press < SettingDefinitionCount() * 2; ++press) {
			menu.Apply(MenuAction::Up, config);

			const auto state = menu.State();
			if (state.firstVisible > state.selected ||
			    DrawnLines(state.firstVisible, state.selected) > window) {
				Check(false, "and going up as well");
				return;
			}
		}
	}
	Check(true, "and going up as well");

	// Paging jumps further than one row, so it is the case most likely to leave
	// the window behind.
	for (UInt32 window = 3; window <= 12; ++window) {
		SettingsMenu menu;
		Config config;
		menu.Toggle();
		menu.SetVisibleRows(window);

		for (UInt32 press = 0; press < 8; ++press) {
			menu.Apply(MenuAction::PageDown, config);
			const auto state = menu.State();
			if (state.firstVisible > state.selected ||
			    DrawnLines(state.firstVisible, state.selected) > window) {
				Check(false, "and paging too");
				return;
			}
		}
		for (UInt32 press = 0; press < 8; ++press) {
			menu.Apply(MenuAction::PageUp, config);
			const auto state = menu.State();
			if (state.firstVisible > state.selected ||
			    DrawnLines(state.firstVisible, state.selected) > window) {
				Check(false, "and paging too");
				return;
			}
		}
	}
	Check(true, "and paging too");
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
	TestActionRow();
	std::printf("\n");
	TestRows();
	std::printf("\n");
	TestSelectionStaysVisible();
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
