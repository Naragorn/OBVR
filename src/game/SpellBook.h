#pragma once

#include "core/Types.h"
#include "vr/SpellRing.h"

namespace obvr::game {

// The player's spellbook for the spell ring (docs/controls-spec.md 4.44;
// vr/SpellRing.h): the spells the player knows, and the active spell.
//
// Where the game keeps them (xOBSE GameForms.h, GameObjects.cpp,
// Commands_Player.cpp, read 2026-10-16):
// - The known spells are on the player's base form (TESNPC, the
//   reference's +0x1C): a TESActorBase carries a TESSpellList at +0x54,
//   whose first entry is embedded at +0x4 - so the first node is at
//   +0x58 of the base form, each node a form at +0 and the next node at
//   +4; the list ends at a null form. (The leveled spells are the list
//   beside it and are not the player's.) OBSE's GetSpells reads it so.
// - A SpellItem's MagicItem - the name and the effects - begins at +0x18
//   and ends at +0x34, where the spell's own fields begin: the type (a
//   spell, a disease, a greater power, a lesser power, an ability;
//   vr::SpellKind), the cost, the mastery level, the flags. xOBSE's member
//   comments put them four bytes later, its class sizes (MagicItemForm
//   0x34) and the game put them here (read 2026-10-16, GameAddresses.h).
// - The active spell: PlayerCharacter::GetActiveMagicItem (0x0065D4A0)
//   answers the MagicItem, and PlayerCharacter::SetActiveSpell
//   (0x00664700) takes one - OBSE's SetActiveSpell command passes
//   &spellItem->magicItem, the form + 0x18 - and answers whether it took.

// Reads the player's castable spells into `book`, in the order of their
// names, and counts in `leftOut` the castable ones past the book's room.
// Answers false, the book empty, when the player or the base form cannot
// be read.
bool ReadPlayerSpells(vr::SpellBook& book, UInt32& leftOut);

// The form of the spell that is active now - found in `book` by its
// MagicItem - or 0 when none is, or the active one is not in the book.
UInt32 ActiveSpellForm(const vr::SpellBook& book);

// Makes `form` the active spell the way the magic menu does. Answers what
// the engine answers; false too when the player cannot be read.
bool MakeActiveSpell(UInt32 form);

// The spell list's first nodes for the log - node:form(id "name")/formType/
// spellType/cost/level each - what the book was read from.
void DescribeSpellList(char* out, UInt32 size);

}  // namespace obvr::game
