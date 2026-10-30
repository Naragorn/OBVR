#pragma once

// The kick (docs/physical-combat-spec.md section 11; the tester, 2026-10-25:
// "ein kick vom player wie in B&S auf button druck ... ein fuß schiesst dann
// voraus wie in B&S und staggert gegner"; Nadi, 2026-10-30: "go B" - the
// foot seen, by the leg IK, or no kick at all). The weapon hand's B in the
// world kicks once (vr::PlanHandControls); the weapon side's leg is driven by
// the body's leg IK (game/BodyIk.cpp, StepBodyLegs) from where the animation
// has the foot to a point ahead of the hip and back:
//
// - out in KickOutSeconds, eased; held KickHoldSeconds; back in
//   KickBackSeconds - its weight 0..1 the share of the way to the peak;
// - the peak: along the body's facing, level, at KickReachShare of the leg's
//   length from the hip, KickDropDegrees below the hip's level - short of
//   the leg's full length, so the knee keeps a bend to solve;
// - the foot strikes (striking) through the second half of its way out and
//   the hold, never on the way back;
// - one kick a press, none again within KickCooldownSeconds of the press;
// - only on foot in the world: no menu, no conversation, not on a horse,
//   not swimming, not knocked down, not exhausted. The gate closing
//   mid-kick gives the leg back at once.
//
// Standing it staggers (the shove's light kind), running it knocks down
// (the hard kind; decision 3). Pure, covered by kick_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BodyIkLogic.h"
#include "game/NiMath.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

struct KickSettings {
	bool enabled = true;            // [Hands] Kick
	float cooldownSeconds = 0.8f;   // KickCooldownSeconds, from the press
	float outSeconds = 0.12f;       // KickOutSeconds
	float holdSeconds = 0.08f;      // KickHoldSeconds
	float backSeconds = 0.2f;       // KickBackSeconds
	float reachShare = 0.92f;       // KickReachShare: of the leg's length, hip to foot
	float dropDegrees = 20.0f;      // KickDropDegrees: below the hip's level
};

struct KickGate {
	bool inWorld = false;
	bool menuUp = false;
	bool inDialogue = false;
	bool mounted = false;
	bool swimming = false;
	bool knockedDown = false;
	bool exhausted = false;
};

inline bool KickAllowed(const KickSettings& s, const KickGate& g) {
	return s.enabled && g.inWorld && !g.menuUp && !g.inDialogue && !g.mounted && !g.swimming && !g.knockedDown &&
	       !g.exhausted;
}

enum class KickPhase : UInt8 { Ready, Out, Held, Back };

struct KickState {
	KickPhase phase = KickPhase::Ready;
	float seconds = 0.0f;   // in the phase
	float cooldown = 0.0f;  // until the next press may kick
	UInt32 serial = 0;      // grows at every kick
};

struct KickFrame {
	bool started = false;   // the press kicked this frame
	float weight = 0.0f;    // the leg's share of the way to the peak
	bool striking = false;  // the foot meets whom it reaches
};

inline float KickEase(float x) {
	const float t = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
	return t * t * (3.0f - 2.0f * t);
}

// One frame: `pressed` the B's press this frame (an edge), `allowed`
// KickAllowed.
inline KickFrame StepKick(KickState& k, const KickSettings& s, bool pressed, bool allowed, float dt) {
	KickFrame f;
	const float step = dt > 0.0f ? dt : 0.0f;
	k.cooldown = k.cooldown > step ? k.cooldown - step : 0.0f;
	if (!allowed) {
		k.phase = KickPhase::Ready;
		k.seconds = 0.0f;
		return f;
	}
	if (k.phase == KickPhase::Ready) {
		if (!pressed || k.cooldown > 0.0f) {
			return f;
		}
		k.phase = KickPhase::Out;
		k.seconds = 0.0f;
		k.cooldown = s.cooldownSeconds;
		++k.serial;
		f.started = true;
	}
	k.seconds += step;
	if (k.phase == KickPhase::Out && k.seconds >= s.outSeconds) {
		k.seconds -= s.outSeconds;
		k.phase = KickPhase::Held;
	}
	if (k.phase == KickPhase::Held && k.seconds >= s.holdSeconds) {
		k.seconds -= s.holdSeconds;
		k.phase = KickPhase::Back;
	}
	if (k.phase == KickPhase::Back && k.seconds >= s.backSeconds) {
		k.phase = KickPhase::Ready;
		k.seconds = 0.0f;
		return f;
	}
	switch (k.phase) {
		case KickPhase::Out:
			f.weight = KickEase(s.outSeconds > 0.0f ? k.seconds / s.outSeconds : 1.0f);
			f.striking = k.seconds >= 0.5f * s.outSeconds;
			break;
		case KickPhase::Held:
			f.weight = 1.0f;
			f.striking = true;
			break;
		case KickPhase::Back:
			f.weight = 1.0f - KickEase(s.backSeconds > 0.0f ? k.seconds / s.backSeconds : 1.0f);
			break;
		case KickPhase::Ready:
			break;
	}
	return f;
}

// The peak of the foot's way: from `hip` along `facing` taken level, at
// reachShare of `legLength`, dropDegrees below the hip's level. No level
// facing (straight up or down): straight down from the hip.
inline NiPoint3 KickPeak(const NiPoint3& hip, const NiPoint3& facing, float legLength, const KickSettings& s) {
	const float reach = legLength * s.reachShare;
	const float flat = math::Sqrt(facing.x * facing.x + facing.y * facing.y);
	if (flat < 1e-4f) {
		return NiPoint3{hip.x, hip.y, hip.z - reach};
	}
	const float drop = s.dropDegrees * math::kDegreesToRadians;
	const float ahead = reach * math::Cos(drop) / flat;
	return NiPoint3{hip.x + facing.x * ahead, hip.y + facing.y * ahead, hip.z - reach * math::Sin(drop)};
}

// Where the IK puts the foot this frame: from the animation's `resting`
// foot to `peak` by `weight` (held to 0..1).
inline NiPoint3 KickFootTarget(const NiPoint3& resting, const NiPoint3& peak, float weight) {
	const float w = weight < 0.0f ? 0.0f : (weight > 1.0f ? 1.0f : weight);
	return NiPoint3{resting.x + (peak.x - resting.x) * w, resting.y + (peak.y - resting.y) * w,
	                resting.z + (peak.z - resting.z) * w};
}

inline ShoveKind KickKindFor(const KickSettings&, bool running) {
	return running ? ShoveKind::Hard : ShoveKind::Light;
}

}  // namespace obvr::game
