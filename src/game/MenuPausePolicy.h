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
inline bool WorldPausesForMenu(bool menuMode, bool unpausedMenus, UInt32 topMenuId, bool containerRuns = false,
                               bool lockRuns = false) {
	if (!menuMode) {
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
// the controls run as they do.
inline bool PlayerControlsRunUnderMenu(bool menuMode, bool walkWanted, UInt32 topMenuId) {
	if (!menuMode) {
		return true;
	}
	return walkWanted && topMenuId == kMenuIdContainer;
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
