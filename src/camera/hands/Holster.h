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
#include "camera/hands/PotionBelt.h"
#include "ui/CanvasOverlay.h"
#include "ui/GuidePanel.h"
#include "ui/HolsterSignPainter.h"
#include "vr/Holster.h"
#include "ui/StowSpotPainter.h"
#include "vr/HolsterFit.h"
#include "vr/HolsterPlace.h"
#include "vr/HolsterSign.h"

namespace obvr::camera::hands {

struct HolsterState {
	UInt32 draws = 0;                // reaches that drew a weapon, for the hints (camera/hands/Hints.h)
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

	// The places as gold circles to drag (vr/HolsterPlace.h), from the
	// settings' window or the first start's page: one overlay a circle.
	vr::HolsterPlaceState place;
	ui::CanvasOverlay placeLayers[vr::kHolsterPlaceCount]{
		{"obvr.holsterplace.hipleft", "OBVR Weapon Place Left Hip", ui::kStowSpotCanvas, ui::kStowSpotCanvas},
		{"obvr.holsterplace.hipright", "OBVR Weapon Place Right Hip", ui::kStowSpotCanvas, ui::kStowSpotCanvas},
		{"obvr.holsterplace.backleft", "OBVR Weapon Place Left Back", ui::kStowSpotCanvas, ui::kStowSpotCanvas},
		{"obvr.holsterplace.backright", "OBVR Weapon Place Right Back", ui::kStowSpotCanvas, ui::kStowSpotCanvas},
	};
	ui::StowSpotView placeViews[vr::kHolsterPlaceCount];
	UInt32 placeRevision = 1;
	// The right hip's circle in the room as the circles came up: what a mark
	// measures its move from (the harness, weapon-places-look.txt).
	NiPoint3 placeUpRightHip{0.0f, 0.0f, 0.0f};
	bool placeSaidUp = false;
	// The same circles shown faintly while the player looks down
	// (camera/hands/HolsterGlance.h): their opacity now.
	float glanceAlpha = 0.0f;
	UInt32 glanceLines = 6;

	// The potions kept at the same places (camera/hands/PotionBelt.h).
	PotionBeltState potionBelt;
};

// What the reach to a place did this frame (the hand mode's HolsterVerdict),
// carried out and logged. `equippedWeaponForm`: the weapon slot's form this
// frame (null: none). `rolesSwapped`: the hands' roles swapped (left-handed).
void StepHolsterReach(HolsterState& state, const HandShared& shared, const vr::HandModeFrame& frame,
                      UInt8* equippedWeaponForm, bool rolesSwapped);

// The reaches at the places this frame: the holster's (StepHolsterReach),
// then the potions kept there (camera/hands/PotionBelt.h), which drop into
// `hold`.
void StepHolsterReaches(HolsterState& state, const HandStep& step, const HandShared& shared,
                        const vr::HandModeFrame& frame, UInt8* equippedWeaponForm, bool rolesSwapped,
                        InventoryHold& hold);

// The places' sign this frame. `deltaSeconds`: the frame's (g_deltaSeconds),
// as the sign always timed itself.
void StepHolsterSign(HolsterState& state, const HandStep& step, const HandShared& shared,
                     const vr::HandModeFrame& frame, float deltaSeconds);

// Steps the fit of the weapon places. True while it runs: the triggers and
// menu buttons are its, and nothing else of the hands reaches the game.
// `config` is the live one: the fit saves the places it measured.
bool UpdateHolsterFit(HolsterState& state, Config& config, vr::OpenVRBackend& backend,
                      const vr::HandModeFrame& frame, bool allowed);

// Steps the places' circles (vr::StepHolsterPlace) with the window's
// commands, draws them while they are up and saves what Keep keeps. Called
// by UpdateHolsterFit every frame, in the world or the main menu alike.
// The frame's head and hands as SteamVR gives them - or the hand script's
// ([Debug] HandScript), placed from this frame's head, then into the pose
// recording (test/PoseRecorder.h) - and the body's frame the holster places
// live in (vr/BodyFrame.h): the torso's heading by the drawn body's own rule
// ([Body] TorsoFollow*), held while the circles are up.
void ReadFrameHeadAndHands(vr::HandModeFrame& frame, vr::OpenVRBackend& backend, const Config& config, float dt,
                           bool scriptLive);

void UpdateHolsterPlacing(HolsterState& state, Config& config, vr::OpenVRBackend& backend,
                          const vr::HandModeFrame& frame, bool allowed);

}  // namespace obvr::camera::hands
