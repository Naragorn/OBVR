#pragma once

// When and where the places' sign shows (vr/Holster.h, ui/HolsterSignPainter.h;
// Nadi, 2026-10-28: "die tafeln beim holstern sind teilweise zu weit weg vom
// blick und zu dominant, mach die dezenter wie B&S und sichtbarer. ausserdem
// kommen die zu schnell oder competen mitteinander").
//
// When: a hand has to stay at a place for kSignDwellSeconds before the sign
// comes, so a hand swinging past the hip shows nothing. The hand it came for
// owns it while it stays at that place and kSignGraceSeconds after, so the
// other hand arriving at a place does not take it over; after that the other
// hand's place shows when that hand has dwelt there. Both arriving in the
// same frame: the weapon hand's. A hint (a reach that did nothing, or one
// that would) comes at once and takes the sign: it answers the reach just
// made. It fades in and out (kSignFade*Seconds); a menu or a load hides it
// at once.
//
// Where: no longer at the place itself - the hip's was 80 degrees below the
// gaze, the back's 0.29 m before the face at 0.16 m wide (about 30 degrees).
// It hangs kSignDistanceMetres from the eyes, kSignDownDegrees below the gaze
// and kSignAcrossDegrees to the place's side, following the head softly
// (kSignFollowSeconds), so it is seen without looking for it and stays out of
// the middle of the view.

#include <cmath>

#include "vr/Holster.h"
#include "vr/Quaternion.h"

namespace obvr::vr {

inline constexpr float kSignDwellSeconds = 0.30f;
inline constexpr float kSignGraceSeconds = 0.20f;
inline constexpr float kSignFadeInSeconds = 0.12f;
inline constexpr float kSignFadeOutSeconds = 0.20f;
inline constexpr float kSignLongestFrame = 0.05f;

inline constexpr float kSignDistanceMetres = 0.55f;
inline constexpr float kSignDownDegrees = 24.0f;
inline constexpr float kSignAcrossDegrees = 14.0f;
inline constexpr float kSignFollowSeconds = 0.12f;

struct SignTimingInput {
	HolsterSlot rightSlot = HolsterSlot::None;  // the place each hand is at
	HolsterSlot leftSlot = HolsterSlot::None;
	bool hint = false;  // a hint is up, for this hand at this place
	HolsterSlot hintSlot = HolsterSlot::None;
	bool hintLeft = false;
	bool allowed = false;  // in the world, no menu, the labels on
	float seconds = 0.0f;  // this frame's
};

struct SignTimingState {
	HolsterSlot slot = HolsterSlot::None;  // the owner's place, None when none owns it
	bool left = false;
	float away = 0.0f;  // since the owner's hand left its place
	HolsterSlot shownSlot = HolsterSlot::None;  // what shows, kept while it fades out
	bool shownLeft = false;
	float alpha = 0.0f;
	HolsterSlot dwellSlot[2] = {HolsterSlot::None, HolsterSlot::None};  // right, left
	float dwell[2] = {0.0f, 0.0f};
};

struct SignTimingOutput {
	bool visible = false;
	HolsterSlot slot = HolsterSlot::None;
	bool left = false;
	float alpha = 0.0f;
};

inline SignTimingOutput StepSignTiming(SignTimingState& s, const SignTimingInput& in) {
	float dt = in.seconds == in.seconds && in.seconds > 0.0f ? in.seconds : 0.0f;
	dt = dt > kSignLongestFrame ? kSignLongestFrame : dt;
	if (!in.allowed) {
		s = SignTimingState{};
		return SignTimingOutput{};
	}
	const HolsterSlot at[2] = {in.rightSlot, in.leftSlot};
	for (int h = 0; h < 2; ++h) {
		if (at[h] != HolsterSlot::None && at[h] == s.dwellSlot[h]) {
			s.dwell[h] += dt;
		} else {
			s.dwellSlot[h] = at[h];
			s.dwell[h] = 0.0f;
		}
	}
	bool snap = false;
	if (in.hint && in.hintSlot != HolsterSlot::None) {
		snap = s.slot != in.hintSlot || s.left != in.hintLeft || s.alpha < 1.0f;
		s.slot = in.hintSlot;
		s.left = in.hintLeft;
		s.away = 0.0f;
	} else if (s.slot != HolsterSlot::None) {
		if (at[s.left ? 1 : 0] == s.slot) {
			s.away = 0.0f;
		} else {
			s.away += dt;
			if (s.away >= kSignGraceSeconds) {
				s.slot = HolsterSlot::None;
				s.away = 0.0f;
			}
		}
	}
	if (s.slot == HolsterSlot::None) {
		const bool rightReady = at[0] != HolsterSlot::None && s.dwell[0] >= kSignDwellSeconds;
		const bool leftReady = at[1] != HolsterSlot::None && s.dwell[1] >= kSignDwellSeconds;
		if (rightReady || leftReady) {
			s.left = leftReady && (!rightReady || s.dwell[1] > s.dwell[0]);
			s.slot = at[s.left ? 1 : 0];
		}
	}
	if (s.slot != HolsterSlot::None) {
		s.shownSlot = s.slot;
		s.shownLeft = s.left;
		s.alpha = snap ? 1.0f : s.alpha + dt / kSignFadeInSeconds;
	} else {
		s.alpha -= dt / kSignFadeOutSeconds;
	}
	s.alpha = s.alpha > 1.0f ? 1.0f : s.alpha < 0.0f ? 0.0f : s.alpha;
	if (s.alpha <= 0.0f && s.slot == HolsterSlot::None) {
		s.shownSlot = HolsterSlot::None;
	}
	SignTimingOutput out;
	out.visible = s.shownSlot != HolsterSlot::None && s.alpha > 0.0f;
	out.slot = out.visible ? s.shownSlot : HolsterSlot::None;
	out.left = s.shownLeft;
	out.alpha = out.visible ? s.alpha : 0.0f;
	return out;
}

inline bool SlotIsLeft(HolsterSlot slot) { return slot == HolsterSlot::HipLeft || slot == HolsterSlot::BackLeft; }

// The sign's point in tracking space (x right, y up, z back): along the gaze,
// turned down and to the place's side.
inline NiPoint3 SignPlaceInView(const Quaternion& head, const NiPoint3& eyes, HolsterSlot slot) {
	const NiMatrix33 m = ToMatrix(head);
	const NiPoint3 forward = m * NiPoint3{0.0f, 0.0f, -1.0f};
	const NiPoint3 up = m * NiPoint3{0.0f, 1.0f, 0.0f};
	const NiPoint3 right = m * NiPoint3{1.0f, 0.0f, 0.0f};
	constexpr float kRadians = 3.14159265f / 180.0f;
	const float side = SlotIsLeft(slot) ? -1.0f : 1.0f;
	NiPoint3 d = forward + up * -std::tan(kSignDownDegrees * kRadians) +
	             right * (side * std::tan(kSignAcrossDegrees * kRadians));
	const float length = std::sqrt(d.LengthSquared());
	d = d * (kSignDistanceMetres / length);
	return eyes + d;
}

// The sign's point a frame on: at the target at once when it was not shown,
// else eased toward it with the time constant kSignFollowSeconds.
inline NiPoint3 FollowSign(const NiPoint3& at, const NiPoint3& target, float seconds, bool justShown) {
	if (justShown) {
		return target;
	}
	const float dt = seconds == seconds && seconds > 0.0f ? seconds : 0.0f;
	const float k = 1.0f - std::exp(-dt / kSignFollowSeconds);
	return at + (target - at) * k;
}

}  // namespace obvr::vr
