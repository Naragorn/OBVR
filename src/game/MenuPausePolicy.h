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
inline bool WorldPausesForMenu(bool menuMode, bool unpausedMenus, UInt32 topMenuId) {
	if (!menuMode) {
		return false;
	}
	if (!unpausedMenus) {
		return true;
	}
	return !MenuKeepsWorldRunning(topMenuId);
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
