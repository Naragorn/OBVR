#pragma once

#include "core/Types.h"
#include "game/MenuType.h"

namespace obvr::game {

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

}  // namespace obvr::game
