#pragma once

// The kick on the weapon hand's B (game/KickLogic.h; Nadi, 2026-10-30: "go
// B"): the foot seen, driven by the leg IK (game/BodyIk.h), its sweep
// against the living's capsules - staggered standing, knocked down running
// or in a jump, the engine's fist blow for the damage - and the ring over
// the other controller while its B is held toward the pause menu
// (vr::StepButtonHold, ui/HoldRingPainter.h).
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.12): the code as it was, its
// four file globals and nine statics now KickHoldState's members. Harness:
// kick.txt, kick-hit.txt, kick-damage.txt.

#include "camera/hands/HandStep.h"
#include "game/BladeBodies.h"
#include "game/KickLogic.h"
#include "ui/CanvasOverlay.h"
#include "ui/HoldRingPainter.h"

namespace obvr::camera::hands {

constexpr float kMenuHoldAboveMetres = 0.08f;

struct KickHoldState {
	game::KickState kick;
	// The ring over the left controller while its B is held.
	ui::CanvasOverlay menuHoldLayer{"obvr.menuhold", "OBVR Menu Hold", ui::kHoldRingCanvas, ui::kHoldRingCanvas};
	ui::HoldRingView menuHoldView;
	UInt32 menuHoldRevision = 1;
	// The block's function statics, as they were.
	UInt32 kickLines = 24;
	SInt32 bodyStateWas = -2;
	UInt32 bodyStateLines = 12;
	NiPoint3 footBefore{0.0f, 0.0f, 0.0f};
	bool footBeforeValid = false;
	game::BladeBodies kickBodies{};
	game::KickMet kickMet{};
	UInt32 kickLanded = 0;  // the kick whose fatigue was paid
	UInt32 nearSaid = 0;
};

// What the kick reads of the rest of the frame.
struct KickLinks {
	bool exhausted;     // the player is exhausted (game::ExhaustionState)
	bool rolesSwapped;  // the hands' roles swapped (left-handed): the left leg kicks
};

void StepKick(KickHoldState& state, const HandStep& step, const HandShared& shared, const vr::HandModeFrame& frame,
              const KickLinks& links);

}  // namespace obvr::camera::hands
