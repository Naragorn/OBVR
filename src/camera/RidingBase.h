#pragma once

#include <cmath>

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::camera {

// The base of the eyes on a horse.
//
// Mounted, the engine hangs the first-person skeleton with its camera node
// under the horse's spine (Camera01 < ... < ActorParent < Bip02 Spine2, the
// chain read 2026-10-19), so the first-person camera rides the walk and the
// gallop animation: a position bob only - height, surge and sway, no tilt,
// the yaw exactly the horse's heading (horse-ride-steer.txt, 2026-10-20: at a
// walk 4.1 units of height, 7.9 of surge, 4.0 of sway over the horse's place,
// a cycle of about a second). A monitor takes that as the feel of a ride; a
// headset takes it as the floor moving under a still body - "the camera moves
// too much with the horse's motion", nauseating (Nadi, 2026-10-19).
//
// So the base is held in the horse's own frame: the camera's offset from the
// horse's place, turned into the horse's heading, settles with a first-order
// lag of kRidingSettleSeconds - the slow part (the saddle's height, the
// terrain, the horse's turn) is followed, the cadence is not - and a share of
// what is left (HorseBob, 0 to 1) is laid back on top so the ride can still
// read as a ride. 1 is the engine's camera untouched, 0 a steady saddle.
//
// Pure arithmetic over plain values, so every flow is a unit test: the first
// frame on a horse, the settle, the share at 0 and 1, a horse that turns, a
// change of horse, a bad frame time, a camera that moved rather than bobbed.

struct RidingBaseState {
	bool valid = false;
	// Which horse the offset was settled on; a different one starts anew.
	UInt32 horse = 0;
	// The settled offset of the engine's camera from the horse's place, in
	// the horse's frame: along its heading, to its right, up.
	float forward = 0.0f;
	float right = 0.0f;
	float up = 0.0f;
};

// The lag of the settled offset. The walk's cycle is about a second; two
// seconds leaves under a tenth of it (a first-order lag at 1 Hz with a time
// constant of 2 s passes 1/sqrt(1 + (2 pi 2)^2) ~ 0.08), while the saddle's
// height after the mount and the terrain's rise are followed within a few
// seconds.
inline constexpr float kRidingSettleSeconds = 2.0f;

// Farther than this from the settled offset, in any direction, the camera has
// not bobbed but moved: the engine's first-person camera arriving at the
// saddle after the mount (57.9 units over the horse's place at the switch to
// first person, 175 a frame later - horse-ride-steer.txt 10:08, where the
// settled offset then took eight seconds to climb after it), a re-placement
// of horse and rider, a snap back of a drifted body. The settled offset then
// jumps to the camera's. The gallop's bob stays well under it (height 22
// units peak to peak, surge 48 over a drift of twelve seconds).
inline constexpr float kRidingSnapUnits = 30.0f;

// The engine's camera offset from the horse's place, turned into the horse's
// frame. The heading is the engine's rotZ: forward is (sin h, cos h), right
// is (cos h, -sin h), as the camera's own yaw reads in the riding trace.
inline void RidingOffsetInHorseFrame(const NiPoint3& camera, const NiPoint3& horseAt, float heading, float& forward,
                                     float& right, float& up) {
	const float dx = camera.x - horseAt.x;
	const float dy = camera.y - horseAt.y;
	const float s = std::sin(heading);
	const float c = std::cos(heading);
	forward = dx * s + dy * c;
	right = dx * c - dy * s;
	up = camera.z - horseAt.z;
}

// The inverse: an offset in the horse's frame back into the world at the
// horse's place.
inline NiPoint3 RidingOffsetToWorld(const NiPoint3& horseAt, float heading, float forward, float right, float up) {
	const float s = std::sin(heading);
	const float c = std::cos(heading);
	return NiPoint3{horseAt.x + forward * s + right * c, horseAt.y + forward * c - right * s, horseAt.z + up};
}

// One frame: the settled offset followed, the base returned. The first frame
// on a horse (or on another horse) takes the engine's camera as it is, so the
// mount does not jump; from then on the offset lags behind the camera's with
// the time constant above, and the base is the settled offset plus bobShare
// of what the camera is off it. deltaSeconds outside (0, 1) counts as a
// whole step towards the camera, which is what a bad frame time deserves.
inline NiPoint3 StepRidingBase(RidingBaseState& state, UInt32 horse, const NiPoint3& camera, const NiPoint3& horseAt,
                               float heading, float deltaSeconds, float bobShare) {
	float forward = 0.0f;
	float right = 0.0f;
	float up = 0.0f;
	RidingOffsetInHorseFrame(camera, horseAt, heading, forward, right, up);
	if (!state.valid || state.horse != horse) {
		state.valid = true;
		state.horse = horse;
		state.forward = forward;
		state.right = right;
		state.up = up;
		return camera;
	}
	const float errF = forward - state.forward;
	const float errR = right - state.right;
	const float errU = up - state.up;
	if (errF > kRidingSnapUnits || errF < -kRidingSnapUnits || errR > kRidingSnapUnits || errR < -kRidingSnapUnits ||
	    errU > kRidingSnapUnits || errU < -kRidingSnapUnits) {
		state.forward = forward;
		state.right = right;
		state.up = up;
		return camera;
	}
	float k = 1.0f;
	if (deltaSeconds > 0.0f && deltaSeconds < 1.0f) {
		k = deltaSeconds / kRidingSettleSeconds;
		if (k > 1.0f) {
			k = 1.0f;
		}
	}
	state.forward += (forward - state.forward) * k;
	state.right += (right - state.right) * k;
	state.up += (up - state.up) * k;
	float share = bobShare;
	if (share < 0.0f) {
		share = 0.0f;
	} else if (share > 1.0f) {
		share = 1.0f;
	}
	const float f = state.forward + (forward - state.forward) * share;
	const float r = state.right + (right - state.right) * share;
	const float u = state.up + (up - state.up) * share;
	return RidingOffsetToWorld(horseAt, heading, f, r, u);
}

// Off the horse: the next mount starts anew.
inline void ResetRidingBase(RidingBaseState& state) { state = RidingBaseState{}; }

}  // namespace obvr::camera
