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
	// [Hands] RoomScaleStart: the lead that starts a walk, units. Under it
	// the head's lead stays a camera offset - a sway, a breath, the tracking's
	// own wander. Before 2026-11-02 every lead over a twentieth of a unit was
	// walked: under a head that only turned, the lead grew 0.016 units a frame
	// and the body stepped 0.05 units every four to five frames, and the
	// engine re-grounded the player at every step - on the rocky shore of
	// Lake Rumare 0.7 units down each time, which the water's reflection
	// showed as a "reset" (the harness runs water-turn-jitter*, the Move
	// trace: the feet moved only in the frames a room step was pending).
	float startUnits = 2.0f;
};

// Whether the body is in the middle of a walk after the head (the start
// zone is a gate on the start, not on the walk).
struct RoomFollowState {
	bool following = false;
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
// of what the head is ahead of the followed, at most maxStepUnits long. A
// walk starts once the lead passes s.startUnits and goes on every frame
// until the body has caught up (the lead under a twentieth of a unit).
inline NiPoint3 RoomStepWanted(const NiPoint3& head, const NiPoint3& followed, const RoomFollowSettings& s,
                               RoomFollowState& state) {
	NiPoint3 want{head.x - followed.x, head.y - followed.y, 0.0f};
	const float len = math::Sqrt(want.x * want.x + want.y * want.y);
	// Under a twentieth of a unit (0.7 mm): nothing to walk - the rounding of
	// what was followed, not a step; a walk under way is over.
	if (!(len == len) || len < kRoomStepLeastUnits) {
		state.following = false;
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	if (!state.following && len < s.startUnits) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	state.following = true;
	if (s.maxStepUnits > 0.0f && len > s.maxStepUnits) {
		const float k = s.maxStepUnits / len;
		want.x *= k;
		want.y *= k;
	}
	return want;
}

// The neck's pivot under the head (Nadi in the headset, 2026-11-10: turning
// or tilting the head to look left or right should not move the body). The
// headset sits in front of and above the neck, so a head that only turns
// carries the eyes round the neck: 90 degrees to the side moves them 11 cm,
// past RoomScaleStart's 2 units, and the body walked after a look. The room
// follow walks the neck instead: the eyes' offset less the turn of the
// neck-to-eye vector since the recenter (Oculus SDK's neck model,
// OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL 0.0805 m and _VERTICAL 0.075 m). What a
// turn moves the eyes stays a camera offset.
inline constexpr float kNeckToEyeForwardMetres = 0.0805f;
inline constexpr float kNeckToEyeUpMetres = 0.075f;

// The head's offset (the recentered frame: x right, y forward, z up) with
// the head's rotation since the recenter in the same frame: the neck's
// offset. `eye` is the neck-to-eye vector in the head's own axes, in units.
inline NiPoint3 NeckPivotOffset(const NiPoint3& headOffset, const NiMatrix33& headRotation, const NiPoint3& eye) {
	const NiPoint3 turned = headRotation * eye;
	const NiPoint3 pivot = headOffset - turned + eye;
	return pivot.x == pivot.x && pivot.y == pivot.y && pivot.z == pivot.z ? pivot : headOffset;
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

// What the room's step (`room`, world x and y) achieved, in the world, to join
// what the body has followed. Without a walk the whole move is the room's:
// pressed into a wall, the character controller slides the slanted part of the
// step along it, and that slide is where the feet now are - counted whole, the
// eyes stay where the head is and the next step asks only what is still
// ahead. Before 2026-11-08 only the share along the asked step was counted: the
// slide moved the feet, the view rode along with them, and the lead it left
// was asked again, so a body pressed against a wall ran along it at a turn of
// the head (a Quest 3 tester, beta 8; room-follow-wall-slide.txt: the eyes 7
// units aside, the feet 26). With a walk under way the move is shared between
// the two, and the room's part is its share along the whole step asked. At
// most the room step's length, nothing for a NaN.
inline NiPoint3 RoomStepAchieved(float movedX, float movedY, float askedX, float askedY, float roomX, float roomY,
                                 bool walking) {
	const NiPoint3 none{0.0f, 0.0f, 0.0f};
	const float room2 = roomX * roomX + roomY * roomY;
	if (!(room2 > 1e-6f) || !(movedX == movedX) || !(movedY == movedY)) {
		return none;
	}
	if (walking) {
		const float share = RoomStepShare(movedX, movedY, askedX, askedY);
		return NiPoint3{roomX * share, roomY * share, 0.0f};
	}
	const float moved2 = movedX * movedX + movedY * movedY;
	if (moved2 > room2) {
		const float k = math::Sqrt(room2 / moved2);
		return NiPoint3{movedX * k, movedY * k, 0.0f};
	}
	return NiPoint3{movedX, movedY, 0.0f};
}

}  // namespace obvr::vr
