#pragma once

// What the hands hold, read for the frame: the weapon slot and its kind, the
// weapon kept at a place when it had none (vr::PlaceForUnkept), the torch
// beside a two-hander, the drawn weapon's weight for its lag
// (vr/WeaponWeight.h, logged once per draw), the other hand's shield or
// torch and the places' items, a torch newly equipped for the torch's grace,
// and a torch come into the hand watched for its flame's shapes.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.18): the code as it was, its
// four file globals and five statics now WeaponFrameState's members (the
// body, the release and the contacts read the weapon's form and kind as
// g_weaponFrame.<name>). Harness: holster-places.txt, weapon-weight*.txt,
// torch-*.txt, handover-shield.txt.

#include "camera/hands/HandStep.h"
#include "camera/hands/Holster.h"
#include "game/FlameStateLogic.h"
#include "game/WeaponGrab.h"

namespace obvr::camera::hands {

struct WeaponFrameState {
	// The weapon slot as read this frame: its kind for the body, its form for
	// the release, the contacts and the places.
	vr::EquippedKind equippedForBody = vr::EquippedKind::Nothing;
	UInt8* equippedWeaponForm = nullptr;
	// A torch newly equipped this frame (in and cleared by the release, for
	// the torch's grace), and the torch in the hand watched for its shapes.
	bool torchJustEquipped = false;
	UInt32 torchWatch = 0;
	// The block's function statics, as they were.
	const UInt8* weighed = nullptr;  // the weapon whose weight was logged
	bool loadGap = true;             // a load since the torch was last seen
	const UInt8* torchSeen = nullptr;
	const UInt8* shieldWas = nullptr;
	const UInt8* torchWas = nullptr;
};

// What the step shares with the rest of the frame.
struct WeaponFrameLinks {
	float exhaustionEase;    // the exhaustion's ease and factor (StepExhaustion), into the frame
	float exhaustionFactor;
	HolsterState& holster;   // its lines budgets
	void (*logTorchShapeStates)(const char* what, UInt32 ref, const game::FlameShapeStates& s);
};

// Fills the frame's weapon and off-hand fields.
void StepWeaponFrame(WeaponFrameState& state, const HandStep& step, vr::HandModeFrame& frame,
                     const WeaponFrameLinks& links);

}  // namespace obvr::camera::hands
