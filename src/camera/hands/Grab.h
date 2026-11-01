#pragma once

// Reaching for things (game/WeaponGrab.h, game/TorchGrab.h, game/InventoryGrab.h;
// free-weapons-spec section 5):
// - the item a hand reaches for (game::FindNearestItem, held steady by
//   game::StepPickHold), the pick aimed from that hand at it;
// - the grab reach (vr::StepGrabReach): a grip closing on what is in reach,
//   the grab key, and where the held object goes (the palm's point);
// - a weapon or a torch at its grip: taken, equipped and drawn, or pulled
//   from afar and equipped on arrival;
// - the inventory's item taken into the hand whose hold closes there.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.8): the 700 lines as they
// were, the weapon grab's file globals now GrabState's members under the
// same names (the release, the hand-back and the camera pass reach them as
// g_grab.<name>); what the step hands on - the grab key, the reach's target
// and the hold point - comes back in GrabOutput under the names it had.
// Harness: take-*.txt, weapon-grab*.txt, torch-take*.txt, inventory-grab*.txt,
// holster-pull.txt, reach*.txt.

#include "camera/hands/Handover.h"
#include "camera/hands/HandStep.h"
#include "game/DropRotationLogic.h"
#include "game/GrappleLogic.h"
#include "game/InventoryGrabLogic.h"
#include "game/NearbyItems.h"
#include "game/PickHold.h"
#include "game/TorchGrab.h"
#include "game/WeaponGrab.h"
#include "game/WeaponTimeline.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {

// The weapon the pick holds, at its grip (game/WeaponGrab.h), as read this
// frame: the ring sits on the grip, and the weapon hand's hold closing there
// takes the weapon.
struct WeaponPick {
	bool valid = false;
	UInt32 ref = 0;
	SInt32 type = -1;
	bool torch = false;  // a torch, the other hand's (game/TorchGrab.h)
	NiPoint3 gripPoint{0.0f, 0.0f, 0.0f};
	float toGripUnits = -1.0f;
};

// The inventory's take waiting for the first world frame after the close.
struct InventoryTakePending {
	game::InventoryTake take = game::InventoryTake::Nothing;
	UInt32 form = 0;
	SInt32 weaponType = -1;
	bool left = false;  // the hand that closed: the other hand (the roles)
	float wait = 0.0f;
};

struct GrabState {
	// Held steady across frames (game/PickHold.h): the choice, the hand, and
	// the points the ring, the info row and the pick's aim follow.
	game::PickHoldState pickHold;
	UInt32 pickLines = 40;
	WeaponPick weaponPick;
	// A torch taken at its grip or pulled in by the other hand: its own take,
	// latch and pull beside the weapon hand's.
	game::TorchTakeState torchTake;
	game::WeaponHoldLatch torchHoldLatch;
	game::WeaponPullState torchPull;
	bool torchPullGrab = false;    // the current grab began as a torch's pull by the other hand
	bool torchPalmToSay = false;   // a taken torch: say once where the palm sits on it
	game::WeaponTakeState weaponTake;
	// The inventory's grab: the hold's edge in the inventory, and the take
	// waiting for the first world frame after the close.
	game::InventoryGrabTrigger inventoryTrigger;
	InventoryTakePending inventoryTake;
	UInt32 takeFrameLines = 60;    // the straight take's frames, logged
	game::WeaponHoldLatch weaponHoldLatch;  // one take per closing of the hold
	game::WeaponPullState weaponPull;       // a pulled weapon equipped on arrival
	bool weaponPullGrab = false;            // the current grab began as a weapon's pull
	bool weaponHoldLeft = false;            // the hand the current grab is with
	bool weaponPullTaking = false;          // the grab physics stands back while a pulled weapon is taken
	float sinceGrabKey = 10.0f;             // seconds since the grab key last went down
	bool weaponTakeReadyClick = false;      // the draw's click, for the next frame's hand mode step
	UInt32 weaponGrabLinesLeft = 60;
	vr::GrabReachState grabReach;
	bool grabReachPick = false;
	bool grabKeyDown = false;
	// The take's timeline (game/WeaponTimeline.h): from a pulled weapon's
	// arrival or the take at the grip until it hangs on the hand.
	game::WeaponTimeline takeTimeline;
	bool takeTimelineLeft = false;
	// The reaching's function statics, as they were.
	bool npcSaid = false;
	UInt32 weaponPickRef = 0;
	game::WeaponGrabChoice lastWeaponChoice = game::WeaponGrabChoice::NotAWeapon;
	game::TorchTakeGate torchGate = game::TorchTakeGate::Off;
	bool keyWas = false;
};

// What the reaching shares with the rest of the frame.
struct GrabLinks {
	InventoryHold& inventoryHold;     // the thing a hand holds as an object
	game::NearItem& nearItem;         // the item nearest a hand (the camera pass aims the pick at it)
	const game::GrappleState& grapple;
	bool slapTapPressed;              // the mod's slap holds the grab key down this frame
	bool rolesSwapped;                // the hands' roles swapped (left-handed)
	float deltaSeconds;               // the frame's (g_deltaSeconds)
	bool holsterFitting;              // the weapon places' fit runs: its grips are its own
	void (*forgetTaken)(UInt32 ref);
	bool (*heldItemPoseNow)(int side, const char* attachName, game::ItemPose& out);
};

// What the frame after the reaching reads, under the names it had.
struct GrabOutput {
	vr::GrabReachVerdict reach;
	UInt32 reachRef = 0;
	bool haveHoldPoint = false;
	NiPoint3 holdPoint{0.0f, 0.0f, 0.0f};
};

GrabOutput StepGrab(GrabState& state, const HandStep& step, const HandShared& shared, vr::HandModeFrame& frame,
                    GrabLinks& links);

}  // namespace obvr::camera::hands
