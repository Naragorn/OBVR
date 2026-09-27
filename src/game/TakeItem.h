#pragma once

#include "core/Types.h"

namespace obvr::game {

// Taking a loose item into the player's inventory the way activating it
// takes it (addr::kRefActivate): its script, its owner and the crime are the
// game's. A book goes through the pickup the activate would reach, so it is
// taken rather than opened to read (vr/Stow.h).
enum class TakeResult : UInt8 {
	Taken,        // the activate or the pickup ran
	Refused,      // the activate answered no (a script that keeps it, say)
	CannotTake,   // a book marked "cannot be taken"
	NotAnItem,    // not an item a pack holds, or it could not be read
	NoPlayer,
};

// `ownerId` answers the ref's owner's form ID, 0 with none - for the log.
TakeResult TakeIntoInventory(UInt32 ref, UInt32* ownerId);

// The ref's base form: an item a pack holds, and whether it is a book.
bool RefIsItem(UInt32 ref, bool* isBook);

const char* TakeResultName(TakeResult result);

// What the engine's grab holds (player+0x578, addr::kPlayerGrabbedRefOffset),
// 0 with nothing or when it cannot be read.
UInt32 GrabbedRef();

}  // namespace obvr::game
