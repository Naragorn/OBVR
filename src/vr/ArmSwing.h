#pragma once

// Running by swinging both arms, as when running for real ([Hands]
// ArmSwingRun, default off since 2026-10-28 - the first start asks; Nadi, 2026-10-26: "statt left stick press down
// zum laufen: mit beiden Händen vor und zurück wandern wie als würde man in
// echt laufen", 2026-10-28: "wenn an dann ... stick press läuft nicht mehr",
// "default off und es kommt ins onboarding user können dort entscheiden"). The left stick still walks; swung arms make it a
// run, and the left stick held no longer does.
//
// A swing: both hands moving along the body (forward and back, up and down -
// the head's y and z, so a head looking down reads the swing too) against
// each other, each at ArmSwingSpeed or faster. One arm (a blow), both the
// same way (a two-hander), sideways (a clap) or a hand untracked is none.
// The hands stand still for a moment at each turn of a swing, so the run
// holds ArmSwingHoldSeconds past the last swinging frame. Both values are
// first guesses, not tried in the headset (docs/guesses-overview.md).
//
// Pure; arm_swing_test.

#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::vr {

struct ArmSwingSettings {
	bool enabled = false;  // off by default; the first start asks (ui/FirstStart.h RunBy)
	float speedMetres = 0.7f;   // each hand along the body, metres a second
	float holdSeconds = 0.35f;  // the run kept past the last swinging frame
};

struct ArmSwingState {
	float sinceSwing = 1.0e9f;
};

// The two hands are against each other when the angle between their ways
// along the body is more than 120 degrees.
inline constexpr float kArmSwingOpposedCosine = -0.5f;

// One frame: both hands' velocities in the head's frame (Oblivion axes - x
// right, y forward, z up - metres a second), whether both are tracked.
// Answers whether the arms run.
inline bool StepArmSwing(ArmSwingState& s, const ArmSwingSettings& settings, bool bothTracked,
                         const NiPoint3& left, const NiPoint3& right, float dtSeconds) {
	const float ll = math::Sqrt(left.y * left.y + left.z * left.z);
	const float rl = math::Sqrt(right.y * right.y + right.z * right.z);
	const bool fast = ll >= settings.speedMetres && rl >= settings.speedMetres;
	const bool opposed = fast && (left.y * right.y + left.z * right.z) <= kArmSwingOpposedCosine * ll * rl;
	if (settings.enabled && bothTracked && opposed) {
		s.sinceSwing = 0.0f;
	} else if (dtSeconds > 0.0f) {
		s.sinceSwing += dtSeconds;
	}
	return settings.enabled && s.sinceSwing <= settings.holdSeconds;
}

// The run control: with ArmSwingRun the swing (and the inventory's drop key,
// which holds Shift), else the stick's run as before.
inline bool ArmSwingRun(bool enabled, bool swinging, bool stickRun, bool dropShift) {
	return enabled ? (swinging || dropShift) : stickRun;
}

}  // namespace obvr::vr
