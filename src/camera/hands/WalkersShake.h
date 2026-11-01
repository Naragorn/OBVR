#pragma once

// Oblivion Walkers' grab on the body (vr/WalkersShake.h; Nadi, 2026-10-30:
// "go for vr shake, mit ui einblendung dass der user controller shaken muss
// wie in saints and sinners"): each held arm's hand shows the call, jolts at
// the grab, counts its strokes for Walkers; a bite jolts it harder and
// flashes the view red; the stick does not walk while held.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.14): the code as it was, its
// five file globals and ten statics now WalkersShakeState's members; the
// overlay pass reads the bite's flash from here. Harness:
// walkers-shake.txt, walkers-shake-e2e.txt.

#include "camera/hands/HandStep.h"
#include "ui/CanvasOverlay.h"
#include "ui/HolsterSignPainter.h"
#include "vr/WalkersShake.h"

namespace obvr::camera::hands {

constexpr float kShakeSignMetres = 0.16f;
constexpr float kShakeSignAboveMetres = 0.12f;

struct WalkersShakeState {
	// "Shake it off!" at a hand a walker holds, one per arm: the left arm's
	// first, the right arm's second.
	ui::CanvasOverlay signLayers[2] = {
		ui::CanvasOverlay("obvr.shakesign.left", "OBVR Shake Sign Left", ui::kHolsterSignWidth, ui::kHolsterSignHeight),
		ui::CanvasOverlay("obvr.shakesign.right", "OBVR Shake Sign Right", ui::kHolsterSignWidth,
		                  ui::kHolsterSignHeight)};
	ui::HolsterSignView signViews[2];
	UInt32 signRevisions[2] = {1, 1};
	// The red flash at a walker's bite (vr::BiteFlashAlpha), drawn by the
	// overlay pass.
	float biteFlashAlpha = 0.0f;
	bool biteFlashLethal = false;
	// The block's function statics, as they were.
	vr::ShakeStrokeState strokes[2]{};
	UInt32 holders[2] = {0, 0};
	UInt32 strokeCount[2] = {0, 0};
	UInt32 biteSerial = 0;
	UInt32 bites[2] = {0, 0};
	UInt32 toDie = 0;
	float biteSince = -1.0f;
	UInt32 lines = 60;
	UInt32 vrAsks = 3;
	bool lockSaid = false;
};

// What the shake reads of the rest of the frame.
struct WalkersShakeLinks {
	bool rolesSwapped;  // the hands' roles swapped (left-handed)
};

void StepWalkersShake(WalkersShakeState& state, const HandStep& step, const HandShared& shared,
                      const vr::HandModeFrame& frame, const WalkersShakeLinks& links);

}  // namespace obvr::camera::hands
