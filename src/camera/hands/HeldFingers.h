#pragma once

// The holding hand's four fingers kept behind the tablet's page
// (vr/HeldFingers.h; docs/inventory-tablet-spec.md 32.2): each frame, after
// the hands are pinned and before game::StepHandFingers poses the fingers,
// both hands' curls as the controller says, the holding hand's four fingers
// each limited by the page placed this frame, each by its own skin as
// measured from the drawn hand (vr/FingerSkin.h, MeasureFingerSkin). Its
// bones are found once per first-person tree. The thumb and the tapping hand
// as they are. Harness: tablet-fingers-behind.txt.

#include <vector>

#include "game/GameTypes.h"
#include "game/HandGrip.h"
#include "vr/FingerSkin.h"
#include "vr/HandMode.h"
#include "vr/HeldFingers.h"

namespace obvr::camera::hands {

struct HeldFingersState {
	const NiAVObject* root = nullptr;  // the first-person tree the bones below were found in
	bool searched = false;
	// 0 the L bones (the left role's hand), 1 the R bones; the four fingers'
	// three links each, null where not found or not hung under the one before;
	// the thumb's three, null where not found.
	NiAVObject* hand[2] = {};
	NiAVObject* links[2][4][3] = {};
	NiAVObject* thumb[2][3] = {};
	vr::CurlSide prefer[4] = {};  // each finger's side, kept from frame to frame
	bool active = false;          // held behind the page this frame (the reach kept: vr::KeptReach)
	bool holderRight = false;
	// This frame's page and each finger's curl asked and allowed, for the skin's reading after the pose.
	vr::SheetInWorld sheet;
	vr::HeldFingersOutput out;
	float asked[4] = {};
	// Each finger's skin, the widest measured this tree, and the log's line of it.
	vr::FingerSkinKept skin;
	vr::SkinLineState skinLine;
	UInt32 linesLeft = 30;
};

// `rightTracked`, `leftTracked`: that hand's fingers follow the controller's
// (game::FingerPose::Tracked); `reachRead`: the holding hand's reach has been
// read (the page's first place taken with the fingers free).
void CurlsForFingers(HeldFingersState& state, const vr::HandModeResult& hand, bool rightTracked, bool leftTracked,
                     bool reachRead, const NiMatrix33& cameraRot, const NiPoint3& cameraPos, float unitsPerMetre,
                     game::FingerCurls& right, game::FingerCurls& left);

// After the fingers are posed, when the hand's vertices were read
// (camera/hands/HandReachRead.cpp): the holding hand's (`vertices`, the
// world's units) against its bones and this frame's page - each finger's skin
// kept for the limit, and the log's line of how deep each stands.
void MeasureFingerSkin(HeldFingersState& state, const std::vector<NiPoint3>& vertices, float unitsPerMetre);

}  // namespace obvr::camera::hands
