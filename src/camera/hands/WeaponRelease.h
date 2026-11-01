#pragma once

// Letting go of what a hand carries (game/WeaponRelease.h, game/TorchRelease.h;
// free-weapons-spec section 5, decisions 3 and 10): the holding hand's hold
// closing on the drawn weapon arms it, the hold opening past the release time
// lets it go - sheathed in a holster zone, unequipped at the chest, dropped at
// the hand with the hand's speed elsewhere; the bow is the other hand's. The
// other hand's torch likewise, dropped burning (game/TorchFire.h). The
// dropped thing watched for its body, its flight and turn logged, the take's
// and the release's timelines (game/WeaponTimeline.h) stepped.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 steps 2.8 and 2.9 for this block):
// the code as it was, the twenty file globals only it used now
// ReleaseState's members under the same names; everything it shares with
// the rest of the hand features named in ReleaseLinks.
// Harness: weapon-drop*.txt, weapon-release*.txt, torch-drop*.txt.

#include "camera/hands/Handover.h"
#include "camera/hands/HandStep.h"
#include "camera/hands/Holster.h"
#include "game/FlameStateLogic.h"
#include "game/GrappleLogic.h"
#include "game/DropRotationLogic.h"
#include "game/TorchRelease.h"
#include "game/WeaponGrab.h"
#include "game/WeaponRelease.h"
#include "game/WeaponTimeline.h"
#include "vr/HalfSwordLogic.h"

namespace obvr::camera::hands {

// The dropped weapon's first frames after its send-off: how fast its model
// moves, against what OBVR gave it (Nadi, Windows, 2026-10-24: "Waffe
// loslassen ist zu viel Force" with every drop sent off at 0.00 m/s).
struct DroppedFlight {
	UInt32 ref = 0;
	float given = 0.0f;  // units a second
	bool haveLast = false;
	NiPoint3 last{0.0f, 0.0f, 0.0f};
	NiPoint3 first{0.0f, 0.0f, 0.0f};
	UInt32 count = 0;
	float speeds[12] = {};
};

// The turn a dropped item was given, checked again when its body comes.
struct DropTurnWatch {
	UInt32 ref = 0;
	NiMatrix33 rot = NiMatrix33::Identity();
};

struct ReleaseState {
	// The drawn weapon let go of: the hold's closing and opening, the hand's
	// speed kept while armed, the dropped reference watched for its body.
	game::WeaponReleaseState weaponRelease;
	DroppedFlight droppedFlight;
	UInt32 droppedFlightLines = 12;
	// The other hand's torch let go of, its drop watched for the body as the
	// weapon's, and the fire put on once its 3D is there: read before and
	// after, and again a second later.
	game::TorchReleaseState torchRelease;
	game::DroppedWeaponState droppedTorch;
	UInt32 torchFireRef = 0;
	UInt32 torchFireFrames = 0;
	bool torchFireLit = false;
	UInt32 torchLines = 40;
	UInt32 torchFireLitAt = 0;
	// The player's cell walked every 30 frames for the torches OBVR lit
	// (game/TorchGroundLogic.h).
	UInt32 litTorchFrames = 0;
	UInt32 litTorchLines = 30;
	NiPoint3 droppedAt{0.0f, 0.0f, 0.0f};  // where the hand was at the drop, world units
	UInt32 weaponReleaseLinesLeft = 60;
	// The release's timeline: from the hold reading open until the weapon is
	// off the hand and the dropped one is in the world.
	game::WeaponTimeline releaseTimeline;
	bool releaseTimelineLeft = false;
	UInt32 timelineLinesLeft = 24;
	UInt32 heldOnLinesLeft = 12;
	DropTurnWatch weaponDropTurn;
	DropTurnWatch torchDropTurn;
	// The block's function statics, as they were (half-swording's two-handed
	// torch log, the torch take-back probe, the lit torches' cell walk, the
	// weapon node trace).
	int twoHandTorch = -1;
	UInt32 takeRef = 0;
	float takeIn = 0.0f;
	UInt32 cell = 0;
	UInt32 ours = 0xFFFFFFFF;
	game::HungWatch hung[2]{};
	game::HungWatch hungState{};
	UInt32 traceLines = 120;
};

// What the release shares with the other hand features this frame.
struct ReleaseLinks {
	HolsterState& holster;                   // its lines budgets
	InventoryHold& inventoryHold;            // a thing let go into the other hand's hold
	game::DroppedWeaponState& droppedWeapon; // the dropped weapon, watched (the world push reads it)
	game::WeaponTakeState& weaponTake;       // no release while a take runs
	game::WeaponPullState& weaponPull;       // nor while a pull runs
	game::WeaponTimeline& takeTimeline;      // the take's timeline, stepped here
	bool& takeTimelineLeft;
	bool& weaponTakeReadyClick;              // out: the draw's click for the next frame
	bool& torchJustEquipped;                 // in and cleared: a torch newly equipped this frame
	const vr::RightHoldState& rightHold;     // half-swording: the weapon held by the other hand
	const vr::RightHoldInput& rightHoldIn;
	const game::GrappleState& grapple;       // the other hand holds someone
	bool leftGripOnHandle;                   // the other hand on a two-hander's handle
	bool drawnWeaponValid;                   // the drawn weapon's span this frame
	NiPoint3 drawnWeaponGrip;
	NiPoint3 drawnWeaponTip;
	UInt8* equippedWeaponForm;               // the weapon slot's form (null: none)
	float deltaSeconds;                      // the frame's (g_deltaSeconds)
	bool holsterFitting;                     // the weapon places' fit runs: its grips are its own
	void (*forgetTaken)(UInt32 ref);
	bool (*heldItemPoseNow)(int side, const char* attachName, game::ItemPose& out);
	void (*logTorchShapeStates)(const char* what, UInt32 ref, const game::FlameShapeStates& s);
	void (*logTakeTimeline)(const game::WeaponTimeline& t, game::TimelineEnd end);
	void (*logReleaseTimeline)(const game::WeaponTimeline& t, game::TimelineEnd end);
};

void StepRelease(ReleaseState& state, const HandStep& step, const HandShared& shared,
                 const vr::HandModeFrame& frame, ReleaseLinks& links);

}  // namespace obvr::camera::hands
