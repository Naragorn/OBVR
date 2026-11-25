#pragma once

// An arrow that sticks in the world, taken by the grip (a Quest 2 tester,
// 2026-11-25: "Arrows stuck in training targets / wood etc cannot be
// recovered as they cannot be grabbed ... training then becomes a real big
// expense").
//
// What it is in the engine [read in Oblivion.exe 1.2.416; the addresses in
// docs/holding-objects-spec.md, "Arrows that stick"]: a shot arrow stays an
// ArrowProjectile reference for its whole life - flying, stuck or bounced.
// Its base form is the ammo, it is in its cell's object list and has a scene
// node, so game::FindNearestItem finds it as any item. Its state is at +0x60
// (0 flying, 1 hit this frame, 2 at rest, 3 fading out) and what it hit at
// +0x5C, a record whose first word is the kind: 0 an actor, 1 another
// reference's body (a wall, a post, a target), 2 the land, 3 bounced off,
// 4 the flight ran out.
//
// Stuck in a body or the land its own Havok body stays keyframed, and the
// engine's grab refuses a keyframed body: a closed grip put the grab key down
// for nothing. The activate button is kept from loose items (vr/Stow.h,
// TakeOnlyByHand). So nothing took the arrow. Bounced or dropped it is
// dynamic clutter and the grab holds it like any item.
//
// The game itself takes a stuck arrow when it is activated: the pick-up
// sound, one arrow into the inventory, the reference gone, no owner asked.
// game::TakeIntoInventory is that call. So the grip closed with a stuck arrow
// in its reach - where it would grab or pull an item - takes the arrow, once
// per closing; that hand grabs nothing else until it opens.
//
// An arrow in a body (kind 0) is left alone: the hit puts every second one
// into that body's inventory already (iArrowInventoryChance), and a take of
// the reference would double it.
//
// Pure, covered by stuck_arrow_test; the read is game/StuckArrow.cpp's.

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

inline constexpr UInt32 kArrowProjectileVtable = 0x00A6F08C;
inline constexpr UInt32 kArrowImpactOffset = 0x5C;
inline constexpr UInt32 kArrowStateOffset = 0x60;

inline constexpr UInt32 kArrowStateFlying = 0;
inline constexpr UInt32 kArrowStateHit = 1;
inline constexpr UInt32 kArrowStateAtRest = 2;
inline constexpr UInt32 kArrowStateFading = 3;

inline constexpr UInt32 kArrowHitActor = 0;
inline constexpr UInt32 kArrowHitBody = 1;
inline constexpr UInt32 kArrowHitLand = 2;
inline constexpr UInt32 kArrowBounced = 3;
inline constexpr UInt32 kArrowFlightOver = 4;

// A reference read as an arrow projectile.
struct ArrowRead {
	bool isArrow = false;     // its vtable is ArrowProjectile's
	UInt32 state = 0;         // +0x60
	bool haveImpact = false;  // the hit record at +0x5C is there
	UInt32 kind = 0;          // its first word
};

// Whether the arrow sticks where the grab cannot hold it and the game's own
// activate takes it: at rest or fading, in another reference's body or in the
// land.
inline bool IsStuckArrow(const ArrowRead& a) {
	if (!a.isArrow || !a.haveImpact) {
		return false;
	}
	const bool resting = a.state == kArrowStateAtRest || a.state == kArrowStateFading;
	return resting && (a.kind == kArrowHitBody || a.kind == kArrowHitLand);
}

// The reference as an arrow projectile; isArrow false for anything else,
// with nothing more read from it.
ArrowRead ReadArrow(UInt32 ref);

// The arrow projectiles of the player's cell, up to `max` - for a hand
// script's mark, not for every frame (game/NearbyItems.cpp walks the cell).
struct ArrowSeen {
	UInt32 ref = 0;
	UInt32 formId = 0;
	NiPoint3 position{0.0f, 0.0f, 0.0f};
};
UInt32 ArrowsOfPlayerCell(ArrowSeen* out, UInt32 max);

// ------------------------------------------------------------- The take
struct ArrowTakeState {
	bool spent = false;  // this closing of the grip took an arrow
};

struct ArrowTakeInput {
	bool gripHeld = false;      // a grip is closed and may grab
	bool stuckInReach = false;  // at that hand a stuck arrow, where the grip would grab or pull an item
	bool free = false;          // in the world, alive, nothing in the engine's grab
};

struct ArrowTakeVerdict {
	bool takeNow = false;     // take it this frame
	bool blocksGrab = false;  // the hand that took an arrow grabs nothing until it opens
};

// One take per closing of the grip. A grip already closed that comes to the
// arrow takes it too, as it would grab an item; while the take may not run
// it waits, with nothing held back.
inline ArrowTakeVerdict StepArrowTake(ArrowTakeState& s, const ArrowTakeInput& in) {
	ArrowTakeVerdict v;
	if (!in.gripHeld) {
		s.spent = false;
		return v;
	}
	if (!s.spent && in.stuckInReach && in.free) {
		s.spent = true;
		v.takeNow = true;
	}
	v.blocksGrab = s.spent;
	return v;
}

}  // namespace obvr::game
