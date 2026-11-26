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
// A pump or a stroke ([Hands] ArmSwingBalance; the tester, 2026-11-26: "wenn
// man die waffe gezogen hat dann ist 'swing arms' zum sprinten schwierig oder
// buggy, weil er nicht weiss ob sprint oder schwung"). The weapon hand's
// swing detector never knew about the run: his sprints' pumps went past the
// swing's 2.6 m/s and became attacks - six inside his sprints, 2.7-3.7 m/s
// and 0.22-0.75 m (outside them his swings ran 5.5 m/s and 0.71 m), each
// paid in fatigue - and a slash with the torch hand drawn back was the arms
// against each other, a sprint for a moment. A runner's arms swing alike,
// the slower at least ArmSwingBalance (half) of the faster's whole speed: a
// pump - it starts the run, and the swing detectors are held idle through it
// (vr::HandMode), so a pump never strikes, swishes or costs a swing's
// fatigue. One hand far faster than the other is a stroke: an attack as
// ever, and it starts no run. A run started holds on any swing against each
// other, a lopsided one too, as before. And while the arms run, a hand swung
// up past the block's height is no block (ArmSwingHoldsBlocks). 0 switches it
// all off: every swing against each other runs, every fast hand swings and
// every raised hand blocks, as before 2026-11-26.
//
// Pure; arm_swing_test.

#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::vr {

struct ArmSwingSettings {
	bool enabled = false;  // off by default; the first start asks (ui/FirstStart.h RunBy)
	float speedMetres = 0.7f;   // each hand along the body, metres a second
	float holdSeconds = 0.35f;  // the run kept past the last swinging frame
	float balance = 0.5f;       // a pump's slower hand at least this share of the faster's speed; 0 off
};

struct ArmSwingState {
	float sinceSwing = 1.0e9f;
};

// The two hands are against each other when the angle between their ways
// along the body is more than 120 degrees.
inline constexpr float kArmSwingOpposedCosine = -0.5f;

// Whether the two hands swing along the body against each other, each fast
// enough: both hands' velocities in the head's frame (Oblivion axes - x
// right, y forward, z up - metres a second).
inline bool ArmsAgainstEachOther(const ArmSwingSettings& settings, const NiPoint3& left, const NiPoint3& right) {
	const float ll = math::Sqrt(left.y * left.y + left.z * left.z);
	const float rl = math::Sqrt(right.y * right.y + right.z * right.z);
	const bool fast = ll >= settings.speedMetres && rl >= settings.speedMetres;
	return fast && (left.y * right.y + left.z * right.z) <= kArmSwingOpposedCosine * ll * rl;
}

// Whether this frame is a pump of the run: ArmSwingRun and ArmSwingBalance
// on, both hands tracked and against each other, the slower's whole speed at
// least `balance` of the faster's.
inline bool ArmSwingPump(const ArmSwingSettings& settings, bool bothTracked, const NiPoint3& left,
                         const NiPoint3& right) {
	if (!settings.enabled || !(settings.balance > 0.0f) || !bothTracked || !ArmsAgainstEachOther(settings, left, right)) {
		return false;
	}
	const float l = math::Sqrt(left.LengthSquared());
	const float r = math::Sqrt(right.LengthSquared());
	return (l < r ? l : r) >= settings.balance * (l < r ? r : l);
}

// One frame, whether both are tracked. Answers whether the arms run: a pump
// starts it, any swing against each other keeps it (with ArmSwingBalance 0
// any swing against each other starts it too).
inline bool StepArmSwing(ArmSwingState& s, const ArmSwingSettings& settings, bool bothTracked,
                         const NiPoint3& left, const NiPoint3& right, float dtSeconds) {
	const bool running = s.sinceSwing <= settings.holdSeconds;
	const bool against = bothTracked && ArmsAgainstEachOther(settings, left, right);
	const bool beat =
		against && (running || !(settings.balance > 0.0f) || ArmSwingPump(settings, bothTracked, left, right));
	if (settings.enabled && beat) {
		s.sinceSwing = 0.0f;
	} else if (dtSeconds > 0.0f) {
		s.sinceSwing += dtSeconds;
	}
	return settings.enabled && s.sinceSwing <= settings.holdSeconds;
}

// Whether a raised hand's block is held off: while the arms run with
// ArmSwingBalance on (the hold over each turn included). A pump takes the
// left hand up past the block's height at every turn, and the engine walks a
// blocking player - the harness, 2026-11-26: running at 322 units a second
// with the torch low, 107 with it raised into the block, the walk's speed -
// so the tester's sprint broke off at each pump of the torch arm. To block,
// the arms stop: the run ends ArmSwingHoldSeconds later and the hand blocks.
inline bool ArmSwingHoldsBlocks(const ArmSwingState& s, const ArmSwingSettings& settings) {
	return settings.enabled && settings.balance > 0.0f && s.sinceSwing <= settings.holdSeconds;
}

// The run control: with ArmSwingRun the swing (and the inventory's drop key,
// which holds Shift), else the stick's run as before.
inline bool ArmSwingRun(bool enabled, bool swinging, bool stickRun, bool dropShift) {
	return enabled ? (swinging || dropShift) : stickRun;
}

}  // namespace obvr::vr
