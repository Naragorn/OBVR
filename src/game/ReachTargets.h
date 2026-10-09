#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/ReachOpen.h"

namespace obvr::game {

// The engine side of opening by reaching (vr/ReachOpen.h): what a hand is
// at, how far, whether it is locked, and the engine's own open and close.

// The hands as the search sees them: positions in the world, and whether
// each may be used. [0] the right hand, [1] the left.
struct ReachHands {
	NiPoint3 position[2]{};
	bool valid[2] = {false, false};
};

// The nearest container (base form type 0x17), body (a dead Character or
// Creature) or living actor to a usable hand, among the references of the
// player's cell, measured to the container's own box in its own frame
// (vr::DistanceToBox) or to the actor's bones less the flesh round them
// (vr::DistanceToBones), in game units. `withinUnits` bounds the search; an
// invalid result (ref 0) with nothing that near.
struct ReachTarget {
	UInt32 ref = 0;
	vr::ReachKind kind = vr::ReachKind::None;
	float distanceUnits = 0.0f;
	bool left = false;          // the left hand is the near one
	bool personInCombat = false;  // a living actor fighting
};
// Living actors only with `pockets` (the player sneaks), and not one in
// combat: a person to talk to is never reached for.
ReachTarget FindReachTarget(const ReachHands& hands, float withinUnits, bool pockets);

// The distance from the nearer usable hand to `ref`, measured as above.
// False when the reference cannot be read any more (`gone` set), or has
// nothing to measure.
bool ReachDistanceTo(UInt32 ref, const ReachHands& hands, float& units, bool& gone);

// Whether `ref` is locked: its ExtraLock (extra data 0x31, xOBSE
// GameExtraData.h: { lockLevel, key, flags }, bit 0 locked) read off its
// extra list (+0x44). False with none or when unreadable.
bool RefIsLocked(UInt32 ref);

// TESObjectREFR::Activate(player, 0, 0, 1) on `ref`, as the player's own
// activate control calls it (0x0067318A): a container opens its menu, a
// locked one its lockpicking, a dead actor its inventory, a living one -
// sneaking - its pockets. False when the player cannot be read.
bool ActivateByPlayer(UInt32 ref);

// The engine's close-all-menus (0x00579770, xOBSE's CloseAllMenus command
// calls it), verified by its first bytes before the first call. False when
// the bytes are not the ones read, and then nothing is called.
bool CloseMenus();

}  // namespace obvr::game
