#pragma once

// The game's sneak lowers the first-person eye; in Full VR the real head
// gives the height (controls-spec 4.27, Nadi 2026-10-24: "crouchen in real
// life [verursacht] auch einen kamera sprung nach unten. wegpatchen (default
// an)").
//
// Measured (crouch-camera.txt, 2026-10-24 19:32): the engine's eye stands
// 117.9 units above the feet and 93.2 sneaking - 24.7 units (0.35 m) lower,
// eased over 8 frames as the sneak begins and back over 8 as it ends. The
// first-person camera is the skeleton's Camera01 (FUN_0066b710 reads its
// world translation); its own local height stays, its parents carry the
// drop. A 45 cm crouch in the room so put the eyes 80 cm down.
//
// So OBVR holds the engine's eye at the height above the feet it had on the
// last frame before the sneak: the lift is what the engine took away, frame
// for frame, and its ease cancels with it. After the sneak the lift follows
// the engine's eye back up. The eye also moves a little with the pose -
// 118.7 to 119.9 units in body-ik.txt's standing marks, back at 119.3 after
// a sneak begun at 119.9 - so the rise counts as done once the eye is within
// kSneakEyeSettledUnits of the height held and has stopped moving
// (kSneakEyeStillUnits a frame); what is left of the lift then fades out
// over kSneakEyeFadeSeconds rather than stepping. An eye still low after
// kSneakEyeReleaseSeconds fades out the same way. Every sneak in Full VR,
// the stick's too: the head is the only height there.
//
// Pure, covered by sneak_eye_hold_test; CameraHook reads the engine's eye
// and the feet, and adds the lift to the camera.

#include "core/Types.h"

namespace obvr::camera {

inline constexpr float kSneakEyeReleaseSeconds = 1.0f;
inline constexpr float kSneakEyeSettledUnits = 1.5f;
inline constexpr float kSneakEyeStillUnits = 0.2f;
inline constexpr float kSneakEyeFadeSeconds = 0.3f;
inline constexpr float kSneakEyeMaxLiftUnits = 60.0f;  // the measured drop is 24.7

struct SneakEyeHoldState {
	bool haveStanding = false;
	float standing = 0.0f;  // the engine's eye above the feet, the last frame before a sneak
	bool holding = false;   // the sneak, or the rise after it
	float release = 0.0f;   // seconds left for the rise after the sneak
	bool haveLastEye = false;
	float lastEye = 0.0f;   // the engine's eye the frame before
	float fade = 0.0f;      // what is left of the lift after the hold, fading out
	float fadeRate = 0.0f;  // units a second
};

struct SneakEyeHoldInput {
	bool enabled = false;      // [Hands] KeepEyeHeightSneaking with Full VR running
	bool live = false;         // first person, not riding, the feet read
	bool loading = false;      // a load: the height held is the last character's
	bool sneaking = false;
	float eyeAboveFeet = 0.0f; // the engine's camera height minus the feet's, this frame
	float dt = 0.0f;
};

enum class SneakEyeEvent : UInt8 {
	None,
	Began,     // a sneak's drop is held
	Settled,   // the engine's eye is back at the height held
	TimedOut,  // the release time ran out with the eye still low
	Reset,     // off, or not live: the hold is dropped
};

struct SneakEyeHoldVerdict {
	float lift = 0.0f;  // units to add to the camera's height
	SneakEyeEvent event = SneakEyeEvent::None;
};

inline float SneakEyeLiftFor(float standing, float eyeAboveFeet) {
	const float drop = standing - eyeAboveFeet;
	if (!(drop > 0.0f)) {
		return 0.0f;
	}
	return drop < kSneakEyeMaxLiftUnits ? drop : kSneakEyeMaxLiftUnits;
}

inline void BeginSneakEyeFade(SneakEyeHoldState& s, float lift) {
	s.holding = false;
	s.release = 0.0f;
	s.fade = lift;
	s.fadeRate = lift / kSneakEyeFadeSeconds;
}

inline SneakEyeHoldVerdict StepSneakEyeHold(SneakEyeHoldState& s, const SneakEyeHoldInput& in) {
	SneakEyeHoldVerdict v;
	const bool readable = in.eyeAboveFeet == in.eyeAboveFeet;
	if (!in.enabled || in.loading || !in.live || !readable) {
		if (s.holding || s.fade > 0.0f) {
			v.event = SneakEyeEvent::Reset;
		}
		// Third person or a ride a while keeps the standing height for the
		// first person after it; off or a load forget it.
		const bool keep = in.enabled && !in.loading && s.haveStanding;
		const float standing = s.standing;
		s = SneakEyeHoldState{};
		s.haveStanding = keep;
		s.standing = keep ? standing : 0.0f;
		return v;
	}
	const float dt = in.dt == in.dt && in.dt > 0.0f ? in.dt : 0.0f;
	const float moved = s.haveLastEye ? in.eyeAboveFeet - s.lastEye : 0.0f;
	const bool still = s.haveLastEye && moved < kSneakEyeStillUnits && moved > -kSneakEyeStillUnits;
	s.haveLastEye = true;
	s.lastEye = in.eyeAboveFeet;
	if (in.sneaking) {
		if (!s.haveStanding) {
			return v;
		}
		if (!s.holding) {
			// A sneak again while the last one's lift fades: the height the
			// view is at, not the engine's eye under it.
			s.standing += s.fade;
			s.fade = 0.0f;
			s.holding = true;
			v.event = SneakEyeEvent::Began;
		}
		s.release = kSneakEyeReleaseSeconds;
		v.lift = SneakEyeLiftFor(s.standing, in.eyeAboveFeet);
		return v;
	}
	if (s.holding) {
		const float lift = SneakEyeLiftFor(s.standing, in.eyeAboveFeet);
		s.release -= dt;
		if (lift <= kSneakEyeSettledUnits && still) {
			v.event = SneakEyeEvent::Settled;
		} else if (s.release <= 0.0f) {
			v.event = SneakEyeEvent::TimedOut;
		} else {
			v.lift = lift;
			return v;
		}
		BeginSneakEyeFade(s, lift);
		v.lift = lift;
	} else if (s.fade > 0.0f) {
		s.fade -= s.fadeRate * dt;
		if (s.fade < 0.0f) {
			s.fade = 0.0f;
		}
		v.lift = s.fade;
	}
	s.haveStanding = true;
	s.standing = in.eyeAboveFeet;
	return v;
}

inline const char* SneakEyeEventWord(SneakEyeEvent e) {
	switch (e) {
	case SneakEyeEvent::Began:
		return "the sneak lowers the engine's eye - held at the standing height";
	case SneakEyeEvent::Settled:
		return "the engine's eye is back at the standing height - the hold ends, the rest fades";
	case SneakEyeEvent::TimedOut:
		return "the engine's eye stayed low after the sneak - the hold ends, the lift fades";
	case SneakEyeEvent::Reset:
		return "the hold dropped - off, third person, riding, the feet unread or a load";
	case SneakEyeEvent::None:
		break;
	}
	return nullptr;
}

}  // namespace obvr::camera
