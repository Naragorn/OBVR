#include <cstdio>
#include <cstring>

#include "ui/MainMenuButton.h"

using namespace obvr::ui;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

MainMenuButtonInput InMenu() {
	MainMenuButtonInput in;
	in.menuTile = 0x1000;
	in.mainOnTop = true;
	return in;
}

// A state with the button already in the menu.
MainMenuButtonState Inserted() {
	MainMenuButtonState s;
	StepMainMenuButton(s, InMenu());
	return s;
}

void TestNames() {
	std::printf("The tile and the line\n");
	Check(IsMainMenuVrTile("obvr_vr"), "the button's tile");
	Check(!IsMainMenuVrTile("main_new") && !IsMainMenuVrTile("") && !IsMainMenuVrTile(nullptr) && !IsMainMenuVrTile("obvr_vr_label"),
	      "another tile, none, or its label: not the button");
	Check(std::strcmp(kMainMenuVrInsertLine, "InsertXML \"OBVR\\main_vr_button.xml\" 1044") == 0,
	      "the line: the prefab at the top level of the main menu (1044)");
	Check(std::strlen(kMainMenuVrInsertLine) < 95, "short enough for the console line's queue");
}

void TestInsert() {
	std::printf("The button put into each main menu once\n");
	MainMenuButtonState s;
	MainMenuButtonInput none;
	MainMenuButtonStep step = StepMainMenuButton(s, none);
	Check(!step.insert && !step.startGuide, "no main menu: nothing");
	step = StepMainMenuButton(s, InMenu());
	Check(step.insert && !step.startGuide && s.insertedFor == 0x1000, "the main menu there: the button asked in");
	step = StepMainMenuButton(s, InMenu());
	Check(!step.insert, "the same menu the next frame: not again");
	step = StepMainMenuButton(s, none);
	Check(!step.insert && s.insertedFor == 0, "the menu gone (a game loaded): forgotten");
	step = StepMainMenuButton(s, InMenu());
	Check(step.insert, "back in the main menu: asked in again");
	MainMenuButtonInput other = InMenu();
	other.menuTile = 0x2000;
	step = StepMainMenuButton(s, other);
	Check(step.insert && s.insertedFor == 0x2000, "another main menu at once (built anew): its own button");
	MainMenuButtonInput clickNow = InMenu();
	clickNow.menuTile = 0x3000;
	clickNow.handClickOnButton = true;
	step = StepMainMenuButton(s, clickNow);
	Check(step.insert && !step.startGuide, "a click in the frame it is asked in: no button there yet");
}

void TestClicks() {
	std::printf("The click\n");
	{
		MainMenuButtonState s = Inserted();
		MainMenuButtonInput in = InMenu();
		in.handClickOnButton = true;
		Check(StepMainMenuButton(s, in).startGuide, "a hand's click on the button: the guide again");
	}
	{
		MainMenuButtonState s = Inserted();
		MainMenuButtonInput in = InMenu();
		in.pointerOnButton = true;
		Check(!StepMainMenuButton(s, in).startGuide, "the pointer on it, no click: nothing");
		in.mouseDown = true;
		Check(StepMainMenuButton(s, in).startGuide, "the mouse button going down on it: the guide again");
		Check(!StepMainMenuButton(s, in).startGuide, "the button held: once");
		in.mouseDown = false;
		StepMainMenuButton(s, in);
		in.mouseDown = true;
		Check(StepMainMenuButton(s, in).startGuide, "let go and down again: again");
	}
	{
		MainMenuButtonState s = Inserted();
		MainMenuButtonInput in = InMenu();
		in.mouseDown = true;
		Check(!StepMainMenuButton(s, in).startGuide, "the mouse down somewhere else: nothing");
		in.pointerOnButton = true;
		Check(!StepMainMenuButton(s, in).startGuide, "dragged onto the button while held: no click");
	}
	{
		MainMenuButtonState s = Inserted();
		MainMenuButtonInput in = InMenu();
		in.handClickOnButton = true;
		in.guideShowing = true;
		Check(!StepMainMenuButton(s, in).startGuide, "the guide up already: not again");
	}
	{
		MainMenuButtonState s = Inserted();
		MainMenuButtonInput in = InMenu();
		in.handClickOnButton = true;
		in.mainOnTop = false;
		Check(!StepMainMenuButton(s, in).startGuide, "another menu over the main menu (the options): nothing");
	}
	{
		MainMenuButtonState s = Inserted();
		MainMenuButtonInput in;
		in.handClickOnButton = true;
		in.mouseDown = true;
		in.pointerOnButton = true;
		Check(!StepMainMenuButton(s, in).startGuide, "no main menu (or the button switched off): nothing");
	}
}

}  // namespace

int main() {
	TestNames();
	TestInsert();
	TestClicks();
	std::printf(g_failures == 0 ? "All main menu button checks passed\n" : "%d main menu button checks FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
