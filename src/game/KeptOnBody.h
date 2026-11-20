#pragma once

// The weapons kept at the hips, hung on the player's body (game/KeptOnBodyLogic.h
// says which and where; [Hands] ShowKeptWeapons).
//
// Each hip's weapon is a copy of its model (game/OwnedNode.h LoadModelCopy:
// the weapon's TESModel path, [weapon+0x34]) hung under the third-person
// skeleton's "Bip01 Pelvis" - the parent of the game's own sheath node
// "SideWeapon" - beside that node, never under it: the game's hang moves the
// sheath node's FIRST child onto the hand at a draw (game/WeaponHang.h), and
// a copy in a free slot there would be taken for the weapon. OBVR keeps a
// reference on each copy, so a skeleton rebuilt (a load, a race change) only
// drops the copy: it is not taken off a parent that may be gone, and the next
// frame hangs a new one on the new skeleton.

#include "core/Types.h"
#include "game/NiMath.h"
#include "game/KeptOnBodyLogic.h"

namespace obvr::game {

// Each frame on the main thread, before the scene is drawn
// (CameraHook BeforeFirstScenePass): each hip's copy made, kept or taken off
// as `wanted` says. A model that would not load or hang is tried again when
// the hip wants another form, or after about two seconds.
void StepKeptOnBody(const KeptOnBody& wanted);

// How many of a base form the player's pack holds - the engine's own count
// (0x004869C0, thiscall on the container changes, as Actor::EquipItem counts
// before it equips; the changes from 0x0041E6F0 on the player's extra list).
// 0 when the player, the changes or the count are not as read.
UInt32 PlayerItemCount(UInt8* item);

// Where the copy at `hip` (0 left, 1 right) hangs in the world - its model's
// origin, the weapon's grip as for the game's own sheathed weapon
// (camera/hands/SheathGrab.cpp) - when one hangs on the body drawn now.
bool KeptOnBodyHilt(UInt32 hip, NiPoint3& world);

// For a hand script's mark: each hip's copy - its form and where it hangs in
// the world - and the game's own sheathed weapon beside it ("Kept on body:
// (mark ...)").
void LogKeptOnBody(const char* mark);

}  // namespace obvr::game
