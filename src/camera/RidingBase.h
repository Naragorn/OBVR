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
// horse's place, turned into the horse's frame (its scene node's rotation -
// the heading, and the pitch of a body on a slope), settles with a first-order
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
	// The settled saddle (the rider's pelvis, camera/RidingView.cpp) in the
	// node's frame: right, forward, up; and how far
	// the eyes stand upright over it, 0 (turned with the whole body) to 1.
	bool saddleValid = false;
	NiPoint3 saddle{0.0f, 0.0f, 0.0f};
	float upright = 0.0f;
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

// The frame the offset is held in: the horse's scene node's world rotation.
// It carries the heading and, on a slope, the pitch the engine gives the
// body with the ground (horse-ride-slope.txt, 2026-10-23: the node up to 13.6
// degrees nose-up on the priory's hill, its roll 0, the reference's rotX and
// rotY 0, the Bip01 bone level). In it the saddle's offset holds within the
// stride's bob through a climb (-12.6 to -3.3 units forward, as on the flat);
// a frame of the heading alone read the same climb as a camera moved 54 units
// back and lagged the eyes 26 units ahead of the saddle - a Quest 3 tester's
// "the camera gets 'thrown' out of the saddle" (beta 6, 2026-10-22). A heading
// alone gives its yaw matrix: forward (sin h, cos h, 0), right (cos h, -sin h,
// 0), the engine's rotZ as the camera's own yaw reads in the riding trace.
inline NiMatrix33 RidingFrameFromHeading(float heading) {
	const float s = std::sin(heading);
	const float c = std::cos(heading);
	// rot * (1,0,0) is the right, rot * (0,1,0) the forward: the columns, in
	// row-major data[row][col].
	NiMatrix33 m = NiMatrix33::Identity();
	m.data[0][0] = c;
	m.data[1][0] = -s;
	m.data[0][1] = s;
	m.data[1][1] = c;
	return m;
}

// The engine's camera offset from the horse's place in the frame: forward,
// right and up along the frame's own axes - its inverse, not its transpose,
// since a node's rotation may carry a scale (game/NiMath.h).
inline void RidingOffsetInFrame(const NiPoint3& camera, const NiPoint3& horseAt, const NiMatrix33& frame,
                                float& forward, float& right, float& up) {
	const NiPoint3 local = InverseMatrix(frame) * (camera - horseAt);
	right = local.x;
	forward = local.y;
	up = local.z;
}

// The inverse: an offset in the frame back into the world at the horse's
// place.
inline NiPoint3 RidingOffsetToWorld(const NiPoint3& horseAt, const NiMatrix33& frame, float forward, float right,
                                    float up) {
	return horseAt + frame * NiPoint3{right, forward, up};
}

// The same for a heading alone (the engine's rotZ): the yaw matrix of it.
inline void RidingOffsetInHorseFrame(const NiPoint3& camera, const NiPoint3& horseAt, float heading, float& forward,
                                     float& right, float& up) {
	RidingOffsetInFrame(camera, horseAt, RidingFrameFromHeading(heading), forward, right, up);
}

inline NiPoint3 RidingOffsetToWorld(const NiPoint3& horseAt, float heading, float forward, float right, float up) {
	return RidingOffsetToWorld(horseAt, RidingFrameFromHeading(heading), forward, right, up);
}

// One frame: the settled offset followed, the base returned. The first frame
// on a horse (or on another horse) takes the engine's camera as it is, so the
// mount does not jump; from then on the offset lags behind the camera's with
// the time constant above, and the base is the settled offset plus bobShare
// of what the camera is off it. deltaSeconds outside (0, 1) counts as a
// whole step towards the camera, which is what a bad frame time deserves.
inline NiPoint3 StepRidingBase(RidingBaseState& state, UInt32 horse, const NiPoint3& camera, const NiPoint3& horseAt,
                               const NiMatrix33& frame, float deltaSeconds, float bobShare) {
	float forward = 0.0f;
	float right = 0.0f;
	float up = 0.0f;
	RidingOffsetInFrame(camera, horseAt, frame, forward, right, up);
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
	return RidingOffsetToWorld(horseAt, frame, f, r, u);
}

// The same with the heading alone as the frame: a yaw-only frame, which reads
// a body's tilt on a slope as a move of the camera (the tests keep it as the
// contrast; the game passes the node's rotation).
inline NiPoint3 StepRidingBase(RidingBaseState& state, UInt32 horse, const NiPoint3& camera, const NiPoint3& horseAt,
                               float heading, float deltaSeconds, float bobShare) {
	return StepRidingBase(state, horse, camera, horseAt, RidingFrameFromHeading(heading), deltaSeconds, bobShare);
}

// ------------------------------------------------- Upright over the saddle
//
// The node's frame holds the saddle on the body through a climb, and with it
// the whole rider: the engine's camera turned with the body about the
// horse's place on the ground. A rider does not lean with the horse; he
// stays upright over the saddle. Turned with the body the eyes rode the
// pitch, 59 units back of the horse's place at 18 degrees nose-up (measured
// 2026-11-08, horse-ride-slopes.txt), and with the head turned that ride
// back reads as a slide sideways - a Quest 3 tester, beta 8, 2026-11-06:
// "the camera shifts sideways during ascents or descents (as the horse's
// body tilts)". So the saddle rides the tilted body, and what stands above
// it - the eyes' offset from it - is turned by the heading alone.

// The time the eyes take to come upright after the mount (and to go back
// when the saddle is lost): the first frame on a horse is the engine's
// camera as it is, and on a slope the upright eyes stand (eyes - saddle)
// sin(pitch) from it - 20 units at 18 degrees, which half a second spreads
// over 37 frames at 75 Hz.
inline constexpr float kRidingUprightBlendSeconds = 0.5f;

// The heading's frame of a tilted node: its forward laid level (or, nose
// straight up or down, its right) and the world's up, scaled as the node's
// up column is (a reference's scale). False for a node with no heading in it
// - collapsed, or a NaN.
inline bool UprightFrameOf(const NiMatrix33& frame, NiMatrix33& upright) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (!std::isfinite(frame.data[r][c])) {
				return false;
			}
		}
	}
	const float scale = std::sqrt(frame.data[0][2] * frame.data[0][2] + frame.data[1][2] * frame.data[1][2] +
	                              frame.data[2][2] * frame.data[2][2]);
	if (!(scale > 1e-4f)) {
		return false;
	}
	// The forward column laid level; failing that the right column turned a
	// quarter back to a forward (right (c, -s) is forward (s, c)).
	float fx = frame.data[0][1];
	float fy = frame.data[1][1];
	float level = std::sqrt(fx * fx + fy * fy);
	if (!(level > 1e-3f * scale)) {
		const float rx = frame.data[0][0];
		const float ry = frame.data[1][0];
		fx = -ry;
		fy = rx;
		level = std::sqrt(fx * fx + fy * fy);
		if (!(level > 1e-3f * scale)) {
			return false;
		}
	}
	const float s = fx / level;  // sin of the heading
	const float c = fy / level;  // cos of the heading
	upright = NiMatrix33::Identity();
	upright.data[0][0] = c * scale;
	upright.data[1][0] = -s * scale;
	upright.data[0][1] = s * scale;
	upright.data[1][1] = c * scale;
	upright.data[2][2] = scale;
	return true;
}

// The eyes' offset from the horse's place: the saddle on the body, the eyes'
// offset from it turned by `share` of the way from the body's frame to the
// upright one. Saddle and eyes in the node's frame (right, forward, up).
inline NiPoint3 UprightOverSaddle(const NiMatrix33& frame, const NiMatrix33& upright, const NiPoint3& saddle,
                                  const NiPoint3& eyes, float share) {
	const NiPoint3 above = eyes - saddle;
	const NiPoint3 turned = frame * above;
	const NiPoint3 stood = upright * above;
	return frame * saddle + turned + (stood - turned) * share;
}

// One frame with the saddle: StepRidingBase above, its base then stood
// upright over the settled saddle. `saddle` is the rider's pelvis's place
// in the node's frame, nullptr (or a NaN) when it could not be read; the
// share of upright then goes back to 0 over the blend, and with it the base
// to the body's frame. The saddle settles like the camera's offset (the
// stride's bob of the spine followed slowly, a jump of over kRidingSnapUnits
// taken whole).
inline NiPoint3 StepRidingBase(RidingBaseState& state, UInt32 horse, const NiPoint3& camera, const NiPoint3& horseAt,
                               const NiMatrix33& frame, const NiPoint3* saddle, float deltaSeconds, float bobShare) {
	const bool fresh = !state.valid || state.horse != horse;
	const NiPoint3 base = StepRidingBase(state, horse, camera, horseAt, frame, deltaSeconds, bobShare);
	const bool saddleRead =
		saddle != nullptr && std::isfinite(saddle->x) && std::isfinite(saddle->y) && std::isfinite(saddle->z);
	NiMatrix33 upright{};
	const bool uprightRead = UprightFrameOf(frame, upright);
	if (fresh) {
		state.upright = 0.0f;
		state.saddleValid = saddleRead;
		if (saddleRead) {
			state.saddle = *saddle;
		}
		return base;
	}
	if (!saddleRead) {
		state.saddleValid = false;
	} else if (!state.saddleValid) {
		state.saddleValid = true;
		state.saddle = *saddle;
	} else {
		const NiPoint3 err = *saddle - state.saddle;
		if (std::fabs(err.x) > kRidingSnapUnits || std::fabs(err.y) > kRidingSnapUnits ||
		    std::fabs(err.z) > kRidingSnapUnits) {
			state.saddle = *saddle;
		} else {
			float k = 1.0f;
			if (deltaSeconds > 0.0f && deltaSeconds < 1.0f) {
				k = std::fmin(1.0f, deltaSeconds / kRidingSettleSeconds);
			}
			state.saddle = state.saddle + err * k;
		}
	}
	const bool usable = state.saddleValid && uprightRead;
	float step = 1.0f;
	if (deltaSeconds > 0.0f && deltaSeconds < 1.0f) {
		step = deltaSeconds / kRidingUprightBlendSeconds;
	}
	state.upright = usable ? std::fmin(1.0f, state.upright + step) : std::fmax(0.0f, state.upright - step);
	if (!(state.upright > 0.0f) || !uprightRead) {
		return base;
	}
	// The base's offset from the horse's place, back in the node's frame,
	// stood upright over the saddle.
	float f = 0.0f;
	float r = 0.0f;
	float u = 0.0f;
	RidingOffsetInFrame(base, horseAt, frame, f, r, u);
	return horseAt + UprightOverSaddle(frame, upright, state.saddle, NiPoint3{r, f, u}, state.upright);
}

// Off the horse: the next mount starts anew.
inline void ResetRidingBase(RidingBaseState& state) { state = RidingBaseState{}; }

}  // namespace obvr::camera
