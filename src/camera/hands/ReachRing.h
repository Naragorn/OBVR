#pragma once

// The reach marker ([Hands] ReachMarker): a light-brown ring on the object
// under the pick when it is within ReachMarkerMetres of either hand - the
// object a closed grip would reach for. The pick is the laser's with the
// grip open and the hand's while reaching; either way what it found is
// asked. The ring sits on the thing the pick has settled on and eases over
// it (game/PickHold.h); the crosshair's icon moves into its middle, and the
// thing itself is outlined or glows (game/TargetMarker.h).
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.16): the code as it was, its
// seven file globals and two statics now ReachRingState's members; the
// laser's filter, the Target row and the overlay pass read the ring as
// g_reachRing.<name>. Harness: reach-name.txt, ring-*.txt.

#include "camera/SettledRef.h"
#include "camera/hands/HandStep.h"
#include "game/NearbyItems.h"
#include "render/ReachMarker.h"

namespace obvr::camera::hands {

struct ReachRingState {
	game::AnchorState ringAnchor;  // the ring's eased place over the thing
	SettledRef ringSettle;         // the thing the pick has settled on
	render::ReachMarker marker;
	// Whether the ring shows this frame and where: the crosshair's icon hangs
	// in it.
	bool iconShown = false;
	vr::openvr::HmdMatrix34 iconPose{};
	// The ring itself this frame and the thing it is on, for the name's wait
	// (the Target row's "the ring without its name" lines).
	bool shown = false;
	UInt32 shownRef = 0;
	// The block's function statics, as they were: the last thing the ring
	// was on and the lines left for its changes.
	UInt32 ringRef = 0;
	UInt32 ringLines = 40;
};

// What the ring reads of the rest of the frame.
struct ReachRingLinks {
	const game::NearItem& nearItem;  // the item nearest a hand: trusted as the settle's own
};

void StepReachRing(ReachRingState& state, const HandStep& step, const HandShared& shared, const ReachRingLinks& links);

}  // namespace obvr::camera::hands
