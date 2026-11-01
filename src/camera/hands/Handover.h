#pragma once

// A hand closing on what the other hand holds as an object (game/HandoverLogic.h,
// free-weapons-spec 5.14): equipped back when this hand can wear it - the
// other hand's hold let go first, the take once the engine's grab has let go
// (StepHandBack) - else passed to this hand. A thing the engine's grab holds
// by the ordinary grip is taken as the grabbing hand's. Off by default since
// 2026-11-01 ([Hands] HandoverReachMetres 0, docs/release-hardening.md 3).
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.6): the code as it was, its
// function statics now HandoverState's members. What it shares with the
// other hand features is in its signature (HandoverLinks): the inventory's
// held thing it may take over, the weapon take it may start, the pull it
// waits for, and handBackTaking, which the held object's stow reads.
// Harness: handover-off.txt (and handover-*.txt with the reach set).

#include "camera/hands/HandStep.h"
#include "game/HandoverLogic.h"
#include "game/InventoryGrabLogic.h"
#include "game/WeaponGrab.h"

namespace obvr::camera::hands {

// An item from the inventory dropped at the hand and held there (step 4,
// decision (b)): while that hand's hold stays closed its grab is wanted, as
// the grip's click wants it for a thing in the world; the hold opening lets
// it go as any held thing. (Moved here from CameraHook.cpp, unchanged.)
struct InventoryHold {
	UInt32 ref = 0;
	bool left = false;
	float seconds = 0.0f;
	bool grabbedSaid = false;
};

struct HandoverState {
	game::HandBackState handBack{};
	bool handBackLeft = false;  // the hand taking it back
	game::InventoryItemKind handBackKind = game::InventoryItemKind::Other;
	bool rightHoldWas = false;
	bool leftHoldWas = false;
};

// What the handover shares with the other hand features this frame.
struct HandoverLinks {
	InventoryHold& inventoryHold;              // the thing a hand holds as an object
	game::WeaponTakeState& weaponTake;         // a weapon's take, started for a hand-back
	game::WeaponHoldLatch& weaponHoldLatch;    // one take per closing of the hold
	bool weaponPullGrab;                       // the grab began as a weapon's pull
	bool weaponPullTaking;                     // a pulled weapon is being taken
	bool& handBackTaking;                      // out: a handed-over thing is being taken back
	// A taken reference forgotten everywhere that remembers it (the grab
	// physics' flights and waits, the stow's held thing): CameraHook's ForgetTaken.
	void (*forgetTaken)(UInt32 ref);
};

void StepHandover(HandoverState& state, const HandStep& step, const HandShared& shared, HandoverLinks& links);

}  // namespace obvr::camera::hands
