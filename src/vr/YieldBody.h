#pragma once

// The yield's hands in the body's frame (2026-11-11, the tester: "go yield
// A"). The harness's yield-gear run of that day yielded a fourth time two
// seconds after its script, the hands held still in the pose while the head
// turned to follow the bandit: the hands were read relative to the head,
// whose eyes swing about the neck as it turns - a turn of a few degrees
// moved both hands sideways relative to the eyes by centimetres, back and
// forth as the bandit circled, and with the wave's 2 cm that was a wave.
//
// Now each hand is read against vr::BodyFrame (the torso's heading, the
// holster places' frame since e11c200): its place for the gates (up before
// the eyes, ahead of them) relative to the upright eyes, its fingers in the
// body's axes, and the wave's fingertip sideways along the body's right
// anchored in the room (no origin taken off), so a head that turns or jolts
// while the hands stand still moves nothing the wave counts. The palm stays
// asked against the gaze (YieldPalmAhead, the controller relative to the
// head): the palm is shown to the one looked at, whom the enemy-ahead check
// finds along the gaze too.
//
// Pure; covered by yield_test.

#include "vr/BodyFrame.h"
#include "vr/Quaternion.h"
#include "vr/Yield.h"

namespace obvr::vr {

// One hand in tracking space (OpenVR: x right, y up, -z forward) into the
// yield's input: `position` (x outward, y ahead, z up, metres from the
// upright eyes), `fingers` (the controller's pointing axis, -z, in the
// body's axes, x outward), and the room-anchored fingertip `lateral`
// (outward). False with no body frame: the caller keeps its head-relative
// reading.
inline bool YieldHandInBody(const BodyFrame& body, const NiPoint3& handPosition, const Quaternion& handOrientation,
                            bool rightRole, YieldHandInput& out) {
	if (!body.valid) {
		return false;
	}
	const float side = rightRole ? 1.0f : -1.0f;  // x outward for either hand
	const NiPoint3 place = ToBodyFrame(body, handPosition);
	out.position = NiPoint3{place.x * side, place.y, place.z};
	const NiPoint3 f = ToMatrix(handOrientation) * NiPoint3{0.0f, 0.0f, -1.0f};
	const float rightAxisX = -body.forwardZ;  // the body's right on the level: (-fz, fx)
	const float rightAxisZ = body.forwardX;
	out.fingers = NiPoint3{(f.x * rightAxisX + f.z * rightAxisZ) * side, f.x * body.forwardX + f.z * body.forwardZ, f.y};
	const NiPoint3 tip = handPosition + f * kYieldFingertipMetres;
	out.roomLateral = true;
	out.lateral = (tip.x * rightAxisX + tip.z * rightAxisZ) * side;
	return true;
}

}  // namespace obvr::vr
