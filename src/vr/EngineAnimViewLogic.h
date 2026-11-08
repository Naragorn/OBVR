#pragma once

// The eyes held at the body's head while the engine plays one of its own
// third-person animations (a Quest 3 tester, beta 8, 2026-11-06: "When
// changing locations, reloading, dismounting a horse, or after a vampire bite,
// the camera sometimes shifts to the side of the character's head, only
// returning to the correct position after a short delay"; Nadi: chairs and
// knockdowns too). Measured 2026-11-08 (view-offset-events.txt,
// chair-sit-view.txt, camera/MarkEyes.cpp): the engine switches to third
// person for the sit-down and the get-up (about a second each, the eyes 162 to
// 222 units off the head), for a fall and its getting up (8.5 s, up to 187),
// as it does for the mount, the dismount and the feed - in a headset the
// view leaves the body for the chase camera and comes back. Enhanced Camera
// keeps such animations in first person; here the eyes go to the body's head
// for as long as the engine's third person lasts, the headset's turn on top.
//
// Pure, covered by engine_anim_view_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

// The eyes under the centre of the face's bound, units: standing in first
// person the face's centre reads 5 units over the eyes (2026-11-08,
// view-offset-events.txt "the body's head 6.0 units level from the eyes,
// 5.1 up").
inline constexpr float kEyesUnderFaceUnits = 5.0f;
// The animation's head bob damped: a time constant, seconds.
inline constexpr float kHeadBobSeconds = 0.08f;
// A jump of the head over this in one frame is a teleport, taken at once.
inline constexpr float kHeadJumpUnits = 60.0f;

struct EngineAnimViewInput {
	bool enabled = true;      // [Look] FirstPersonAnimations
	bool headset = false;     // a tracked head
	bool thirdPerson = false; // the engine's view this frame
	bool dead = false;        // the death view has its own camera
	UInt16 sitMountBytes = 0; // +0x71C sit down / mount / feed, +0x71D get up / dismount
	UInt8 sitSleepState = 0;  // 3 sitting down, 4 seated, 5 getting up
	SInt32 knockState = 0;    // 1 to 4 down, 5 and 6 getting up, -1 unread
};

// Whether this frame's third person is one of the engine's own animations:
// the sit, mount, dismount or feed bytes, sitting down or getting up, or
// knocked down. A third person nothing of that names (the POV key, a mod's
// ToggleFirstPerson) is left as it is.
inline bool EngineAnimViewWanted(const EngineAnimViewInput& in) {
	if (!in.enabled || !in.headset || !in.thirdPerson || in.dead) {
		return false;
	}
	return in.sitMountBytes != 0 || in.sitSleepState == 3 || in.sitSleepState == 5 || in.knockState > 0;
}

// After the animation's reasons are gone the engine keeps third person for a
// frame before it switches back (chair-sit-view.txt 2026-11-08: the get-up's
// bytes 0000 on frame 2793, the switch to first person on the same frame,
// after the camera) - held over that, at most this long, while the third
// person lasts.
inline constexpr float kHoldAfterSeconds = 0.5f;

struct EngineAnimViewState {
	bool active = false;
	float holdSeconds = 0.0f;
	NiPoint3 eyes{0.0f, 0.0f, 0.0f};
};

// EngineAnimViewWanted with the hold over the engine's last third-person
// frames: true while the reasons last, and after them while the third person
// does, for kHoldAfterSeconds at most.
inline bool EngineAnimViewHeld(EngineAnimViewState& s, const EngineAnimViewInput& in, float dtSeconds) {
	if (EngineAnimViewWanted(in)) {
		s.holdSeconds = kHoldAfterSeconds;
		return true;
	}
	const bool allowed = in.enabled && in.headset && in.thirdPerson && !in.dead;
	if (s.active && allowed && s.holdSeconds > 0.0f) {
		s.holdSeconds -= dtSeconds > 0.0f ? dtSeconds : 0.0f;
		return true;
	}
	s.holdSeconds = 0.0f;
	return false;
}

// The eyes for this frame from the face's centre in the world: at once on the
// first frame and after a jump, the animation's bob damped between. False (and
// the state let go) when not wanted or the head is not a number.
inline bool StepEngineAnimView(EngineAnimViewState& s, bool wanted, const NiPoint3& face, float dtSeconds,
                               NiPoint3& eyes) {
	const bool finite = face.x == face.x && face.y == face.y && face.z == face.z;
	if (!wanted || !finite) {
		s.active = false;
		return false;
	}
	const NiPoint3 target{face.x, face.y, face.z - kEyesUnderFaceUnits};
	const NiPoint3 d{target.x - s.eyes.x, target.y - s.eyes.y, target.z - s.eyes.z};
	if (!s.active || d.x * d.x + d.y * d.y + d.z * d.z > kHeadJumpUnits * kHeadJumpUnits) {
		s.eyes = target;
	} else {
		const float dt = dtSeconds > 0.0f ? (dtSeconds < 0.1f ? dtSeconds : 0.1f) : 0.0f;
		const float k = 1.0f - math::Exp(-dt / kHeadBobSeconds);
		s.eyes = NiPoint3{s.eyes.x + d.x * k, s.eyes.y + d.y * k, s.eyes.z + d.z * k};
	}
	s.active = true;
	eyes = s.eyes;
	return true;
}

}  // namespace obvr::vr
