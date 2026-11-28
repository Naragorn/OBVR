#pragma once

// The hands' reach read for the tablet (vr/HandReach.h;
// docs/inventory-tablet-spec.md section 21): while the tablet is up, the
// first-person hands' vertices where the drawn pose has them
// (game::ReadHandVerticesNow) against each hand's palm and held-item node as
// the bones stand - read when vr::StepHandReachClock says so, eased each
// frame, said in the log. Called from StepIndexTips, right after the fingers
// are posed. Harness: tablet-on-hand.txt.

#include <vector>

#include "camera/hands/HeldFingers.h"
#include "vr/HandMode.h"
#include "vr/HandReach.h"

namespace obvr::camera::hands {

struct HandReachState {
	vr::HandReachClock clock;
	// The hands' vertices in the world, the right's then the left's
	// (game::ReadHandVerticesNow's order); kept for their room.
	std::vector<NiPoint3> vertices[2];
	vr::HandReach read[2];   // 0 the left role's, 1 the right's: the last reading
	vr::HandReach shown[2];  // eased: what the tablet is placed by
	// While the holding hand's fingers are held behind the page: its farthest
	// reading since (vr::KeptReach), so the page does not follow them back.
	vr::HandReach kept[2];
	// For the log: what was last said of each role's hand, the lines left;
	// and between two hand-script marks the reads and the step's own time.
	vr::HandReach said[2];
	UInt32 linesLeft = 40;
	UInt32 readsSinceMark = 0;
	UInt32 framesSinceMark = 0;
	double secondsSinceMark = 0.0;
	double worstSeconds = 0.0;
};

// Once a frame. `tabletUp`: the tablet's menus are up; `hand`: the fingers'
// curls and which hand holds the tablet; `unitsPerMetre`: the world's;
// `fingers`: whether the holding hand's fingers were held behind the page
// this frame (camera/hands/HeldFingers.h), and where their skin, measured at
// each read, is kept.
void StepHandReachRead(HandReachState& state, bool tabletUp, const vr::HandModeResult& hand, float unitsPerMetre,
                       const vr::HeldPlace& place, HeldFingersState& fingers);

}  // namespace obvr::camera::hands
