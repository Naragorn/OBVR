#pragma once

// The "VR" button in the game's main menu (Nadi, 2026-11-16: "wir brauchen
// einen VR button im hauptmenü. dieser startet dann einfach wieder das
// onboarding"). OBVR ships no main menu of its own - a UI mod's would lose its
// own - so the button is one tile put into whatever main menu is loaded, by
// MenuQue's InsertXML (cs.uesp.net/wiki/InsertXML: "file|component", the
// component left out for the menu's top level; the file under
// Data\Menus\Prefabs), once for each main menu the game builds.
// The engine hands the click to the main menu, which knows no such button;
// OBVR notices it itself: the hands' click with the cursor on the tile
// (camera/hands/ControlsOut.cpp), or the mouse's button going down while the
// tile is the interface's active one.
// Pure: one step a frame over plain values, game/MainMenuVr.cpp carries it
// out. Covered by main_menu_button_test.

#include <cstring>

#include "core/Types.h"

namespace obvr::ui {

inline constexpr const char* kMainMenuVrTile = "obvr_vr";
// The console's line (a path's backslash doubled for the C string only).
inline constexpr const char* kMainMenuVrInsertLine = "InsertXML \"OBVR\\main_vr_button.xml\" 1044";

inline bool IsMainMenuVrTile(const char* tileName) {
	return tileName != nullptr && std::strcmp(tileName, kMainMenuVrTile) == 0;
}

struct MainMenuButtonState {
	UInt32 insertedFor = 0;  // the main menu's tile the button was asked into (0: none)
	bool mouseWas = false;
};

struct MainMenuButtonInput {
	UInt32 menuTile = 0;        // the loaded main menu's tile, 0 without one (or with the button switched off)
	bool mainOnTop = false;     // the main menu is the menu on top
	bool guideShowing = false;  // the start guide is up already
	bool mouseDown = false;     // the mouse's left button, the game in front
	bool pointerOnButton = false;    // the interface's active tile is the button
	bool handClickOnButton = false;  // a hand's click went out with the cursor on the button
};

struct MainMenuButtonStep {
	bool insert = false;      // ask MenuQue to put the button into the menu now
	bool startGuide = false;  // the button was clicked: the start guide again
};

inline MainMenuButtonStep StepMainMenuButton(MainMenuButtonState& s, const MainMenuButtonInput& in) {
	MainMenuButtonStep step;
	const bool mouseEdge = in.mouseDown && !s.mouseWas;
	s.mouseWas = in.mouseDown;
	if (in.menuTile == 0) {
		s.insertedFor = 0;  // the menu is gone: the next one gets its own button
		return step;
	}
	if (in.menuTile != s.insertedFor) {
		s.insertedFor = in.menuTile;
		step.insert = true;
		return step;  // nothing to click before it is there
	}
	const bool clicked = in.handClickOnButton || (mouseEdge && in.pointerOnButton);
	step.startGuide = clicked && in.mainOnTop && !in.guideShowing;
	return step;
}

}  // namespace obvr::ui
