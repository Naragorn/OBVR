#pragma once

// Opening things by hand, its state in one owner: by reaching (a free hand at
// a container, a body or - sneaking - a pocket; vr/ReachOpen.h, stepped by
// CameraHook's StepReachOpen) and doors by grip (an empty hand closes its grip
// at a door's handle and pulls or pushes it open; vr/DoorGrip.h,
// docs/door-grip-spec.md option B). The reach's four CameraHook file globals
// moved here on 2026-11-11 so the doors' state had an owner. Harness:
// reach-open.txt, door-grip-*.txt.

#include "camera/hands/HandStep.h"
#include "camera/hands/TakeByHand.h"
#include "game/NearbyItems.h"
#include "game/ReachTargets.h"
#include "game/RefFingerprint.h"
#include "vr/DoorGrip.h"
#include "vr/ReachOpen.h"

namespace obvr::camera::hands {

struct OpenByHandState {
	// By reaching: the step's state, and what the target and the next one
	// were the frame they were reached, live then (game/RefFingerprint.h) -
	// a body or a chest can be deleted while the reach still holds it.
	vr::ReachOpenState reachOpen;
	game::RefFingerprint reachPrint;
	game::RefFingerprint reachNextPrint;
	UInt32 reachLines = 60;
	// Doors by grip: the step's state, the held door's fingerprint, its
	// model boxes, the log's budget.
	vr::DoorGripState door;
	// For the hints (camera/hands/Hints.h): a free hand at a door's handle
	// now (which), and the doors opened by the grip so far.
	bool atHandle = false;
	bool atHandleLeft = false;
	UInt32 opens = 0;
	game::RefFingerprint doorPrint;
	game::DoorBoxCache doorBoxes;
	UInt32 doorLines = 40;
};

// What the doors read of the rest of the frame.
struct DoorLinks {
	bool handsInWorld;            // the hands in the world, first person, no OBVR menu over them
	const game::NearItem& nearItem;  // the hands' pick: an item under a hand goes first
	// The controllers' offsets from the eyes before the walls, the people and
	// the doors stopped the drawn hands ([0] right, [1] left): a hand pushing a
	// door rests on its leaf while the controller goes on, and the push is
	// the controller's.
	const NiPoint3 (&controllersAt)[2];
};

void StepDoorGrip(OpenByHandState& state, const HandStep& step, const HandShared& shared, const DoorLinks& links);

}  // namespace obvr::camera::hands
