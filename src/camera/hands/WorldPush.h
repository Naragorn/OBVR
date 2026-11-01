#pragma once

// The weapon and the hands push what they meet (game/WorldPush.h): the
// weapon hand's blade while a melee weapon is drawn, else the hand; the
// other hand always; in the world only, in first person, not in a menu.
// With it what the pushing needs each frame: whether each hand is a fist
// (vr/Fist.h - for its Havok body, no pushing people with a fist, and for
// the off hand's blow), the hands' Havok bodies (game/HandBodies.h), the
// player's capsule, and the dropped weapon passed through.
//
// Moved out of camera/CameraHook.cpp's UpdateHandMode on 2026-11-01
// (docs/god-functions-plan.md, phase 2 step 2.15): the code as it was, its
// six file globals and one static now WorldPushState's members; the hand
// bodies' step and the shoves read the fists as g_worldPush.<side>BodyFist.
// Harness: body-soft-push.txt, fist.txt, hand-shield-contact*.txt.

#include "camera/hands/HandStep.h"
#include "game/HandBodies.h"
#include "vr/FingersRead.h"

namespace obvr::camera::hands {

struct WorldPushState {
	// Whether each hand is a fist, and which fingers each hand's sensors
	// have read.
	bool rightBodyFist = false;
	bool leftBodyFist = false;
	vr::FingersRead rightFingersRead;
	vr::FingersRead leftFingersRead;
	bool fingerLeftOutLogged[2] = {false, false};
	UInt32 fistChangeLines = 40;
	// The block's function static, as it was.
	UInt32 gripFistLines[2] = {6, 6};
};

// What the pushing reads of the rest of the frame.
struct WorldPushLinks {
	float deltaSeconds;       // the frame's (g_deltaSeconds)
	UInt32 droppedWeaponRef;  // the dropped weapon watched for its body (the release's)
	// CameraHook's measure of the bodies at a mark (its own statics), called
	// between the bodies' step and the push as it was.
	void (*measureBodies)(const game::HandBodyFrame& bodies, bool blade, bool marked);
};

// What the frame after the push reads: whether the hands are in place to
// push (the shoves, the leads and the grapple follow on it).
struct WorldPushOutput {
	bool inPlace = false;
};

WorldPushOutput StepWorldPush(WorldPushState& state, const HandStep& step, const HandShared& shared,
                              const vr::HandModeFrame& frame, const WorldPushLinks& links);

}  // namespace obvr::camera::hands
