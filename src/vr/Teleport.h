#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/TeleportSound.h"

namespace obvr::vr {

// The teleport of the Full VR mode (2026-09-27, agreed with the tester):
// the right stick pushed forward shows an arc from the right hand with a
// ring where it lands, letting go moves the player there - gliding fast by
// default, or with a short fade to black. It is a dodge step more than a
// way to travel: a few metres, the price of a dodge roll in fatigue, and the
// player cannot be hurt while it runs. With Blink on, it also reaches places
// that walking cannot: roofs, ledges, the far side of a gap.
//
// Everything here is pure - the stick, the arc, what a landing is allowed to
// be, what it costs, how the move runs - so every flow is tested without a
// game (TeleportTest). The engine calls live in game::PlayerTeleport.

struct TeleportSettings {
	bool enabled = true;
	// How the move looks: false glides there (Alyx's Shift), true fades to
	// black and back (Alyx's Blink).
	bool instant = false;
	float rangeMetres = 4.0f;
	// The glide's speed, metres a second (kGlideSpeedMin..kGlideSpeedMax).
	float glideMetresPerSecond = 15.0f;
	// The instant mode's fade, each way, and whether it fades to black at
	// all: off, the view jumps there in one frame (2026-09-27, asked for).
	float fadeSeconds = 0.1f;
	bool instantFade = true;
	bool inCombat = true;
	bool vignette = true;
	// Places walking cannot reach: higher than a jump, across a gap. Off, the
	// landing must be ground the player could walk or jump to.
	bool blink = false;
	// On, a teleport is heard like walking: while it moves, the player's
	// movement flags read "walking forward", which is what the engine's
	// detection hears (game::SetTeleportNoise). Sneaking stays sneaking. Off,
	// a teleport makes no movement noise at all - the engine hears none from
	// a placement.
	bool makesNoise = false;
	// With makesNoise, the sound it makes when it lands (vr/TeleportSound.h).
	TeleportSound sound = kTeleportSoundDefault;
	// The fatigue: the vanilla dodge roll's price times this, plus, with Blink,
	// this much per metre climbed.
	float fatigueMult = 1.0f;
	float blinkFatiguePerMetreUp = 10.0f;
	// The stick: pushed past startThreshold within coneDegrees of straight
	// forward it aims; back under releaseThreshold it goes.
	float startThreshold = 0.8f;
	float releaseThreshold = 0.3f;
	float coneDegrees = 30.0f;
	// How long the stick is held up before it aims rather than jumps.
	float holdSeconds = 0.2f;
};

constexpr float kGlideSpeedMin = 5.0f;
constexpr float kGlideSpeedMax = 40.0f;

// ------------------------------------------------------------------ Stick
//
// The right stick pushed up does two things, told apart by time
// (2026-09-27, the tester's choice): flicked up and let go it jumps, as in
// Skyrim VR, where the right stick up is the jump in the standard Index
// layout and in VRIK's (reddit r/ValveIndex f1nqvd, r/skyrimvr clsav7);
// held up for holdSeconds it aims the teleport, and let go it goes.
//
// A push counts when it is past the start threshold and no more than the
// cone off straight up: a stick pushed diagonally while turning is a turn
// (in Half-Life: Alyx players teleport by accident while turning,
// reddit.com/r/ValveIndex/comments/ikzuca). While pushed the stick belongs
// to the push - no turning. It is let go under the release threshold. A
// grip, or the teleport becoming impossible, cancels an aim; the release
// then does nothing. With the teleport switched off, a push jumps at once.
// For a moment after a release the stick still belongs to it, so the spring
// back through the middle does not turn.

constexpr float kTeleportTurnLockSeconds = 0.25f;

struct TeleportStickState {
	bool pushed = false;
	bool aiming = false;
	bool cancelled = false;
	float heldSeconds = 0.0f;
	float lockSeconds = 0.0f;
};

struct TeleportStickVerdict {
	bool jump = false;       // a flick: jump now
	bool aiming = false;     // show the arc this frame
	bool commit = false;     // go now
	bool ownsStick = false;  // the stick does not turn this frame
};

inline bool InTeleportCone(float x, float y, float startThreshold, float coneDegrees) {
	if (!(y >= startThreshold)) {
		return false;
	}
	const float cone = coneDegrees > 0.0f ? (coneDegrees < 89.0f ? coneDegrees : 89.0f) : 0.0f;
	const float ax = x < 0.0f ? -x : x;
	// Off forward by atan(|x| / y): within the cone while |x| <= y tan(cone).
	return ax <= y * math::Tan(cone * math::kDegreesToRadians);
}

// `active`: the stick may jump at all (in the world, not in a menu).
// `teleportAllowed`: a hold may aim a teleport now.
inline TeleportStickVerdict StepTeleportStick(TeleportStickState& s, float x, float y,
                                              bool cancel, bool active, bool teleportAllowed,
                                              float dtSeconds, const TeleportSettings& settings) {
	TeleportStickVerdict v;
	if (!(x == x) || !(y == y)) {
		x = 0.0f;
		y = 0.0f;
	}
	const float dt = dtSeconds > 0.0f ? dtSeconds : 0.0f;
	if (s.lockSeconds > 0.0f) {
		s.lockSeconds -= dt;
		if (s.lockSeconds < 0.0f) {
			s.lockSeconds = 0.0f;
		}
		v.ownsStick = true;
	}
	if (!active) {
		s = TeleportStickState{};
		return v;
	}
	const float magnitude = math::Sqrt(x * x + y * y);
	if (!s.pushed) {
		if (!InTeleportCone(x, y, settings.startThreshold, settings.coneDegrees)) {
			return v;
		}
		s.pushed = true;
		s.aiming = false;
		s.cancelled = false;
		s.heldSeconds = 0.0f;
		if (!settings.enabled) {
			// No teleport: the push is the jump, at once.
			v.jump = true;
		}
	}
	v.ownsStick = true;
	if (magnitude < settings.releaseThreshold) {
		if (settings.enabled) {
			if (!s.aiming) {
				v.jump = true;
			} else if (!s.cancelled) {
				v.commit = true;
			}
		}
		s.pushed = false;
		s.aiming = false;
		s.cancelled = false;
		s.lockSeconds = kTeleportTurnLockSeconds;
		return v;
	}
	if (!settings.enabled) {
		return v;
	}
	s.heldSeconds += dt;
	if (!s.aiming && s.heldSeconds >= settings.holdSeconds) {
		// Held long enough: from here it is an aim, and the release no jump.
		s.aiming = true;
		s.cancelled = !teleportAllowed;
	}
	if (s.aiming && (cancel || !teleportAllowed)) {
		s.cancelled = true;
	}
	v.aiming = s.aiming && !s.cancelled;
	return v;
}

// -------------------------------------------------------------------- Arc
//
// A throw from the hand under a gravity of its own, fast enough that thrown
// at 45 degrees over flat ground it comes down at the range: v = sqrt(g R).
// The arc is walked in steps; each step is a segment the world is tested
// against, and the first hit is where it lands.

constexpr float kArcGravityMetres = 9.81f;
constexpr UInt32 kArcMaxPoints = 32;

inline float ArcSpeed(float rangeUnits, float gravityUnits) {
	const float g = gravityUnits > 0.0f ? gravityUnits : 1.0f;
	const float r = rangeUnits > 0.0f ? rangeUnits : 0.0f;
	return math::Sqrt(g * r);
}

// Where the arc is after t seconds; z is up (the game's axes).
inline NiPoint3 ArcPoint(const NiPoint3& origin, const NiPoint3& direction, float speed,
                         float gravityUnits, float t) {
	return NiPoint3{origin.x + direction.x * speed * t, origin.y + direction.y * speed * t,
	                origin.z + direction.z * speed * t - 0.5f * gravityUnits * t * t};
}

// The time step that spends the arc's points on the flight to the range and
// back down as far below: twice the 45-degree flight time, over the points.
inline float ArcTimeStep(float rangeUnits, float gravityUnits) {
	const float speed = ArcSpeed(rangeUnits, gravityUnits);
	const float g = gravityUnits > 0.0f ? gravityUnits : 1.0f;
	const float flight = 2.0f * speed * 0.70710678f / g;
	return 2.0f * flight / static_cast<float>(kArcMaxPoints - 1);
}

// --------------------------------------------------------------- Landing

enum class TeleportRefusal : UInt8 {
	None,
	NoGround,       // the arc hit nothing within its points
	TooSteep,       // a wall or a slope too steep to stand on
	TooFar,         // beyond the range
	TooHigh,        // higher than a jump, without Blink
	TooLow,         // further down than the range, without Blink
	Blocked,        // something in the way of the straight line there, without Blink
	NotEnoughFatigue,
	NotNow,         // in a menu, in combat when that is off, swimming, riding
};

// Walkable: the surface's normal no more than about 45 degrees from up.
constexpr float kLandingMinNormalUp = 0.7f;

struct LandingQuery {
	bool hit = false;
	NiPoint3 point{0.0f, 0.0f, 0.0f};
	float normalUp = 0.0f;  // the hit surface's normal, z component
	NiPoint3 feet{0.0f, 0.0f, 0.0f};
	float rangeUnits = 0.0f;
	float jumpUnits = 0.0f;  // how high the player can jump
	bool blink = false;
	// Without Blink: nothing in the way of the straight line from the player
	// to the landing at waist height, so the glide cannot pass through a wall.
	bool pathClear = true;
	float fatigueCost = 0.0f;
	float fatigueNow = 0.0f;
	bool allowedNow = true;
};

inline TeleportRefusal JudgeLanding(const LandingQuery& q) {
	if (!q.allowedNow) {
		return TeleportRefusal::NotNow;
	}
	if (!q.hit) {
		return TeleportRefusal::NoGround;
	}
	if (!(q.normalUp >= kLandingMinNormalUp)) {
		return TeleportRefusal::TooSteep;
	}
	const NiPoint3 d = q.point - q.feet;
	// A little slack over the range: the arc's own step can land a hair past it.
	const float range = q.rangeUnits * 1.05f;
	if (!(d.LengthSquared() <= range * range)) {
		return TeleportRefusal::TooFar;
	}
	if (!q.blink) {
		if (d.z > q.jumpUnits) {
			return TeleportRefusal::TooHigh;
		}
		if (-d.z > q.rangeUnits) {
			return TeleportRefusal::TooLow;
		}
		if (!q.pathClear) {
			return TeleportRefusal::Blocked;
		}
	}
	if (!(q.fatigueNow >= q.fatigueCost)) {
		return TeleportRefusal::NotEnoughFatigue;
	}
	return TeleportRefusal::None;
}

inline const char* TeleportRefusalName(TeleportRefusal r) {
	switch (r) {
	case TeleportRefusal::None: return "none";
	case TeleportRefusal::NoGround: return "no ground";
	case TeleportRefusal::TooSteep: return "too steep";
	case TeleportRefusal::TooFar: return "too far";
	case TeleportRefusal::TooHigh: return "higher than a jump";
	case TeleportRefusal::TooLow: return "too far down";
	case TeleportRefusal::Blocked: return "something in the way";
	case TeleportRefusal::NotEnoughFatigue: return "not enough fatigue";
	case TeleportRefusal::NotNow: return "not now";
	}
	return "?";
}

// -------------------------------------------------------------- Fatigue
//
// The price of the vanilla dodge roll (a jump with the block held, UESP
// Oblivion:Acrobatics), which the caller reads from the game, times the
// setting; with Blink, a surcharge per metre climbed, since a roof is more
// than a step. Never negative.
inline float TeleportFatigueCost(float dodgeCost, float riseMetres, const TeleportSettings& s) {
	const float mult = s.fatigueMult > 0.0f ? s.fatigueMult : 0.0f;
	float cost = (dodgeCost > 0.0f ? dodgeCost : 0.0f) * mult;
	if (s.blink && riseMetres > 0.0f && s.blinkFatiguePerMetreUp > 0.0f) {
		cost += riseMetres * s.blinkFatiguePerMetreUp;
	}
	return cost;
}

// The vanilla jump's price (cs.uesp.net, Fatigue Game Settings):
// (fFatigueJumpBase + fFatigueJumpMult * encumbrance / max) times
// fPerkJumpFatigueExpertMult from Acrobatics Expert on.
inline float JumpFatigueCost(float base, float mult, float encumbrance, float maxEncumbrance,
                             bool acrobaticsExpert, float expertMult) {
	const float load = maxEncumbrance > 0.0f ? encumbrance / maxEncumbrance : 0.0f;
	float cost = base + mult * load;
	if (acrobaticsExpert) {
		cost *= expertMult;
	}
	return cost > 0.0f ? cost : 0.0f;
}

// ----------------------------------------------------------------- Move
//
// Once committed the teleport runs on its own: the glide moves the player
// along the line at the set speed; the instant mode fades out, moves at the
// darkest point and fades back in. The player is untouchable from the
// commit until it is over.

enum class TeleportPhase : UInt8 { Idle, Gliding, FadingOut, FadingIn };

struct TeleportMove {
	TeleportPhase phase = TeleportPhase::Idle;
	NiPoint3 from{0.0f, 0.0f, 0.0f};
	NiPoint3 to{0.0f, 0.0f, 0.0f};
	float elapsed = 0.0f;
	float duration = 0.0f;
	bool fade = false;  // the instant mode fades to black and back
};

struct TeleportMoveStep {
	bool place = false;         // put the player at `at` this frame
	NiPoint3 at{0.0f, 0.0f, 0.0f};
	bool fadeOut = false;       // start the fade to black this frame
	bool fadeIn = false;        // start the fade back this frame
	bool finished = false;      // the move ended this frame
	bool invulnerable = false;  // the player cannot be hurt this frame
};

constexpr float kGlideMinSeconds = 0.05f;

inline float GlideSeconds(float distanceUnits, float metresPerSecond, float unitsPerMetre) {
	float speed = metresPerSecond;
	if (!(speed >= kGlideSpeedMin)) {
		speed = kGlideSpeedMin;
	} else if (speed > kGlideSpeedMax) {
		speed = kGlideSpeedMax;
	}
	const float seconds = distanceUnits / (speed * (unitsPerMetre > 0.0f ? unitsPerMetre : 1.0f));
	return seconds > kGlideMinSeconds ? seconds : kGlideMinSeconds;
}

inline TeleportMove StartTeleport(const NiPoint3& from, const NiPoint3& to,
                                  const TeleportSettings& s, float unitsPerMetre) {
	TeleportMove m;
	m.from = from;
	m.to = to;
	if (s.instant) {
		m.phase = TeleportPhase::FadingOut;
		m.fade = s.instantFade && s.fadeSeconds > 0.0f;
		m.duration = m.fade ? s.fadeSeconds : 0.0f;
	} else {
		m.phase = TeleportPhase::Gliding;
		m.duration = GlideSeconds(math::Sqrt((to - from).LengthSquared()), s.glideMetresPerSecond,
		                          unitsPerMetre);
	}
	return m;
}

inline TeleportMoveStep StepTeleportMove(TeleportMove& m, float dtSeconds) {
	TeleportMoveStep step;
	if (m.phase == TeleportPhase::Idle) {
		return step;
	}
	step.invulnerable = true;
	const bool first = m.elapsed == 0.0f;
	m.elapsed += dtSeconds > 0.0f ? dtSeconds : 0.0f;
	const float t = m.duration > 0.0f ? (m.elapsed < m.duration ? m.elapsed / m.duration : 1.0f)
	                                  : 1.0f;
	switch (m.phase) {
	case TeleportPhase::Gliding:
		step.place = true;
		step.at = m.from + (m.to - m.from) * t;
		if (t >= 1.0f) {
			m.phase = TeleportPhase::Idle;
			step.finished = true;
		}
		break;
	case TeleportPhase::FadingOut:
		step.fadeOut = first && m.fade;
		if (t >= 1.0f) {
			step.place = true;
			step.at = m.to;
			if (m.fade) {
				step.fadeIn = true;
				m.phase = TeleportPhase::FadingIn;
				m.elapsed = 0.0f;
			} else {
				m.phase = TeleportPhase::Idle;
				step.finished = true;
			}
		}
		break;
	case TeleportPhase::FadingIn:
		if (t >= 1.0f) {
			m.phase = TeleportPhase::Idle;
			step.finished = true;
		}
		break;
	case TeleportPhase::Idle:
		break;
	}
	return step;
}

}  // namespace obvr::vr
