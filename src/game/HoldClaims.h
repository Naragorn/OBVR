#pragma once

// Who holds a fighter. The off hand's standing grab (GrappleLogic.h) and a
// stuck blade (BladeStickLogic.h) both hold someone the same way: their legs
// stood still (game::HoldStill) and their blows stopped (game::SetHeldFoe),
// one slot each in the game. Each claims him in its own right here, and he
// is let go only when neither does - holding with the left and stabbing
// with the right, drawing the blade out leaves him in the grab, and opening
// the left leaves him on the blade (before 2026-11-08 whichever let go first
// freed him for both).
//
// One fighter at a time, as the slots are: the grab, which places him at
// the hand each frame, wins the slot; a blade stuck in someone else while
// the grab holds another is refused. Pure; hold_claims_test.

#include "core/Types.h"

namespace obvr::game {

enum class HoldOwner : UInt8 { Grab, Blade };

struct HoldClaim {
	UInt32 actor = 0;
	bool blows = false;  // this holder stops their blows
};

struct HoldClaims {
	HoldClaim grab;
	HoldClaim blade;
};

inline HoldClaim& ClaimOf(HoldClaims& c, HoldOwner o) { return o == HoldOwner::Grab ? c.grab : c.blade; }

// A holder lets go; the other's claim stands.
inline void Release(HoldClaims& c, HoldOwner o) { ClaimOf(c, o) = HoldClaim{}; }

// A holder claims `actor` (0: lets go). False when refused: a blade on
// someone other than the one the grab holds.
inline bool Claim(HoldClaims& c, HoldOwner o, UInt32 actor, bool blows) {
	if (actor == 0) {
		Release(c, o);
		return false;
	}
	if (o == HoldOwner::Blade && c.grab.actor != 0 && c.grab.actor != actor) {
		return false;
	}
	HoldClaim& mine = ClaimOf(c, o);
	mine.actor = actor;
	mine.blows = blows;
	return true;
}

// What the game's two slots should hold: the legs of the grab's fighter, or
// else the blade's; the blows stopped when any holder of that fighter stops
// them.
struct HoldEffect {
	UInt32 still = 0;
	UInt32 foe = 0;
};

inline HoldEffect HoldEffectOf(const HoldClaims& c) {
	HoldEffect e;
	e.still = c.grab.actor != 0 ? c.grab.actor : c.blade.actor;
	if (e.still == 0) {
		return e;
	}
	const bool grabStops = c.grab.actor == e.still && c.grab.blows;
	const bool bladeStops = c.blade.actor == e.still && c.blade.blows;
	e.foe = grabStops || bladeStops ? e.still : 0;
	return e;
}

}  // namespace obvr::game

namespace obvr::game {

// In the game (HoldClaims.cpp): the claim taken or let go, and the effect
// handed to game::HoldStill and game::SetHeldFoe; a release that leaves the
// fighter held by the other said ("Hold: still held - by the grab").
bool ClaimHold(HoldOwner owner, UInt32 actor, bool blows);
void ReleaseHold(HoldOwner owner);
// The actor a holder holds now, 0 none.
UInt32 HeldBy(HoldOwner owner);

}  // namespace obvr::game
