#include "game/MainMenuVr.h"

#include <windows.h>

#include "core/Log.h"
#include "game/ConsoleLine.h"
#include "game/GuideTrack.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/NativeMenuPrototype.h"
#include "ui/MainMenuButton.h"

namespace obvr::game {

namespace {

// What the button keeps from frame to frame, owned here.
struct ButtonState {
	bool guideAsked = false;
	bool handClick = false;
	ui::MainMenuButtonState button;
};
ButtonState& State() {
	static ButtonState s_state;
	return s_state;
}

// The mouse's left button, only while the game's own window is in front.
bool MouseDownInGame() {
	if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) {
		return false;
	}
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return process == GetCurrentProcessId();
}

}  // namespace

void RequestStartGuide() { State().guideAsked = true; }
bool StartGuideAsked() { return State().guideAsked; }
void ClearStartGuideAsked() { State().guideAsked = false; }

void NoteMenuClickTile(const char* tileName) { State().handClick = State().handClick || ui::IsMainMenuVrTile(tileName); }

void StepMainMenuVrButton(bool enabled) {
	ui::MainMenuButtonInput in;
	in.menuTile = enabled ? MainMenuRootTile() : 0;
	ButtonState& s = State();
	in.handClickOnButton = s.handClick;
	s.handClick = false;
	if (in.menuTile == 0) {
		if (s.button.insertedFor != 0) {
			s.guideAsked = false;  // an ask never taken does not wait for the next main menu
			ui::StepMainMenuButton(s.button, in);
		}
		return;  // not in the main menu: nothing more a frame
	}
	in.mainOnTop = TopVisibleMenu() == kMenuIdMain;
	in.guideShowing = StartGuideShowing();
	in.mouseDown = MouseDownInGame();
	char tile[64] = {};
	in.pointerOnButton = in.mainOnTop && ActiveTileName(tile, sizeof(tile)) && ui::IsMainMenuVrTile(tile);
	const ui::MainMenuButtonStep step = ui::StepMainMenuButton(s.button, in);
	if (step.insert) {
		const bool queued = RequestConsoleLine(ui::kMainMenuVrInsertLine);
		OBVR_LOG("Main menu: the VR button %s the main menu %08X (MenuQue's InsertXML, OBVR\\main_vr_button.xml)",
		         queued ? "asked into" : "could NOT be asked into", in.menuTile);
	}
	if (step.startGuide) {
		RequestStartGuide();
		OBVR_LOG("Main menu: the VR button clicked (%s) - the start guide asked for again",
		         in.handClickOnButton ? "a hand's click" : "the mouse");
	}
}

}  // namespace obvr::game
