#pragma once

// The kick (docs/physical-combat-spec.md section 11; the tester, 2026-10-25:
// "ein kick vom player wie in B&S auf button druck ... ein fuß schiesst dann
// voraus wie in B&S und staggert gegner"; Nadi, 2026-10-30: "go B" - the
// foot seen, by the leg IK, or no kick at all). The weapon hand's B in the
// world kicks once (vr::PlanHandControls); the weapon side's leg is driven by
// the body's leg IK (game/BodyIk.cpp, StepBodyLegs) from where the animation
// has the foot to a point ahead of the hip and back:
//
// - out in KickOutSeconds, eased; held KickHoldSeconds, or while the B is
//   held up to KickHoldMaxSeconds; back in
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
// Standing it staggers (the shove's light kind), running or in a jump it
// knocks down (the hard kind; decision 3). Pure, covered by kick_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/BodyIkLogic.h"
#include "game/NiMath.h"
#include "game/ShoveLogic.h"

namespace obvr::game {

struct KickSettings {
	bool enabled = true;            // [Hands] Kick
	float cooldownSeconds = 0.8f;   // KickCooldownSeconds, from the press
	float outSeconds = 0.18f;       // KickOutSeconds
	float holdSeconds = 0.12f;      // KickHoldSeconds: a tap's
	float holdMaxSeconds = 0.8f;    // KickHoldMaxSeconds: the B held keeps the foot out up to this
	float backSeconds = 0.3f;       // KickBackSeconds
	float reachShare = 0.92f;       // KickReachShare: of the leg's length, hip to foot
	float dropDegrees = 20.0f;      // KickDropDegrees: below the hip's level
	float footUnits = 6.0f;         // the foot's ball, swept along its way
	float fatigue = 15.0f;          // KickFatigue: once a kick that meets someone
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
// KickAllowed, `held` the B down now. Held, the foot stays out past
// KickHoldSeconds until the B is let go or KickHoldMaxSeconds is reached (Nadi,
// 2026-10-30: "rechts b kick mehr wie B&S also wenn ich den halte hat der
// kick mehr zeit und schwung"); a tap keeps KickHoldSeconds.
inline KickFrame StepKick(KickState& k, const KickSettings& s, bool pressed, bool allowed, float dt,
                          bool held = false) {
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
	if (k.phase == KickPhase::Held) {
		const float hold = held && s.holdMaxSeconds > s.holdSeconds ? s.holdMaxSeconds : s.holdSeconds;
		if (k.seconds >= hold) {
			// What is past the hold: a frame's worth at most, so a release long
			// after KickHoldSeconds starts the way back rather than skipping it.
			const float over = k.seconds - hold;
			k.seconds = over < step ? over : step;
			k.phase = KickPhase::Back;
		}
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

// Who the kick has met (the cleave, decision 5): each person once a kick.
inline constexpr UInt32 kKickMetMax = 8;

struct KickMet {
	UInt32 serial = 0;
	UInt32 count = 0;
	UInt32 actor[kKickMetMax] = {};
};

// The people whose capsules the foot's ball meets along its way this frame
// (`from` to `to`, footUnits round), not met before in kick `serial`: put in
// `out` (at most outMax) and kept in `met`. Answers how many.
inline UInt32 KickTouches(const BladeBodies& bodies, const NiPoint3& from, const NiPoint3& to, float footUnits,
                          KickMet& met, UInt32 serial, UInt32* out, UInt32 outMax) {
	if (met.serial != serial) {
		met = KickMet{};
		met.serial = serial;
	}
	UInt32 n = 0;
	for (UInt32 c = 0; c < bodies.count && n < outMax; ++c) {
		const BladeBodyCapsule& cap = bodies.cap[c];
		bool seen = false;
		for (UInt32 i = 0; i < met.count; ++i) {
			seen = seen || met.actor[i] == cap.actor;
		}
		if (seen || met.count >= kKickMetMax) {
			continue;
		}
		if (SegmentSegmentDistance(from, to, cap.a, cap.b) <= cap.radius + footUnits) {
			met.actor[met.count++] = cap.actor;
			out[n++] = cap.actor;
		}
	}
	return n;
}

// The shove's rules as the kick takes them (ShoveLogic.h): always told as a
// hit, standing too (decision 1); its fatigue its own, once a kick
// (KickSettings::fatigue), not per person.
// With `engineHit` the engine's own fist hit landed first (KickAnswersFists):
// it told them of the blow itself - a crime, their answer - so the shove
// does not again.
inline ShoveSettings KickShoveSettings(const ShoveSettings& shove, const KickSettings&, bool engineHit) {
	ShoveSettings k = shove;
	k.countsAsHit = !engineHit;
	k.lightCountsAsHit = !engineHit;
	k.fatigueLight = 0.0f;
	k.fatigueHard = 0.0f;
	return k;
}

inline ShoveKind KickKindFor(const KickSettings&, bool running, bool airborne) {
	return running || airborne ? ShoveKind::Hard : ShoveKind::Light;
}

// The player's character controller's state (hkCharacterStateType, xOBSE
// HavokCharacters.h; the controller's context at +0x1E0, its state at
// +0x0C - NiHavokCharacters.h, IsInAir/IsJumping in Commands_AI.cpp read
// the same): -1 when unread.
inline constexpr SInt32 kCharacterStateOnGround = 0;
inline constexpr SInt32 kCharacterStateJumping = 1;
inline constexpr SInt32 kCharacterStateInAir = 2;
inline constexpr SInt32 kCharacterStateSwimming = 5;

// In a jump (decision 3): jumping or in the air.
inline bool KickAirborne(SInt32 state) {
	return state == kCharacterStateJumping || state == kCharacterStateInAir;
}
inline bool KickSwimming(SInt32 state) { return state == kCharacterStateSwimming; }

// The sounds, the game's own (Nadi, 2026-10-30: "sounds aus dem oblivion
// ingame"), SOUN forms of Oblivion.esm read 2026-10-30: the fist's swish as
// the foot goes out, the fist's hit at a stagger, the blunt weapon on flesh
// at a knockdown.
inline constexpr UInt32 kSoundSwishHand = 0x00088834;     // WPNSwishHand, fx/wpn/swish/hand/
inline constexpr UInt32 kSoundHitHand = 0x00025413;       // WPNHitHand, fx/wpn/weaponhit/hand/
inline constexpr UInt32 kSoundHitBluntFlesh = 0x0018B9F2;  // WPNHitBluntFlesh, fx/wpn/weaponhit/bluntflesh/

// With `engineHit` the engine's fist hit played its own sound: a stagger
// adds none, a knockdown the heavy one on top.
inline UInt32 KickHitSound(ShoveKind kind, bool engineHit) {
	if (kind == ShoveKind::Hard) {
		return kSoundHitBluntFlesh;
	}
	return kind == ShoveKind::Light && !engineHit ? kSoundHitHand : 0;
}

// The damage (decision 2: "soviel wie in h2h angriff"; Nadi, 2026-10-30:
// "ja" to C1): the engine's own hit (AttackHandling 0x005FEBF0, as the strike
// by motion calls it) with the kicked one as its target and the power flag
// running or in a jump. It reads the attacker's weapon through its process's
// vtable +0xEC (0x0064B240, the equipped weapon at +0xE4; read in OBNG's
// decompile 0x5febf0.c, lines 180, 592, 902, 907) and with none takes its
// hand-to-hand branch (Strength, Luck, HandToHand - lines 310-327); so while
// the kick strikes, that getter answers the player's process "none", and the
// engine reckons a fist's blow whatever is in the hand - armour, block,
// experience, blood, its sound, the crime.
inline bool KickAnswersFists(UInt32 process, UInt32 fistsProcess) {
	return fistsProcess != 0 && process == fistsProcess;
}

}  // namespace obvr::game
