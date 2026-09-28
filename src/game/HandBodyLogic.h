#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/GrabPhysics.h"
#include "game/NiMath.h"

namespace obvr::game {

// The decisions behind the hands' and the weapon's Havok bodies
// (docs/hand-weapon-collision-spec.md, game/HandBodies.h): pure, over plain
// values, covered by hand_body_test.

// ------------------------------------------------------------- the filter
//
// Layer 22: unnamed in the engine's table (0x00B2EB40), nothing refers to
// it, it has a row in the layer matrix (filled with every bit by 0x008A83C0)
// and the pick rows (0xA277F, 0x00535A7C) leave it out. The group is the
// player's, as HIGGS does: the hands then touch neither the player's own
// capsule nor what the player holds (both in that group), and stay out of
// every ray asked as the player's group (the grab ray, 0x0066DAAC). Bit 14
// (0x4000) switches a body's collision off (the rule 0x008A7F70, step 1:
// any layer but 29).
inline constexpr UInt32 kHandBodyLayer = 22;
inline constexpr UInt32 kFilterNoCollision = 0x4000;

inline UInt32 HandBodyFilter(UInt32 group, bool collides) {
	const UInt32 g = group != 0 ? (group & 0xFFFF) : kPlayerCollisionGroupFallback;
	return (g << 16) | kHandBodyLayer | (collides ? 0u : kFilterNoCollision);
}

// ------------------------------------------------------------- the shapes
//
// Each body is a capsule along its own y axis (the hand's forward, as the
// hand rotation has it - game::HandSegment): bhkCapsuleShape (0x00563BB0)
// takes its two end points and its radius in Havok units. The spec asked
// for a box for the hands; a capsule is the same constructor for hands and
// weapon, and a hand from wrist to fingertips is near enough one.
struct CapsuleSpec {
	NiPoint3 a{0.0f, 0.0f, 0.0f};  // Havok units, in the body's own frame
	NiPoint3 b{0.0f, 0.0f, 0.0f};
	float radius = 0.0f;
};

// The hand: from 4 units behind the grip to 9 ahead (game::HandSegment),
// 2.5 units round (about 3.5 cm - a hand's half width).
inline constexpr float kHandBodyBackUnits = 4.0f;
inline constexpr float kHandBodyAheadUnits = 9.0f;
inline constexpr float kHandBodyRadiusUnits = 2.5f;
// The blade: from the grip to the tip, 1.5 units round (about 2 cm).
inline constexpr float kBladeBodyRadiusUnits = 1.5f;
// A blade shorter than this is no blade (a bound that could not be read).
inline constexpr float kBladeMinUnits = 5.0f;

inline CapsuleSpec HandCapsule(float radiusUnits) {
	CapsuleSpec c;
	c.a = NiPoint3{0.0f, -kHandBodyBackUnits * kHavokPerUnit, 0.0f};
	c.b = NiPoint3{0.0f, kHandBodyAheadUnits * kHavokPerUnit, 0.0f};
	c.radius = radiusUnits * kHavokPerUnit;
	return c;
}

// False for a blade too short to be one: then the weapon hand keeps its hand.
inline bool BladeCapsule(float lengthUnits, float radiusUnits, CapsuleSpec& out) {
	if (!(lengthUnits >= kBladeMinUnits) || !(radiusUnits > 0.0f)) {
		return false;
	}
	out.a = NiPoint3{0.0f, 0.0f, 0.0f};
	out.b = NiPoint3{0.0f, lengthUnits * kHavokPerUnit, 0.0f};
	out.radius = radiusUnits * kHavokPerUnit;
	return true;
}

// A weapon body is made anew only when the blade's length has changed by
// more than this (another weapon): the bound wobbles a little frame to frame.
inline constexpr float kBladeRebuildUnits = 3.0f;

inline bool BladeNeedsRebuild(float builtUnits, float nowUnits) {
	const float d = builtUnits - nowUnits;
	return d > kBladeRebuildUnits || d < -kBladeRebuildUnits;
}

// ------------------------------------------------------------- the drive
//
// A keyframed body is moved by its velocities: Havok integrates them over
// each step. So each frame the body gets the velocity that reaches the
// tracked pose in one step (HIGGS's applyHardKeyFrame, gain 1). The step is
// the planner's length (0x00BA790C, 1/60 s), whatever the frame: at 90 fps
// about one frame in three has no step, and a velocity for the frame's time
// would fall short. Too far to reach in a step at a sane speed (a teleport,
// a recenter, the first frame) it is placed there instead.
inline constexpr float kHandBodyDefaultStepSeconds = 1.0f / 60.0f;
// 30 m/s and 12 turns a second: a swung sword's tip, not a teleport.
inline constexpr float kHandBodyMaxHavokPerSecond = 300.0f;
inline constexpr float kHandBodyMaxRadiansPerSecond = 75.0f;
// Farther than this from the tracked pose, game units (about 60 cm): placed.
inline constexpr float kHandBodyPlaceGapUnits = 42.0f;

inline float HandBodyStepSeconds(float plannerStep) {
	return plannerStep > 0.001f && plannerStep <= 0.1f ? plannerStep : kHandBodyDefaultStepSeconds;
}

// The time the physics runs this frame: the planner's step count
// (0x00BA7914) times its length (0x00BA790C). The planner (0x00889810) has
// two ways: fixed steps of fMaxTime, the rest carried (0 steps on some
// frames), or - with iUpdateType set - the frame's whole time split into
// equal steps (at least one). A velocity for one step would carry the body
// n steps far. No steps, or a count past the cap of 3: one step's time.
inline float HandBodyDriveSeconds(UInt32 steps, float plannerStep) {
	const float step = HandBodyStepSeconds(plannerStep);
	return steps >= 1 && steps <= 3 ? step * static_cast<float>(steps) : step;
}

inline bool HandBodyPlaceInstead(float gapUnits) { return !(gapUnits <= kHandBodyPlaceGapUnits); }

// Linear: all of the gap in one step, clamped.
inline NiPoint3 HardKeyframeLinear(const NiPoint3& current, const NiPoint3& target, float stepSeconds) {
	const float dt = HandBodyStepSeconds(stepSeconds);
	NiPoint3 v = (target - current) * (1.0f / dt);
	const float speed = math::Sqrt(v.LengthSquared());
	if (speed > kHandBodyMaxHavokPerSecond) {
		v = v * (kHandBodyMaxHavokPerSecond / speed);
	}
	return v;
}

// Angular: the error rotation target * current^T as axis and angle, the
// short way round, all of it in one step, clamped (world axes).
inline NiPoint3 HardKeyframeAngular(const NiMatrix33& current, const NiMatrix33& target, float stepSeconds) {
	NiMatrix33 error;
	for (UInt32 i = 0; i < 3; ++i) {
		for (UInt32 j = 0; j < 3; ++j) {
			float sum = 0.0f;
			for (UInt32 k = 0; k < 3; ++k) {
				sum += target.data[i][k] * current.data[j][k];
			}
			error.data[i][j] = sum;
		}
	}
	float q[4];
	QuaternionFromRotation(error, q);
	if (q[3] < 0.0f) {
		q[0] = -q[0];
		q[1] = -q[1];
		q[2] = -q[2];
		q[3] = -q[3];
	}
	const float sine = math::Sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
	if (!(sine > 1e-6f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	const float angle = 2.0f * math::Atan2(sine, q[3]);
	float rate = angle / HandBodyStepSeconds(stepSeconds);
	if (rate > kHandBodyMaxRadiansPerSecond) {
		rate = kHandBodyMaxRadiansPerSecond;
	}
	const float scale = rate / sine;
	return NiPoint3{q[0] * scale, q[1] * scale, q[2] * scale};
}

// ------------------------------------------------------------- the lifecycle
//
// What happens to one body this frame. The body is only ever in the
// player's world, and only while it is wanted: out of it in menus and while
// loading (so a world torn down by a load never holds it), and moved when
// the player changes worlds (a door).
enum class HandBodyAction : UInt8 {
	None,    // not wanted, not in a world: nothing to do
	Create,  // wanted, not made yet
	Enter,   // made, wanted, in no world or another one: into the player's
	Drive,   // made, wanted, in the player's world: driven to the hand
	Leave,   // made, in a world, not wanted: taken out
	Abandon, // in a world that no longer looks like one: left alone, made anew
};

struct HandBodyState {
	bool wanted = false;        // setting on, in the world, no menu, first person, tracked
	bool made = false;          // the body exists
	UInt32 bodyWorld = 0;       // its hkWorld (body+8), 0 in none
	bool bodyWorldSound = true; // that world still looks like a world
	UInt32 playerWorld = 0;     // the player's hkWorld, 0 for none
};

inline HandBodyAction DecideHandBody(const HandBodyState& s) {
	if (s.made && s.bodyWorld != 0 && !s.bodyWorldSound) {
		return HandBodyAction::Abandon;
	}
	const bool want = s.wanted && s.playerWorld != 0;
	if (!want) {
		return s.made && s.bodyWorld != 0 ? HandBodyAction::Leave : HandBodyAction::None;
	}
	if (!s.made) {
		return HandBodyAction::Create;
	}
	return s.bodyWorld == s.playerWorld ? HandBodyAction::Drive : HandBodyAction::Enter;
}

// ------------------------------------------------------------- the step rate
//
// [Hands] PhysicsRate: 0 leaves the game's fixed 1/60 s step; otherwise the
// runtime copy of fMaxTime:HAVOK (0x00B2E2E8) is set to 1/rate, clamped to
// 30..180 Hz. Not the INI setting: a written setting persists to the user's
// INI (the iSize incident).
inline float PhysicsStepFor(float rateHz) {
	if (!(rateHz > 0.0f)) {
		return 0.0f;
	}
	const float r = rateHz < 30.0f ? 30.0f : (rateHz > 180.0f ? 180.0f : rateHz);
	return 1.0f / r;
}

}  // namespace obvr::game
