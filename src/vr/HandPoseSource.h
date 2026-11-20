#pragma once

// Which pose a controller is drawn and used with this frame (free-weapons-spec,
// open bugs of 2026-11-06: a tester's half-swording "very sluggish" below 60
// fps).
//
// Until 2026-11-20 OBVR read both controllers with
// GetDeviceToAbsoluteTrackingPose(seated, 0): the pose at the moment of asking,
// just after WaitGetPoses - while the head was drawn with WaitGetPoses' own
// render pose, which the compositor predicts to the moment the frame is lit.
// How far ahead that is (Valve, github.com/ValveSoftware/openvr issue 518,
// read 2026-11-20): with a 3 ms running start about 25 ms at 90 Hz (a frame to
// render, a frame to scan out, the running start); under interleaved
// reprojection - SteamVR's half rate - the app gets two frames and loses the
// running start, about 33 ms, and every frame is shown twice, the second time
// 11 ms later still. So the hands and the weapon in them were drawn 25 ms
// behind where the controllers were when the frame was lit at full rate, and
// 33 to 44 ms at half rate; and the hands' place against the head took the head
// and the hands from two moments, so a turning head shifted them.
// The harness (half-sword-lag.txt, one-hand-lag.txt, 2026-11-20) measured
// OBVR's own placement after the read as the same at 90, 45 and 30 fps: the
// read itself is what grows with the frame time.
//
// Now the controllers come from the same WaitGetPoses as the head: its render
// poses, which the compositor fills for every device with the head's
// prediction (openvr.h: "poses to render with"; IVRInput's
// GetPoseActionDataForNextFrame returns the same). The moment of asking stays
// the fallback whenever the head has no pose of this frame (rendering off, no
// compositor, the call failed), the device's frame pose is not valid, and with
// [Hands] PredictHands=0.
//
// Pure, covered by hand_pose_source_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

enum class HandPoseSource : UInt8 {
	Frame,  // this frame's render pose, predicted to when the frame is lit
	Now,    // the pose at the moment of asking
};

struct HandPoseChoice {
	bool predict = true;        // [Hands] PredictHands
	bool headFromFrame = false; // the head is drawn with this frame's render pose
	bool frameHasDevices = false;  // this frame's WaitGetPoses filled every device
	UInt32 device = 0;          // the controller's device index
	UInt32 deviceCount = 0;     // how many devices the frame's poses hold
	bool frameValid = false;    // that device's frame pose: valid and connected
};

inline HandPoseSource ChooseHandPose(const HandPoseChoice& c) {
	const bool frame =
		c.predict && c.headFromFrame && c.frameHasDevices && c.device < c.deviceCount && c.frameValid;
	return frame ? HandPoseSource::Frame : HandPoseSource::Now;
}

// How far ahead of the moment of asking a frame pose is, for the log: the way
// from the pose then to the frame's pose along the controller's motion, over
// its speed (metres, metres a second; seconds out). Slower than
// kHandLeadMinSpeed the way is tracking noise against the time: no answer
// (false), nor for a value that is not a number.
inline constexpr float kHandLeadMinSpeed = 0.5f;  // m/s

inline bool HandLeadSeconds(const NiPoint3& framePosition, const NiPoint3& nowPosition, const NiPoint3& velocity,
                            float& seconds) {
	const float speedSquared = velocity.LengthSquared();
	if (!(speedSquared >= kHandLeadMinSpeed * kHandLeadMinSpeed)) {
		return false;
	}
	const NiPoint3 way = framePosition - nowPosition;
	const float lead = (way.x * velocity.x + way.y * velocity.y + way.z * velocity.z) / speedSquared;
	if (!(lead == lead)) {
		return false;
	}
	seconds = lead;
	return true;
}

}  // namespace obvr::vr
