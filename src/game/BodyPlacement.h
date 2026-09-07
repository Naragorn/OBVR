#pragma once

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
	// The root's parent, whose space the root's local transform is written in.
	NiMatrix33 parentRot = NiMatrix33::Identity();
	NiPoint3 parentPos{0.0f, 0.0f, 0.0f};
	float parentScale = 1.0f;
};

// Where the root has to stand in the world for the eyes to land on the camera:
// the camera, back by the eye offset carried through the root's rotation and
// scale, back again by the head's offset from the root.
inline NiPoint3 BodyRootWorldWanted(const BodyRootInput& in) {
	const float scale = in.rootWorldScale > 0.0f ? in.rootWorldScale : 1.0f;
	const NiPoint3 eyesFromHead = in.rootWorldRot * (in.eyeOffsetUnits * scale);
	const NiPoint3 headFromRoot = in.headWorld - in.rootWorld;
	return in.cameraWorld - eyesFromHead - headFromRoot;
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
