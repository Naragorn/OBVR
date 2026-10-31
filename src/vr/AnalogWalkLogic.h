#pragma once

// The left stick walks analog, as in Blade & Sorcery (Nadi, 2026-10-31: "mit
// linkem stick analog steuern statt schritt vor ja/nein wie in B&S", option A
// "bau a"): how far the stick is pushed sets the pace, which way it is pushed
// sets the exact heading.
//
// The keys still go to the game (the octant's W/A/S/D and the run key), so
// sneaking and the AI hear the walk as before. The pace is the engine's own
// gamepad route (read in Oblivion.exe 1.2.416, 2026-10-31): the player's
// handler 0x00671620 writes the stick's deflection (|stick| x 0.02, at most
// 1) into the float 0x00B14E58, and the movement animation's update
// 0x005FCAB0 plays the walk or run at GetSpeed / the animation's own speed x
// that factor (0x005FE03B..0x005FE06C) - and the step UpdateMovement
// 0x0063C730 hands to Actor::Move is that animation's root motion. So OBVR
// writes the factor (game/AnalogWalk.h): the legs and the step slow together,
// no feet sliding. The heading is the stick's: the octant's step is turned to
// it in the wrap of Actor::Move (game/RoomFollow.cpp), its length kept, so it
// still goes through the character controller - walls, slopes and stairs stop
// or carry it as before.
//
// The gait follows the push: a light push walks (the walk's legs, the pace
// from nothing up to the walk's speed), a push past runFrom runs (the run's
// legs, the pace from the walk's speed up to the run's) - so the legs never
// run while the body creeps. A run asked for otherwise (the stick pressed in,
// the run toggle, swung arms) runs at the full run's pace, as before; only
// the heading is the stick's.
//
// Pure, covered by analog_walk_test.

#include "core/MathFns.h"
#include "vr/HandInput.h"

namespace obvr::vr {

struct AnalogWalkSettings {
	bool enabled = true;     // [Hands] AnalogWalk
	float runFrom = 0.65f;   // [Hands] AnalogRunFrom: the push (0..1 past the dead zone) from which it runs
	// [Hands] AnalogDeadZone: the stick's own dead zone for the walk, round and
	// smaller than StickDeadZone (0.4, for the menus and flicks), so a light
	// push already creeps.
	float deadZone = 0.15f;
	// [Hands] SprintFactor: the sprint's pace over the run (1 = no sprint, the old
	// full run); SprintFatigue: what it costs a second on top of the run's own.
	float sprintFactor = 1.4f;
	float sprintFatiguePerSecond = 10.0f;
};

// Half the width of the band the gait does not switch in, so a push held at
// runFrom does not switch every frame.
inline constexpr float kAnalogGaitBand = 0.05f;

// The stick past its dead zone: amount 0..1 (0 inside the dead zone, 1 at the
// rim), and the unit direction it points (x right, y forward).
struct AnalogStick {
	float amount = 0.0f;
	float x = 0.0f;
	float y = 0.0f;
};

inline AnalogStick ReadAnalogStick(float x, float y, float deadZone) {
	AnalogStick s;
	if (!(x == x) || !(y == y)) {
		return s;
	}
	const float len = math::Sqrt(x * x + y * y);
	const float dz = deadZone > 0.0f && deadZone < 0.95f ? deadZone : 0.0f;
	if (!(len > dz) || !(len > 1e-6f)) {
		return s;
	}
	const float amount = (len - dz) / (1.0f - dz);
	s.amount = amount > 1.0f ? 1.0f : amount;
	s.x = x / len;
	s.y = y / len;
	return s;
}

// The keys of the octant the stick points into, so the engine walks at all
// whenever the stick is past its dead zone: an axis counts once the
// direction leans more than 22.5 degrees its way (sin 22.5 = 0.3827).
inline StickDirections AnalogStickKeys(const AnalogStick& s) {
	StickDirections d;
	if (!(s.amount > 0.0f)) {
		return d;
	}
	constexpr float kLean = 0.38268343f;
	d.forward = s.y > kLean;
	d.back = s.y < -kLean;
	d.right = s.x > kLean;
	d.left = s.x < -kLean;
	return d;
}

// The gait by the push, with a band around runFrom: true runs.
inline bool StepAnalogGait(bool& running, float amount, float runFrom) {
	const float from = runFrom > 0.0f && runFrom <= 1.0f ? runFrom : AnalogWalkSettings{}.runFrom;
	if (running) {
		running = amount >= from - kAnalogGaitBand;
	} else {
		// At runFrom 1 this is never reached: the push never runs.
		running = amount >= from + kAnalogGaitBand;
	}
	return running;
}

// The pace wanted, in the speeds' units: up to the walk's speed while
// walking (amount 0..runFrom), from the walk's up to the run's while running
// (amount runFrom..1). A forced run (stick in, toggle, arms) is the full
// run's speed. Continuous at runFrom, so the switch of the legs is no jump.
inline float AnalogTargetSpeed(float amount, bool running, bool forcedRun, float runFrom, float walkSpeed,
                               float runSpeed) {
	if (!(amount > 0.0f) || !(walkSpeed > 0.0f)) {
		return 0.0f;
	}
	const float a = amount > 1.0f ? 1.0f : amount;
	const float run = runSpeed > walkSpeed ? runSpeed : walkSpeed;
	if (forcedRun) {
		return run;
	}
	const float from = runFrom > 0.0f && runFrom <= 1.0f ? runFrom : AnalogWalkSettings{}.runFrom;
	if (!running) {
		const float k = a / from;
		return walkSpeed * (k > 1.0f ? 1.0f : k);
	}
	if (from >= 1.0f) {
		return run;
	}
	const float k = (a - from) / (1.0f - from);
	const float t = k < 0.0f ? 0.0f : (k > 1.0f ? 1.0f : k);
	return walkSpeed + (run - walkSpeed) * t;
}

// The engine's pace factor (0x00B14E58) for the pace wanted: the target over
// the speed of the gait the legs show - the run's while running or a run is
// forced, else the walk's - kept within 0..1 (the engine's own range for it).
// A speed unread: 1, the engine's pace, as vanilla.
inline float AnalogPaceFactor(float amount, bool running, bool forcedRun, float runFrom, float walkSpeed,
                              float runSpeed) {
	const float gait = running || forcedRun ? (runSpeed > walkSpeed ? runSpeed : walkSpeed) : walkSpeed;
	if (!(walkSpeed > 0.0f) || !(gait > 0.0f)) {
		return 1.0f;
	}
	const float k = AnalogTargetSpeed(amount, running, forcedRun, runFrom, walkSpeed, runSpeed) / gait;
	if (!(k == k)) {
		return 1.0f;
	}
	return k < 0.0f ? 0.0f : (k > 1.0f ? 1.0f : k);
}

// The step handed on: the engine's step (local x right, y forward, the
// octant's direction) turned to the stick's direction, its length kept - the
// pace is the factor's. z is left as it was. No engine step or no push: the
// engine's step unchanged.
inline void AnalogTurnStep(float step[3], const AnalogStick& stick) {
	const float len = math::Sqrt(step[0] * step[0] + step[1] * step[1]);
	if (!(len > 1e-6f) || !(stick.amount > 0.0f)) {
		return;
	}
	step[0] = stick.x * len;
	step[1] = stick.y * len;
}

// The run key for the gait wanted. The player's handler (0x00671620) runs on
// the run key held while Always Run (PlayerCharacter +0x58B) is off, and
// walks on it held while Always Run is on - so the key is the gait wanted,
// turned over when Always Run is on.
inline bool AnalogRunKey(bool wantRun, bool alwaysRun) { return wantRun != alwaysRun; }

// The sprint (Nadi, 2026-10-31: "dann brauchen wir rennen gar nicht mehr durch
// stick reindrücken oder arme schwingen. dann bräuchten wir ja sprinten",
// option A, "10 pro sekunde passt"). Oblivion has no sprint: the run is its
// fastest. The run asked otherwise - the stick pressed in, the run toggle,
// swung arms - becomes a sprint: the run's legs with the pace factor over 1
// (sprintFactor), costing sprintFatiguePerSecond on top of the run's own.
// Out of fatigue it is spent and runs by the push again, until the fatigue is
// back to kSprintResumeFatigue - so it does not flicker at the bottom.
// sprintFactor 1 (or less): no sprint, the old full run.
inline constexpr float kSprintResumeFatigue = 10.0f;
// Under this much left the sprint is spent: Oblivion knocks the player down at
// 0 fatigue and under, and the sprint must never be what does it.
inline constexpr float kSprintFloorFatigue = 1.0f;

struct SprintState {
	bool spent = false;
};

inline bool StepSprint(SprintState& s, bool asked, bool fatigueKnown, float fatigue, float factor) {
	if (!fatigueKnown || !(fatigue == fatigue)) {
		return false;
	}
	if (s.spent && fatigue >= kSprintResumeFatigue) {
		s.spent = false;
	}
	if (!asked || !(factor > 1.0f) || s.spent) {
		return false;
	}
	if (!(fatigue >= kSprintFloorFatigue)) {
		s.spent = true;
		return false;
	}
	return true;
}

// The pace factor of a sprint: 1..2 (twice the run at most), not a number 1.
inline float SprintPace(float factor) {
	if (!(factor == factor) || !(factor > 1.0f)) {
		return 1.0f;
	}
	return factor > 2.0f ? 2.0f : factor;
}

// The sprint's cost this frame: per second x dt, a frame longer than a
// quarter second (a hitch, a load) counted as one, never negative and never
// below half a point left (`left`, the fatigue now).
inline float SprintFatigueCost(float perSecond, float dt, float left) {
	if (!(perSecond > 0.0f) || !(dt > 0.0f) || !(left > 0.0f)) {
		return 0.0f;
	}
	const float cost = perSecond * (dt > 0.25f ? 0.25f : dt);
	// Half a point always left: the sprint never takes the fatigue to 0.
	const float most = left - 0.5f;
	return most > 0.0f ? (cost < most ? cost : most) : 0.0f;
}

// Whether the stick walks analog this frame: the setting on, the player's
// own walk by the stick (Full VR, in the world, no fly camera, not on a
// horse - the horse's walk is the horse's).
struct AnalogWalkGate {
	bool fullVr = false;
	bool inWorld = false;
	bool riding = false;
	bool flyCam = false;
};

inline bool AnalogWalkAllowed(const AnalogWalkSettings& s, const AnalogWalkGate& g) {
	return s.enabled && g.fullVr && g.inWorld && !g.riding && !g.flyCam;
}

}  // namespace obvr::vr
