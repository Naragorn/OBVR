#pragma once

// The body follows the head through the room (Nadi, 2026-10-31: "wenn ich
// ingame 6dof mäßig rumgehe bleibt der körper des players stehen und ich
// bewege mich sozusagen 'aus' dem körper raus ... wenn ich mich in echt
// rumbewege sollte der körper mitgehen"; docs/next-up.md section 6, "To build
// 1. The capsule follows the head").
//
// The head's offset from the recentered reference (HeadTracker, in the
// recentered frame, Oblivion units, z up) is split in two: what the
// character has already followed (`followed`, the same frame) and the rest,
// which stays a camera offset (a lean over a table, a head into a wall).
// Every frame the horizontal rest is asked of the character as a step; the
// engine's own move (Actor::Move, game/RoomFollow.cpp) takes it with the
// walk, through the character controller, so walls, slopes and stairs stop
// or carry it as they do walking. What the step achieved joins `followed`.
//
// Pure, covered by room_follow_test.

#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::vr {

inline constexpr float kRoomStepLeastUnits = 0.05f;

struct RoomFollowSettings {
	bool enabled = true;        // [Hands] RoomScaleBody (harness room-follow PASS 2026-10-31)
	float maxStepUnits = 20.0f;  // one frame's step at most (a tracking jump is not walked)
};

struct RoomFollowGate {
	bool fullVr = false;      // Full VR's hands active
	bool headset = false;     // a tracked head with a position
	bool firstPerson = false;
	bool inWorld = false;
	bool menuUp = false;
	bool riding = false;
	bool flyCam = false;
	bool knockedDown = false;
};

inline bool RoomFollowAllowed(const RoomFollowSettings& s, const RoomFollowGate& g) {
	return s.enabled && g.fullVr && g.headset && g.firstPerson && g.inWorld && !g.menuUp && !g.riding && !g.flyCam &&
	       !g.knockedDown;
}

// The step wanted this frame, in the head offset's frame: the horizontal part
// of what the head is ahead of the followed, at most maxStepUnits long.
inline NiPoint3 RoomStepWanted(const NiPoint3& head, const NiPoint3& followed, const RoomFollowSettings& s) {
	NiPoint3 want{head.x - followed.x, head.y - followed.y, 0.0f};
	const float len = math::Sqrt(want.x * want.x + want.y * want.y);
	// Under a twentieth of a unit (0.7 mm): nothing to walk - the rounding of
	// what was followed, not a step.
	if (!(len == len) || len < kRoomStepLeastUnits) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	if (s.maxStepUnits > 0.0f && len > s.maxStepUnits) {
		const float k = s.maxStepUnits / len;
		want.x *= k;
		want.y *= k;
	}
	return want;
}

// A world step (x, y) in the actor's own frame for its heading rotZ (forward
// (sin, cos), right (cos, -sin)): x to the right, y forward.
inline NiPoint3 WorldStepToActor(float wx, float wy, float rotZ) {
	const float s = math::Sin(rotZ);
	const float c = math::Cos(rotZ);
	return NiPoint3{c * wx - s * wy, s * wx + c * wy, 0.0f};
}

// How much of the room's step the move achieved, 0..1: the world movement
// (`moved`, x and y) against the whole step asked (`asked`: the walk's and the
// room's together), along the asked direction. A wall that stops it gives 0,
// a free floor 1; the share counts for the room's part alike.
inline float RoomStepShare(float movedX, float movedY, float askedX, float askedY) {
	const float len2 = askedX * askedX + askedY * askedY;
	if (!(len2 > 1e-6f)) {
		return 0.0f;
	}
	const float share = (movedX * askedX + movedY * askedY) / len2;
	if (!(share == share)) {
		return 0.0f;
	}
	return share < 0.0f ? 0.0f : (share > 1.0f ? 1.0f : share);
}

}  // namespace obvr::vr
