#pragma once

#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/NiMath.h"

namespace obvr::game {

// The arithmetic of standing the player's own body under the headset.
//
// Oblivion keeps two skeletons for the player: the first-person one, which is
// arms and a weapon hanging off the camera, and the third-person one, the
// whole body, which the engine hides in first person. Enhanced Camera's answer
// (read from its 1.4b source, see docs/vr-modding/ecosystem-and-prior-art.md)
// is to show the whole body anyway and move its root every frame so that the
// head bone sits where the camera is. Enhanced Camera does that with the
// monitor's camera; here the camera is the headset, so the body follows a
// lean and a step in the room.
//
// Enhanced Camera writes the move as a running increment on the root
// ("root += camera - (head + R * offset)"), which only stays put if the
// engine resets the root each frame. Whether it does is not measured, so the
// placement below is absolute instead: the wanted root position is worked out
// from where the head is RELATIVE to the root right now, and written under the
// root's parent. Calling it twice on the same transforms writes the same
// number twice.
//
// Pure, so every flow has a test with made-up transforms.

struct BodyRootInput {
	// The cyclopean camera, in the world: the headset between the eyes.
	NiPoint3 cameraWorld{0.0f, 0.0f, 0.0f};
	// Bip01 Head's world position, as animation left it this frame.
	NiPoint3 headWorld{0.0f, 0.0f, 0.0f};
	// The third-person root's world transform, as animation left it.
	NiPoint3 rootWorld{0.0f, 0.0f, 0.0f};
	NiMatrix33 rootWorldRot = NiMatrix33::Identity();
	float rootWorldScale = 1.0f;
	// Where the eyes are relative to the head bone, in the root's own axes
	// and in game units at scale one: forward (y) and up (z). Enhanced
	// Camera ships 14 forward and 6 up.
	NiPoint3 eyeOffsetUnits{0.0f, 14.0f, 6.0f};
	// The eyes in front of the head bone the way the HEAD faces, not the
	// body: `headRot` is the headset's whole rotation in the world (the
	// camera's - its column 1 forward, column 2 up), pitch and roll included.
	// The head bone is then the neck's pivot: a look down swings the eyes
	// forward and down about it and the body stays put, the way a head nods
	// on its neck. With the yaw alone (the first version, 2026-10-11) the
	// body went forward and down with the headset, and a look down looked
	// into the chest from above instead of down the front of the body (the
	// tester, 2026-10-12: "sehe ich in die körpermitte hinein statt am körper
	// herunter").
	bool eyesFollowHead = false;
	NiMatrix33 headRot = NiMatrix33::Identity();
	// The body stepped back under a bowed head, up to this many units at
	// scale one (LookDownBack, BodyLookDownShift): with eyesFollowHead only.
	float lookDownBackUnits = 0.0f;
	// The root's parent, whose space the root's local transform is written in.
	NiMatrix33 parentRot = NiMatrix33::Identity();
	NiPoint3 parentPos{0.0f, 0.0f, 0.0f};
	float parentScale = 1.0f;
};

// The body steps back under a bowed head (the tester, 2026-10-12, after the
// neck pivot: "nach unten schauen: ich sehe immernoch in den körper rein").
// The head is not drawn - its bone is collapsed, and the neck went with the
// head's mesh - so the body's collar is an open ring around the neck bone,
// and a look down saw down into it: the harness's eye dump at 65 degrees
// down (body-ik-smooth, 2026-10-12) shows the ring's inside at the bottom of
// the picture. A real chin hides that place; nothing here does. So as the
// head bows the body is taken back along the way the face looks, until the
// ring is behind the bottom of the view. The steeper the look, the further
// the bottom of the view reaches back under the eyes, so the further back
// the ring has to be: the share grows with the square of the look down past
// kLookDownStartDegrees, all of it looking straight down. Measured with the
// harness's eye dumps (body-look-wrist, body-look-sweep, 2026-10-12): 8 units
// back hid the ring at 65 degrees down but not at 85, 16 hid it at 85 - and at
// 65 put the whole torso out of sight; the square of the way from 20 to 90
// degrees times 20 gives 8.3 at 65 and 17.2 at 85. The way back is from the
// headset's own up axis: bowed, it leans the way the face looks, and its lean
// is the sine of the look down's cosine - so it is well defined exactly where
// it is needed, looking straight down, where the forward's level part runs out.
inline constexpr float kLookDownStartDegrees = 20.0f;

inline float BodyLookDownShare(const NiMatrix33& headRot) {
	const float sine = -headRot.data[2][1];  // the forward's z, down positive: the sine of the look down
	if (!(sine > -1.5f && sine < 1.5f)) {
		return 0.0f;
	}
	const float s = sine > 1.0f ? 1.0f : sine;
	const float down = math::Atan2(s, math::Sqrt(1.0f - s * s > 0.0f ? 1.0f - s * s : 0.0f)) * math::kRadiansToDegrees;
	if (!(down > kLookDownStartDegrees)) {
		return 0.0f;
	}
	const float t = (down - kLookDownStartDegrees) / (90.0f - kLookDownStartDegrees);
	return t >= 1.0f ? 1.0f : t * t;
}

// The shift taken off the root, in the world: the share of `units` along the
// way the face looks, level (the head's up axis's level part) - the body
// stands that far behind where it would. Nothing when it has no level part.
inline NiPoint3 BodyLookDownShift(const NiMatrix33& headRot, float units) {
	const float share = BodyLookDownShare(headRot);
	if (!(share > 0.0f) || !(units > 0.0f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	const float ux = headRot.data[0][2];
	const float uy = headRot.data[1][2];
	const float level = math::Sqrt(ux * ux + uy * uy);
	if (!(level > 1e-4f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	const float k = units * share / level;
	return NiPoint3{ux * k, uy * k, 0.0f};
}

// Where the root has to stand in the world for the eyes to land on the camera:
// the camera, back by the eye offset carried through the root's rotation and
// scale, back again by the head's offset from the root - and back under a
// bowed head (BodyLookDownShift).
inline NiPoint3 BodyRootWorldWanted(const BodyRootInput& in) {
	const float scale = in.rootWorldScale > 0.0f ? in.rootWorldScale : 1.0f;
	const NiMatrix33& offsetRot = in.eyesFollowHead ? in.headRot : in.rootWorldRot;
	const NiPoint3 eyesFromHead = offsetRot * (in.eyeOffsetUnits * scale);
	const NiPoint3 headFromRoot = in.headWorld - in.rootWorld;
	const NiPoint3 back =
		in.eyesFollowHead ? BodyLookDownShift(in.headRot, in.lookDownBackUnits * scale) : NiPoint3{0.0f, 0.0f, 0.0f};
	return in.cameraWorld - eyesFromHead - headFromRoot - back;
}

// The local position to write into the root for it to stand there: the
// wanted world position taken back under the parent. A parent scale of zero
// or less is refused as one, the same refusal LocalUnderParent makes.
inline NiPoint3 BodyRootLocalPos(const BodyRootInput& in) {
	const NiPoint3 wanted = BodyRootWorldWanted(in);
	const NiMatrix33 inverse = InverseRotation(in.parentRot);
	const float divisor = in.parentScale > 0.0f ? in.parentScale : 1.0f;
	return (inverse * (wanted - in.parentPos)) * (1.0f / divisor);
}

// The camera between the eyes, from the camera node as the render finds it.
// In dual-pass stereo the node stands on the first eye by the time the scene
// is drawn, one half separation off the head; the step it took is known, so
// it is taken back out. With no stereo the step is zero and this is the node.
inline NiPoint3 CyclopeanCamera(const NiPoint3& cameraNodeWorld, const NiPoint3& firstEyeStep) {
	return cameraNodeWorld - firstEyeStep;
}

// Whether the body is shown this frame. First person only: in third person
// the engine draws it itself and moving its root would drag it away from the
// player. Not while a menu is up, where the camera pass has not run and the
// dialogue camera stands somewhere else entirely.
inline bool VisibleBodyWanted(bool enabled, bool isThirdPerson, bool menuIsUp) {
	return enabled && !isThirdPerson && !menuIsUp;
}

}  // namespace obvr::game
