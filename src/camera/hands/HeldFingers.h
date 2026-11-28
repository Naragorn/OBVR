#pragma once

// The holding hand's four fingers kept behind the tablet's page
// (vr/HeldFingers.h; docs/inventory-tablet-spec.md 32.2): each frame, after
// the hands are pinned and before game::StepHandFingers poses the fingers,
// both hands' curls as the controller says, the holding hand's four fingers
// each limited by the page placed this frame. Its bones are found once per
// first-person tree. The thumb and the tapping hand as they are.
// Harness: tablet-fingers-behind.txt.

#include "game/GameTypes.h"
#include "game/HandGrip.h"
#include "vr/HandMode.h"
#include "vr/HeldFingers.h"

namespace obvr::camera::hands {

struct HeldFingersState {
	const NiAVObject* root = nullptr;  // the first-person tree the bones below were found in
	bool searched = false;
	// 0 the L bones (the left role's hand), 1 the R bones; the four fingers'
	// three links each, null where not found or not hung under the one before.
	NiAVObject* hand[2] = {};
	NiAVObject* links[2][4][3] = {};
	vr::CurlSide prefer[4] = {};  // each finger's side, kept from frame to frame
	bool active = false;          // held behind the page this frame (the reach kept: vr::KeptReach)
	UInt8 heldMask = 0;           // the fingers held at the last line, for the log
	UInt32 linesLeft = 30;
};

// `rightTracked`, `leftTracked`: that hand's fingers follow the controller's
// (game::FingerPose::Tracked); `reachRead`: the holding hand's reach has been
// read (the page's first place taken with the fingers free).
void CurlsForFingers(HeldFingersState& state, const vr::HandModeResult& hand, bool rightTracked, bool leftTracked,
                     bool reachRead, const NiMatrix33& cameraRot, const NiPoint3& cameraPos, float unitsPerMetre,
                     game::FingerCurls& right, game::FingerCurls& left);

}  // namespace obvr::camera::hands
