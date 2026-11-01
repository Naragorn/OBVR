#pragma once

// The holster places, as in Blade & Sorcery (vr/Holster.h; docs/controls-spec.md):
// - what a reach to a hip or a shoulder did (the hand mode's HolsterVerdict
//   carried out on the engine: a kept weapon equipped, a drawn one kept, the
//   other hand's shield or torch stowed or taken back) and the fists' log;
// - the places' sign low in the view (ui/HolsterSignPainter.h): what a place
//   keeps and the hint after a reach with the wrong hand;
// - the guided fit of the places (vr::StepHolsterFit) with its panel.
//
// Moved out of camera/CameraHook.cpp on 2026-11-01 (docs/god-functions-plan.md,
// phase 2 step 2.5): the code as it was, its file globals and function
// statics now HolsterState's members. The stow at the chest is not here: it
// is woven into the held object's step and moves with it (step 2.7).
// Harness: holster*.txt, holster-fit.txt, holster-sign*.txt, fist*.txt.

#include "camera/hands/HandStep.h"
#include "ui/CanvasOverlay.h"
#include "ui/GuidePanel.h"
#include "ui/HolsterSignPainter.h"
#include "vr/Holster.h"
#include "vr/HolsterFit.h"
#include "vr/HolsterSign.h"

namespace obvr::camera::hands {

struct HolsterState {
	UInt32 linesLeft = 30;          // the reaches', the fists' and the kept-at-no-place lines
	UInt32 offHandLinesLeft = 40;   // the other hand's shield and torch at a place
	// The grips that closed at no place (HolsterVerdict::rightMissed): their
	// own budget, so a session of misses does not spend the reaches' lines.
	UInt32 missLinesLeft = 12;

	// The places' sign: what it says, and the hint after a reach that did
	// nothing there - its place, its hand and how long it stays.
	ui::CanvasOverlay signLayer{"obvr.holstersign", "OBVR Holster Sign", ui::kHolsterSignWidth,
	                            ui::kHolsterSignHeight};
	ui::HolsterSignView signView;
	UInt32 signRevision = 1;
	vr::HolsterSlot hintSlot = vr::HolsterSlot::None;
	bool hintLeft = false;
	float hintSeconds = 0.0f;
	char hint[48] = {};
	vr::SignTimingState signTiming;
	NiPoint3 signAt{0.0f, 0.0f, 0.0f};
	UInt32 signLines = 30;
	UInt32 signSaidRevision = 0;

	// The guided fit of the weapon places, asked for by the settings row or a
	// hand script, with its instruction panel.
	vr::HolsterFitState fit;
	bool fitRequested = false;
	ui::CanvasOverlay fitLayer{"obvr.holsterfit", "OBVR Weapon Places", ui::kGuidePanelWidth, ui::kGuidePanelHeight};
	ui::GuidePanelText fitText;
	UInt32 fitRevision = 1;
	vr::openvr::HmdMatrix34 fitPose{};
	bool fitWasLeftB = false;
	bool fitWasRightB = false;
};

// What the reach to a place did this frame (the hand mode's HolsterVerdict),
// carried out and logged. `equippedWeaponForm`: the weapon slot's form this
// frame (null: none). `rolesSwapped`: the hands' roles swapped (left-handed).
void StepHolsterReach(HolsterState& state, const HandShared& shared, const vr::HandModeFrame& frame,
                      UInt8* equippedWeaponForm, bool rolesSwapped);

// The places' sign this frame. `deltaSeconds`: the frame's (g_deltaSeconds),
// as the sign always timed itself.
void StepHolsterSign(HolsterState& state, const HandStep& step, const HandShared& shared,
                     const vr::HandModeFrame& frame, float deltaSeconds);

// Steps the fit of the weapon places. True while it runs: the triggers and
// menu buttons are its, and nothing else of the hands reaches the game.
// `config` is the live one: the fit saves the places it measured.
bool UpdateHolsterFit(HolsterState& state, Config& config, vr::OpenVRBackend& backend,
                      const vr::HandModeFrame& frame, bool allowed);

}  // namespace obvr::camera::hands
