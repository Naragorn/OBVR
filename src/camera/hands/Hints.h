#pragma once

// Hints in play (vr/HintLogic.h; docs/onboarding-spec.md 3.3): a small sign
// over the hand that does the thing - the draw and the kick over the weapon
// hand, the shield bash over the shield hand, stowing and eating over the hand
// that holds, the door over the hand at its handle - shown when the moment fits, gone once the
// player has done it. What was shown and learned is kept in OBVR_Hints.ini
// beside the plugin, not in the save: a new character does not get them again,
// and OBVR.ini is not written for it. A harness run neither reads nor writes
// the file. Harness: hint-shield-bash.txt.

#include "camera/hands/HandStep.h"
#include "ui/CanvasOverlay.h"
#include "ui/HolsterSignPainter.h"
#include "vr/HintLogic.h"

namespace obvr::camera::hands {

constexpr float kHintSignMetres = 0.16f;
constexpr float kHintSignAboveMetres = 0.12f;

struct HintsState {
	vr::HintState logic;
	bool loaded = false;
	bool harness = false;  // a harness run: the file is left alone
	ui::CanvasOverlay signLayer{"obvr.hintsign", "OBVR Hint Sign", ui::kHolsterSignWidth, ui::kHolsterSignHeight};
	ui::HolsterSignView view;
	UInt32 revision = 1;
	SInt32 viewFor = -1;
	UInt32 lines = 24;
	// Deeds the host counts for the hints alone: parries and throws.
	UInt32 parries = 0;
	UInt32 throws = 0;
};

// What the hints read of the rest of the frame: whether each hint's moment
// fits, and what the player did.
struct HintLinks {
	bool wants[vr::kHintCount];
	bool did[vr::kHintCount];
	// The hand the sign hangs over: 0 by its role (vr::HintOnWeaponHand), 1
	// the physical left, 2 the physical right - the hand that holds or reaches.
	UInt8 hand[vr::kHintCount];
};

// Whether any hint can still show: when none can, the host need not look for
// their moments (the combat and shield reads).
bool AnyHintOpen(HintsState& state, const Config& config);

void StepHintSigns(HintsState& state, const HandStep& step, const vr::HandModeFrame& frame, const HintLinks& links);

void HideHintSign(HintsState& state, vr::OpenVRBackend& backend);

// The settings' "Show the hints again": at the next step every hint is new
// (shown 0, not learned) and OBVR_Hints.ini says so.
void RequestHintsReset();

}  // namespace obvr::camera::hands
