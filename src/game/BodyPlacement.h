#pragma once

#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/BodyIkLogic.h"
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
	// scale one (BodyLookDownShift; kLookDownBackUnits in play): with
	// eyesFollowHead only.
	float lookDownBackUnits = 0.0f;
	// The root's parent, whose space the root's local transform is written in.
	NiMatrix33 parentRot = NiMatrix33::Identity();
	NiPoint3 parentPos{0.0f, 0.0f, 0.0f};
	float parentScale = 1.0f;
};

// The body steps back under a bowed head (the tester, 2026-10-12, after the
// neck pivot: "nach unten schauen: ich sehe immernoch in den körper rein").
// What was seen was taken for the open collar ring at first (the head is
// collapsed for the draw, so the neck ends in a ring); measured the same
// evening (BodyView, the mesh read with pyffi) it is the camera's near plane:
// the eyes bowed 85 degrees stand on the chest's own surface, and the chest's
// wall nearer than the near plane is cut from the picture - a hole into the
// body, with whatever is two-sided inside (a shirt) or the floor showing
// through it. The ring itself is behind and above the eyes at every bow and
// never in view. A real chin and a real neck would keep the eyes off the
// chest; nothing here does. So as the head bows the body is taken back along
// the way the face looks, until the cut stretch of the wall lies below the
// bottom of the view. The steeper the look, the further the bottom of the
// view reaches back under the eyes, so the further back the body has to be:
// the share grows with the square of the look down past
// kLookDownStartDegrees, all of it looking straight down. Measured with the
// harness's eye dumps (body-look-wrist and a sweep, 2026-10-12, the game's
// near plane of 10): 8 units back left the hole at 85 degrees, 16 closed it -
// and at 65 put the whole torso out of sight; the square of the way from 20
// to 90 degrees times 20 gives 8.3 at 65 and 17.2 at 85. How many units at
// most follows the near plane (LookDownBackUnitsFor): a nearer plane cuts
// less wall, so the body need not go as far, and the chest comes into view.
// Always on, no setting (the tester, 2026-10-12: "kann als einstellung weg.
// muss nämlich immer an sein"). The way back is from the headset's own up
// axis: bowed, it leans the way the face looks, and its lean is the sine of
// the look down's cosine - so it is well defined exactly where it is needed,
// looking straight down, where the forward's level part runs out.
inline constexpr float kLookDownStartDegrees = 20.0f;
inline constexpr float kLookDownBackUnits = 20.0f;  // looking straight down, at scale one, at the game's near plane

// How far back, looking straight down, for a camera near plane of
// `nearUnits`: twice the near plane - 20 at the game's own 10, 8 at
// [Body] NearPlane's default 4. What the step back is for is the near
// plane's cut: the chest's wall closer to the eyes than the near plane is
// not drawn, and the body is taken back until no cut wall is in view. The
// eyes bowed 85 degrees sit 6.5 units in front of the head bone and 5 below
// it, on the chest's own surface (upperbody.nif read with pyffi, 2026-10-12:
// the chest's front 6.6 units before the head bone 7 below it, 10.5 at the
// pectorals 17 below), so the wall within the near plane runs down from
// there and the body has to go back until that stretch lies below the view's
// bottom edge (the headset's 44 degrees under the look): about 11 units with
// the game's near plane, about 6 with one of 4 - twice the near plane holds
// a clothed body's extra inch in reserve, and at 10 it is the 20 measured
// with the eye dumps. A near plane that is no usable number keeps the 20.
inline constexpr float kLookDownBackPerNearUnit = 2.0f;

inline float LookDownBackUnitsFor(float nearUnits) {
	if (!(nearUnits > 0.0f) || !(nearUnits < 1000.0f)) {
		return kLookDownBackUnits;
	}
	return kLookDownBackPerNearUnit * nearUnits;
}

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

// ---------------------------------------------------------- The skeleton read
//
// Whether the skeleton reads like a body at all, before anything is written
// to it. A tester's log (2026-10-16, a Quest 3 over Virtual Desktop, 0.3.0)
// had the third-person skeleton's bones tens of thousands of units apart
// from the first frame after a load - "the body's upper arm 47455.8 and
// forearm 32058.1 units", "the body let down 55424.2 units" - and the body
// placed and solved on those numbers put its arms across the whole view and
// left the hands at not-a-number: blue shapes over the world, gone in every
// menu, where the hands go. What is wrong with that skeleton sits in that
// game (its skeleton, or the engine's transforms of it) and is not known
// yet; a body that reads like that is left hidden, nothing is written to
// it, and the log says what was read ("Body check:").
//
// What a body reads at scale one (BodyIkLogic.h, the skeleton measured):
// the upper arm 21.85 units, the forearm 14.78, the head 112 above Bip01
// standing, 60 to 80 crouched or sat. The bounds leave room for a race's
// scale (read off the root and applied) and for any pose, not for a
// skeleton a thousand times out. Arms not read (the IK off) are not judged.
struct SkeletonReading {
	bool finite = true;                        // every number read was one
	float rootScale = 1.0f;                    // Bip01's world scale
	float parentScale = 1.0f;                  // its parent's, the player's node
	NiPoint3 parentColumns{1.0f, 1.0f, 1.0f};  // the lengths of the parent's rotation columns
	float headUnits = 0.0f;                    // Bip01 Head from Bip01, world units
	float upperUnits[2] = {0.0f, 0.0f};        // the upper arms, right and left; 0 when not read
	float foreUnits[2] = {0.0f, 0.0f};         // the forearms
};

enum class SkeletonVerdict : UInt8 { Sane, NotFinite, ScaleOff, ParentSkewed, HeadOff, ArmOff };

inline constexpr float kSkeletonScaleMin = 0.05f;
inline constexpr float kSkeletonScaleMax = 20.0f;
inline constexpr float kSkeletonColumnMin = 0.8f;  // a rotation's columns are unit; a scaled one is no rotation
inline constexpr float kSkeletonColumnMax = 1.25f;
inline constexpr float kSkeletonHeadMinUnits = 30.0f;
inline constexpr float kSkeletonHeadMaxUnits = 300.0f;
inline constexpr float kSkeletonUpperMinUnits = 5.0f;
inline constexpr float kSkeletonUpperMaxUnits = 80.0f;
inline constexpr float kSkeletonForeMinUnits = 4.0f;
inline constexpr float kSkeletonForeMaxUnits = 60.0f;

inline bool WithinScaled(float value, float minAtOne, float maxAtOne, float scale) {
	return value >= minAtOne * scale && value <= maxAtOne * scale;
}

inline SkeletonVerdict JudgeSkeleton(const SkeletonReading& r) {
	if (!r.finite || !ik::Finite(r.rootScale) || !ik::Finite(r.parentScale) || !ik::Finite(r.parentColumns) ||
	    !ik::Finite(r.headUnits) || !ik::Finite(r.upperUnits[0]) || !ik::Finite(r.upperUnits[1]) || !ik::Finite(r.foreUnits[0]) ||
	    !ik::Finite(r.foreUnits[1])) {
		return SkeletonVerdict::NotFinite;
	}
	if (r.rootScale < kSkeletonScaleMin || r.rootScale > kSkeletonScaleMax || r.parentScale < kSkeletonScaleMin ||
	    r.parentScale > kSkeletonScaleMax) {
		return SkeletonVerdict::ScaleOff;
	}
	const NiPoint3& c = r.parentColumns;
	if (c.x < kSkeletonColumnMin || c.x > kSkeletonColumnMax || c.y < kSkeletonColumnMin || c.y > kSkeletonColumnMax ||
	    c.z < kSkeletonColumnMin || c.z > kSkeletonColumnMax) {
		return SkeletonVerdict::ParentSkewed;
	}
	const float s = r.rootScale;
	if (!WithinScaled(r.headUnits, kSkeletonHeadMinUnits, kSkeletonHeadMaxUnits, s)) {
		return SkeletonVerdict::HeadOff;
	}
	for (UInt32 side = 0; side < 2; ++side) {
		if (r.upperUnits[side] > 0.0f &&
		    !WithinScaled(r.upperUnits[side], kSkeletonUpperMinUnits, kSkeletonUpperMaxUnits, s)) {
			return SkeletonVerdict::ArmOff;
		}
		if (r.foreUnits[side] > 0.0f && !WithinScaled(r.foreUnits[side], kSkeletonForeMinUnits, kSkeletonForeMaxUnits, s)) {
			return SkeletonVerdict::ArmOff;
		}
	}
	return SkeletonVerdict::Sane;
}

// The verdict's words for the log.
inline const char* SkeletonVerdictWords(SkeletonVerdict v) {
	switch (v) {
		case SkeletonVerdict::Sane:
			return "sane";
		case SkeletonVerdict::NotFinite:
			return "not every number read is one";
		case SkeletonVerdict::ScaleOff:
			return "a scale off any body's";
		case SkeletonVerdict::ParentSkewed:
			return "the parent's rotation is no rotation";
		case SkeletonVerdict::HeadOff:
			return "the head off any body's";
		case SkeletonVerdict::ArmOff:
			return "an arm off any body's";
	}
	return "?";
}

}  // namespace obvr::game
