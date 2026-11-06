#pragma once

// The hands in a menu and in a conversation: in a menu the hands are not
// pinned and nothing animates them, so they go - hidden with the arms - or,
// with HandsInMenus, stay pinned and drawn as in the world in the game's own
// menus while the Esc menu and its kin take them (HandsAwayNow); a
// conversation takes them from its first frame (game::StepDialogApproach).
// With it the one list of first-person nodes Full VR hides (the sheaths, the
// arms, the stump), the hands' depth and closed backfaces, and the real
// controllers shown while the hands are adjusted.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.18): the code as it was, its
// eight file globals and one static now HandsInMenusState's members; the
// camera pass reads what was decided as g_handsInMenus.<name>. The three
// helpers moved here with it, the camera pass calls them as before.
// Harness: hands-in-menus.txt, body-in-menus.txt, main-menu-step.txt.

#include "camera/FrameLogic.h"
#include "camera/hands/HandStep.h"
#include "camera/hands/Lockpick.h"
#include "game/DialogZoom.h"
#include "game/MenuPause.h"
#include "render/HeadsetRenderer.h"
#include "vr/HandMode.h"

namespace obvr::camera::hands {

struct HandsInMenusState {
	game::DialogApproachState approach{};
	// As last decided at Present, read again before the next world render.
	bool awayForDialog = false;  // the conversation approach is on
	bool stayInMenu = false;     // a menu is up and the hands stay in it (the arms placed for it)
	bool stumpHandsAway = false; // the hands away (menus, dialogue), for the forearm stump
	// The menu's lines: once per change of menu or of the hands' going.
	UInt32 menuLines = 60;
	bool menuLogged = false;
	bool menuWasUp = false;
	bool menuWasAway = false;
	UInt32 menuLastTop = 0;
	// The lock's minigame by the beam and the sticks (camera/hands/Lockpick.h).
	LockpickHandState lockpick;
};

// What the step reads of the rest of the frame.
struct HandsInMenusLinks {
	bool stumpShown;                          // the forearm stump is shown (the arms as a stump)
	bool (*thirdPerson)();                    // the engine's view as it stands
	render::HeadsetRenderer& headsetRenderer; // the real controllers shown in the eyes
};

void StepHandsInMenus(HandsInMenusState& state, const HandStep& step, const HandsInMenusLinks& links);

// Whether the hands go - hidden, not pinned - for what is up now: with
// HandsInMenus the Esc menu and its kin take them, found anywhere in the
// interface manager's stack (camera::HandsAwayForMenus); with it off any
// menu, and the conversation's approach, as before 2026-10-11.
inline bool HandsAwayNow(const Config& config, bool menuMode, bool dialogue) {
	if (!config.hands.handsInMenus) {
		return menuMode || dialogue;
	}
	UInt32 stack[game::kMenuStackEntries];
	const UInt32 count = menuMode ? game::MenuStackEntries(stack, game::kMenuStackEntries) : 0;
	return HandsAwayForMenus(true, menuMode, stack, count);
}

// The one list of first-person nodes Full VR hides (ComposeHandsHideList).
inline const char* HandsHideList(const vr::HandSettings& hands, bool handsAway, bool sheathing,
                          bool armsShownAsStump = false) {
	static char list[256];
	ComposeHandsHideList(list, sizeof(list), hands.hideArms, hands.hideNodes, hands.hideSheaths,
	                     handsAway, sheathing, armsShownAsStump);
	return list;
}

// What the worn cuirass keeps in first person (ComposeCuirassKeepList), or
// null when the arms are not hidden and the cuirass is left to the list.
inline const char* HandsCuirassKeep(const vr::HandSettings& hands) {
	static char keep[160];
	return ComposeCuirassKeepList(keep, sizeof(keep), hands.hideArms && hands.hideWornCuirass, hands.hideNodes)
	           ? keep
	           : nullptr;
}

}  // namespace obvr::camera::hands
