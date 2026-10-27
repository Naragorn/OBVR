#pragma once

// The four places' memory (vr/Holster.h, HolsterSlot): the base form id each
// keeps, 0 empty, kept in the co-save (game/HolsterSlotRecord.h, written and
// read by game/QuickKeyPages.cpp) and cleared for a new game. CameraHook reads
// and changes it.

#include "core/Types.h"
#include "vr/Holster.h"

namespace obvr::game {

UInt32 (&HolsterSlotIds())[vr::kHolsterSlotCount];

// The base form of an id the places keep (the game's LookupFormByID,
// 0x0046B250, as game/QuickKeyPages.cpp uses it); nullptr for 0 or an id the
// game does not know.
UInt8* HolsterSlotForm(UInt32 id);

// A base form's id (TESForm +0x0C), 0 for nullptr.
UInt32 FormIdOf(const UInt8* form);

// What a base form is for the places: a weapon by its type (TESObjectWEAP
// +0x90, vr::KindOfWeaponType), armour as a shield (only shields are put at
// a place), a light as a torch; Nothing for anything else or nullptr.
vr::SlotItem SlotItemOfForm(const UInt8* form);

// Whether the last load found the places in the co-save, for the log.
bool HolsterSlotsLoaded();
void SetHolsterSlotsLoaded(bool loaded);

}  // namespace obvr::game
