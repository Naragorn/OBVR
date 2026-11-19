#pragma once

#include "core/Types.h"
#include "game/MenuType.h"

namespace obvr::game {

// The small import thunk used by one existing menu plugin: mov eax,target;
// jmp eax. Recognising it lets OBVR redirect only that plugin's call to
// IsMenuMode while preserving the rest of its animation hook.
inline bool IsAbsoluteJumpTo(const UInt8* bytes, UInt32 target) {
	return bytes != nullptr && bytes[0] == 0xB8 && bytes[1] == (target & 0xFF) &&
	       bytes[2] == ((target >> 8) & 0xFF) && bytes[3] == ((target >> 16) & 0xFF) &&
	       bytes[4] == ((target >> 24) & 0xFF) && bytes[5] == 0xFF && bytes[6] == 0xE0;
}

// The decision behind Render.UnpausedMenus, kept apart from the patched
// call sites so every flow can be exercised without a game.
//
// Vanilla pauses the whole simulation whenever any menu is up. The option
// keeps it running behind the menus the player opens to look at their own
// things - the F1-F4 screens, a container, a book, and the two popups the
// inventory raises - and nowhere else. Everything opened from a conversation
// stays paused on purpose: the speaker's AI would otherwise walk off
// mid-sentence, which is exactly the fault Real Time Menus needed two more
// hooks to suppress. The Esc menu, options, loading, saving, sleeping,
// lockpicking, level-up and the main menu pause as they always did.
inline bool MenuKeepsWorldRunning(UInt32 topMenuId) {
	switch (topMenuId) {
	case kMenuIdBigFour:
	case kMenuIdInventory:
	case kMenuIdStats:
	case kMenuIdMagic:
	case kMenuIdMap:
	case kMenuIdContainer:
	case kMenuIdBook:
	case kMenuIdQuantity:
	case kMenuIdMagicPopup:
		return true;
	default:
		return false;
	}
}

// What the redirected IsMenuMode call sites are told. The question the
// engine asks at each of them is "should this subsystem pause", and the
// answer is vanilla's own unless the option is on and the menu on top is
// one of the above. Outside menu mode the answer is always no, whatever
// the stack says - the stack can hold the HUD's own entries.
//
// `containerRuns` is [Look] ContainerInWorld (the tester, 2026-10-08): the
// container's menu alone keeps the world running, whatever the option.
// `lockRuns` is [Hands] ReachOpens (the tester, 2026-10-09: "Verschlossene
// Container auch hier wenn ich mich näher kommt dann das Schlossknacken
// minigame wie das neue menü"): the lock's minigame keeps it running too,
// as the container's panel does.
//
// `barter` is a merchant's barter (BarterInMenuStack; docs/container-touch-
// spec.md 8.1-8.2): opened from a conversation, so it pauses as vanilla
// whatever the switches say - the container menu, its quantity box and its
// message boxes alike, so the merchant's sound and AI are not started and
// stopped at every switch between them.
//
// `tabletRuns` is the tablet (docs/inventory-tablet-spec.md 4.4): its menus
// are up and the one on top keeps the world running (vr::TabletMenusUp and
// vr::TabletMenuRunsWorld, asked by the caller), whatever the option says.
inline bool WorldPausesForMenu(bool menuMode, bool unpausedMenus, UInt32 topMenuId, bool containerRuns = false,
                               bool lockRuns = false, bool barter = false, bool tabletRuns = false) {
	if (!menuMode) {
		return false;
	}
	if (barter) {
		return true;
	}
	if (tabletRuns) {
		return false;
	}
	if (containerRuns && topMenuId == kMenuIdContainer) {
		return false;
	}
	if (lockRuns && topMenuId == kMenuIdLockPick) {
		return false;
	}
	if (!unpausedMenus) {
		return true;
	}
	return !MenuKeepsWorldRunning(topMenuId);
}

// Whether the player's own controls run under the menu on top (the update
// step's site kPlayerControlsIsMenuModeSite answers "no menu"): only while
// the sticks walk the player (`walkWanted`, from the hand mode) and the
// menu on top is a container's - the one the reach opens (vr/ReachOpen.h;
// the tester, 2026-10-09: "es muss möglich sein weiterhin sich normal zu
// bewegen mit stick links bei den neuen offenen menüs"). Outside menu mode
// the controls run as they do. A merchant's barter (`barter`) is not a
// reached container: gated as vanilla. `tabletWalk` is the tablet's menus up
// with the world running behind them (docs/inventory-tablet-spec.md 4.6,
// stage 4; Nadi, 2026-11-18: walking on with the left stick, a click on the
// tablet neither an attack nor a stop): the sticks walk under them as well.
inline bool PlayerControlsRunUnderMenu(bool menuMode, bool walkWanted, UInt32 topMenuId, bool barter = false,
                                       bool tabletWalk = false) {
	if (!menuMode) {
		return true;
	}
	return walkWanted && ((topMenuId == kMenuIdContainer && !barter) || tabletWalk);
}

// The game's controls, by their index in the control map (0x00403520,
// OSInputGlobals::GetControlState, reads them by it; xOBSE's list, the
// same order as the 29 control-name settings at 0xB399D0).
inline constexpr UInt32 kControlForward = 0;
inline constexpr UInt32 kControlBack = 1;
inline constexpr UInt32 kControlSlideLeft = 2;
inline constexpr UInt32 kControlSlideRight = 3;
inline constexpr UInt32 kControlAttack = 4;
inline constexpr UInt32 kControlActivate = 5;
inline constexpr UInt32 kControlBlock = 6;
inline constexpr UInt32 kControlCast = 7;
inline constexpr UInt32 kControlSneak = 9;
inline constexpr UInt32 kControlRun = 10;
inline constexpr UInt32 kControlAlwaysRun = 11;
inline constexpr UInt32 kControlAutoMove = 12;
inline constexpr UInt32 kControlJump = 13;
inline constexpr UInt32 kControlMenuMode = 15;
inline constexpr UInt32 kControlGrab = 28;

// What the player's controls read of a control while they run under a menu
// (PlayerControlsRunUnderMenu): the walk alone. The menu's click is the
// attack's own button (game/HandControls.cpp), and the player's input,
// running, took it as a blow; the menu key, activate and the rest belong to
// the menu while it is up. False: the control reads 0 to the player.
inline bool PlayerControlReadUnderMenu(UInt32 control) {
	switch (control) {
	case kControlForward:
	case kControlBack:
	case kControlSlideLeft:
	case kControlSlideRight:
	case kControlRun:
	case kControlAlwaysRun:
	case kControlAutoMove:
		return true;
	default:
		return false;
	}
}

// A merchant's barter (docs/container-touch-spec.md 8.1): the engine opens it
// as the ContainerMenu from the dialogue, and the dialogue stays in the
// interface manager's stack under it (the barter closes back into it). A
// chest, a body or a pickpocket opens the ContainerMenu with no dialogue
// there.
inline bool MenuStackHas(const UInt32* stack, UInt32 count, UInt32 menuId) {
	if (stack == nullptr) {
		return false;
	}
	for (UInt32 i = 0; i < count; ++i) {
		if (stack[i] == menuId) {
			return true;
		}
	}
	return false;
}

inline bool BarterInMenuStack(const UInt32* stack, UInt32 count) {
	return MenuStackHas(stack, count, kMenuIdContainer) && MenuStackHas(stack, count, kMenuIdDialog);
}

// Whether the player's own anim data stands in for the F1-F4 figure's
// during the player's update (addr::kPlayerMenuAnimDataOffset): the walk is
// that anim data's root motion, and with the figure's in the player's
// +0x5DC the stick walked on the spot under the tablet. Only while the
// player's controls run under the tablet and the figure's anim data is set;
// it is put back at the span's end, before the menus run again.
inline bool PlayerOwnAnimDataForSpan(bool spanUnderTablet, bool figureAnimDataSet) {
	return spanUnderTablet && figureAnimDataSet;
}

// The mouse axis the player's controls read as the wheel (the third of
// 0x00403190's axes; game/GameAddresses.h kPlayerWheelReadSite).
inline constexpr UInt32 kMouseAxisWheel = 3;

// Whether the player's controls get a mouse axis's value. In vanilla they
// never run in menu mode, so the wheel is the menu's alone; under a
// container's menu they run while the sticks walk (PlayerControlsRunUnderMenu),
// and the wheel OBVR sends there to scroll the list was also read by the
// player's camera: in first person a notch back is the engine's zoom out to
// third person (Nadi, Windows, 2026-10-24: a body opened by reaching, then
// third person until R). So in menu mode the wheel reads 0 to them. The x
// and y pass under a container's menu: OBVR sends no cursor step while the
// sticks walk there, and the right stick's turn is the mouse's x. Under the
// tablet (`underTablet`) the laser's cursor steps go on while walking, so
// the x and y are the menu's alone - the right stick scrolls there and
// turns nothing.
inline bool PlayerControlsReadAxis(bool menuMode, UInt32 axis, bool underTablet = false) {
	if (!menuMode) {
		return true;
	}
	return axis != kMouseAxisWheel && !underTablet;
}

// GetTopVisibleMenuID can briefly return none while the F1-F4 stack remains
// open. Keep the last observed menu for that one menu-mode episode so the
// seven subsystem checks cannot alternate between running and paused answers.
inline UInt32 StablePauseMenuId(bool menuMode, UInt32 observed, UInt32 remembered) {
	if (!menuMode) {
		return kMenuIdNone;
	}
	return observed != kMenuIdNone ? observed : remembered;
}

}  // namespace obvr::game
