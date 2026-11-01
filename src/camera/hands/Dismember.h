#pragma once

// The head comes off (game/DismemberLogic.h, game/Dismember.h; Nadi,
// 2026-10-30: "go"): the drawn blade's sweep since the frame before through
// a dead body's neck, with an edge and fast enough; the killing blow through
// a living neck, waiting for the engine's death; the wrist the blade takes;
// the hand script's test cuts at its marks.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.13): the code as it was, its
// ten statics now DismemberState's members. Harness: dismember-*.txt.

#include "camera/hands/HandStep.h"
#include "game/Dismember.h"
#include "game/DismemberLogic.h"

namespace obvr::camera::hands {

struct DismemberState {
	// The block's function statics, as they were.
	NiPoint3 gripBefore{0.0f, 0.0f, 0.0f};
	NiPoint3 tipBefore{0.0f, 0.0f, 0.0f};
	bool haveBefore = false;
	UInt32 cutLines = 30;
	UInt32 lastActor = 0;
	game::CutVerdict lastVerdict = game::CutVerdict::Off;
	game::PendingCut killingBlow{};
	game::NeckOf necksBefore[8]{};  // the necks as they stood the frame before
	UInt32 necksBeforeCount = 0;
	UInt32 wristLines = 16;
};

// What the cuts read of the rest of the frame.
struct DismemberLinks {
	bool rolesSwapped;      // the hands' roles swapped (left-handed): the pulse goes to the other hand
	bool drawnWeaponValid;  // the drawn weapon's span this frame (CameraHook's g_drawnWeapon*)
	NiPoint3 drawnWeaponGrip;
	NiPoint3 drawnWeaponTip;
};

void StepDismember(DismemberState& state, const HandStep& step, const HandShared& shared,
                   const vr::HandModeFrame& frame, const DismemberLinks& links);

}  // namespace obvr::camera::hands
