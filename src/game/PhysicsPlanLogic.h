#pragma once

// The physics steps a frame runs, planned so that every frame advances its
// own time in steps of one length (free-weapons-spec, open bugs of 2026-11-06:
// a thrown sword's flight stuttered at 45 fps).
//
// The engine's planner (0x00889810, OBNG re/functions/0x00889810.md, update
// type 0) runs whole steps of the maximum step [0x00B2E2E8] and carries the
// rest: below the step's own rate the frames get one step, then two. Measured
// 2026-11-20 with the game's 1/60 s step at 45 fps: one, one, two steps a
// frame, the thrown sword's model moving twice as far every third frame
// (weapon-throw-smooth-load12.txt). Above the step's rate it already runs one
// step of the frame's own length (P4), which is even.
//
// Here instead (Glenn Fiedler, "Fix Your Timestep!", the semi-fixed timestep,
// gafferongames.com/post/fix_your_timestep): the frame's time is split into
// the fewest equal steps no longer than the maximum (or a twentieth over it,
// kPhysicsStepSlack), so every frame's steps add up to its own time and each
// step is the same length. At most `cap`
// steps (the engine's 3, or 2 when its flag says so): a frame longer than cap
// steps runs cap whole steps and carries the rest, held to one step, as the
// engine does - the physics then runs slower than the clock, as before.
//
// Pure, covered by physics_plan_test; game/PhysicsPlan.cpp writes the plan
// into the engine's globals.

#include "core/Types.h"

namespace obvr::game {

struct PhysicsStepPlan {
	UInt32 steps = 0;          // 0x00BA7914
	float stepSeconds = 0.0f;  // 0x00BA790C
	float carry = 0.0f;        // 0x00BA7910, into the next frame
	float simulated = 0.0f;    // the time the steps cover
};

// A frame whose time is within this share of a step over a whole number of
// steps runs that number, each a little longer than the maximum: frames
// jitter about their period, and a frame of 11.4 ms at a step of 1/90 s
// would otherwise run two steps of 5.7 ms - twice the physics for nothing.
// Whole-millisecond frame seconds (OBVR's precise frame clock off) need it
// too: 33 ms at a 16.67 ms step is two steps, not three.
inline constexpr float kPhysicsStepSlack = 0.05f;

inline PhysicsStepPlan PlanEvenPhysicsSteps(float frameSeconds, float carry, float maxStepSeconds, UInt32 cap) {
	PhysicsStepPlan p;
	const float before = carry == carry && carry > 0.0f ? carry : 0.0f;
	const float t = frameSeconds + before;
	if (!(t > 0.0f) || !(t < 1.0e6f)) {
		p.carry = before;  // nothing to run, the carry kept (the engine's P1)
		return p;
	}
	const UInt32 most = cap >= 1 ? cap : 1;
	if (!(maxStepSeconds > 0.0f) || !(maxStepSeconds < 1.0e6f)) {
		p.steps = 1;  // no step to hold to: one step of the frame (the engine's P4)
		p.stepSeconds = t;
		p.simulated = t;
		return p;
	}
	const float ratio = t / maxStepSeconds;
	UInt32 n = static_cast<UInt32>(ratio);
	if (ratio - static_cast<float>(n) > kPhysicsStepSlack) {
		++n;
	}
	if (n < 1) {
		n = 1;
	}
	if (n <= most) {
		p.steps = n;
		p.stepSeconds = t / static_cast<float>(n);
		p.simulated = t;
		return p;
	}
	p.steps = most;
	p.stepSeconds = maxStepSeconds;
	p.simulated = maxStepSeconds * static_cast<float>(most);
	const float rest = t - p.simulated;
	p.carry = rest < maxStepSeconds ? rest : maxStepSeconds;
	return p;
}

}  // namespace obvr::game
