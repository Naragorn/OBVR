#pragma once

// The view held still against the first-person animation while the player
// stands (Nadi in the headset, 2026-11-10: drawing a weapon the view wobbled,
// and the legs with it - nothing should wobble).
//
// Measured 2026-11-06 (draw-leg-flicker.txt, docs/player-body-spec.md, the
// open point "the view itself steps as the weapon is drawn"): the engine's
// first-person eye is Camera01 of the first-person skeleton, and the
// hastened Equip moves it - up to 0.5 units a frame there and back with the
// head and the player still, 0.55 at the draw's start at the game's own
// speed too; drawn and sheathed it stands 0.9 units apart. The shown body's
// hips hang from the camera, so its knees moved with it (half a unit of
// height moves a nearly straight knee 2.6).
//
// So while the player stands, the engine's eye is taken against the player's
// feet in the player's own frame (x right, y forward, z up) and that offset
// is eased toward the engine's over kStandingEyeSeconds: a step of an
// animation is spread over a second instead of a frame, and the change from
// sheathed to drawn arrives as a glide. Walking, it follows within
// kWalkingEyeSeconds, so a walk's start does not trail. An offset further
// than kStandingEyeSnapUnits from the engine's (a load, a teleport, a scale)
// is taken at once. Not live - third person, no headset, on a horse,
// sneaking, swimming, talking, out of Full VR - the engine's own eye.
//
// The real head is the only thing that should move the view while the
// player stands still; the room follow, the sneak's hold and the riding view
// stand on top of this as before.
//
// Pure, covered by standing_eye_hold_test.

#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::camera {

inline constexpr float kStandingEyeSeconds = 1.0f;
inline constexpr float kWalkingEyeSeconds = 0.1f;
inline constexpr float kStandingEyeSnapUnits = 20.0f;

struct StandingEyeHoldState {
	bool valid = false;
	NiPoint3 held{0.0f, 0.0f, 0.0f};  // the eye from the feet, the player's frame
};

struct StandingEyeHoldInput {
	bool live = false;
	bool standing = false;               // the feet asked for no step
	NiPoint3 eye{0.0f, 0.0f, 0.0f};      // the engine's eye, the world
	NiPoint3 feet{0.0f, 0.0f, 0.0f};     // the player's position, the world
	float yaw = 0.0f;                    // the player's rotZ: forward (sin, cos), right (cos, -sin)
	float dt = 0.0f;
};

inline NiPoint3 EyeToPlayerFrame(const NiPoint3& world, float yaw) {
	const float s = math::Sin(yaw);
	const float c = math::Cos(yaw);
	return NiPoint3{world.x * c - world.y * s, world.x * s + world.y * c, world.z};
}

inline NiPoint3 EyeFromPlayerFrame(const NiPoint3& local, float yaw) {
	const float s = math::Sin(yaw);
	const float c = math::Cos(yaw);
	return NiPoint3{local.x * c + local.y * s, -local.x * s + local.y * c, local.z};
}

inline bool EyeHoldFinite(const NiPoint3& p) {
	return p.x == p.x && p.y == p.y && p.z == p.z && p.x < 1.0e7f && p.x > -1.0e7f && p.y < 1.0e7f &&
	       p.y > -1.0e7f && p.z < 1.0e7f && p.z > -1.0e7f;
}

// The eye to place the camera at this frame, in the world.
inline NiPoint3 StepStandingEyeHold(StandingEyeHoldState& s, const StandingEyeHoldInput& in) {
	const NiPoint3 offset = EyeToPlayerFrame(in.eye - in.feet, in.yaw);
	if (!in.live || !EyeHoldFinite(offset) || !(in.yaw == in.yaw)) {
		s = StandingEyeHoldState{};
		return in.eye;
	}
	const NiPoint3 gap = offset - s.held;
	if (!s.valid || gap.LengthSquared() > kStandingEyeSnapUnits * kStandingEyeSnapUnits) {
		s.valid = true;
		s.held = offset;
		return in.eye;
	}
	const float dt = in.dt == in.dt && in.dt > 0.0f && in.dt < 1.0f ? in.dt : 0.0f;
	const float seconds = in.standing ? kStandingEyeSeconds : kWalkingEyeSeconds;
	const float share = 1.0f - math::Exp(-dt / seconds);
	s.held = s.held + gap * share;
	return in.feet + EyeFromPlayerFrame(s.held, in.yaw);
}

}  // namespace obvr::camera
