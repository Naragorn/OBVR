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

// Eating and drinking at the mouth (vr/Stow.h): an ingredient - Oblivion's
// food, the apple and the bread - or a potion, drink and food alike.
inline bool IsEdibleType(UInt8 type) { return type == 0x19 || type == 0x28; }

// Whether the ref's base form is one the inventory eats or drinks: an
// ingredient, or a potion with at least one effect that is not hostile (the
// game's own poison test, addr::kEffectListAllHostile). A poison answers
// false with `isPoison` set: from the inventory the game puts it on the
// weapon.
bool RefIsEdible(UInt32 ref, bool* isPoison);

// The base form eaten or drunk as the inventory does it - Actor::EquipItem
// on the player, which eats an ingredient and drinks a potion with the
// game's sound. False, and nothing called, for anything else or a poison.
bool EatItem(UInt32 baseForm);

// The sound the game picks for the item used - eaten or drunk: what EquipItem
// plays after the use (0x005E99C0 with up 1, use 1, through the picker
// 0x005E96E0): "NPCHumanChew" for an ingredient, "NPCHumanSwallow" for a
// potion [read]. For the log; null when the picker is not as read.
const char* ItemUseSoundName(UInt32 baseForm);

// What the engine's grab holds (player+0x578, addr::kPlayerGrabbedRefOffset),
// 0 with nothing or when it cannot be read.
UInt32 GrabbedRef();

}  // namespace obvr::game
