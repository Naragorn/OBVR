#pragma once

#include "core/MathFns.h"
#include "core/Rotation.h"
#include "core/Smoothing.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::camera {

// The race and sex menu's view: the eyes in front of the character's face,
// looking at it - as the game's own camera does for that menu.
//
// Read in Oblivion.exe (Ghidra, 2026-10-23). The menu's opening (0x005CA010,
// the call at 0x005CC17A) keeps the player's point of view (+0x588) in the
// menu at +0x894 and switches to third person through ToggleCamera(0); the
// race selection's completion (0x005C28F0, the call at 0x005C2B36) switches
// back to first person when the menu opened in it, ToggleCamera(saved == 0).
// The camera itself is 0x005C2BF0, called at the opening with 0 degrees and
// from the menu's click handler (0x005C3BE0) with the drag's angle - the
// cursor's x less the press's, over 5, in degrees: the face is the world
// bound's centre of the player's FaceGen node (the reference's virtual at
// 0x134, xOBSE's GetFacegenNiNodeSkinned, called with 0), the yaw the
// player's GetZRotation (the virtual at 0x1E0); the camera stands 30 units
// from the face along the yaw turned by the drag's angle and looks at a
// point 11 units beside the face, so the face sits at the right of the
// picture with the menu's rows at the left. It is written straight into the
// camera node, at the opening and on each drag - not every frame: the
// player's own camera update, which OBVR hooks, does not run behind the
// menu.
//
// OBVR's menu frames place the camera themselves from the last world
// frame's base (PlaceMenuCamera) - which with the engine switched to third
// person for the menu is its chase camera behind the body: the character
// seen from behind (Nadi, 2026-10-23: "obvr zeigt einfach unsern char von
// hinten"). So while the race menu is the top menu the base is this
// instead: the same face, the same facing, the engine's angle to the side
// kept as a proportion (11 over 30, 20 degrees), the distance a setting in
// metres - 30 units (0.43 m) is a monitor's close-up, not a comfortable VR
// one. The head's own rotation and offset go on top as on any menu frame.
// The drag's angle is not followed (open). Pure, over plain values; the
// camera hook reads the menu, the face and the facing.

inline constexpr float kEngineChargenDistanceUnits = 30.0f;
inline constexpr float kEngineChargenAimSideUnits = 11.0f;

struct ChargenViewInput {
	bool wanted = false;         // the setting
	bool raceMenuUp = false;     // the race and sex menu is the top visible menu
	bool faceKnown = false;      // the face's centre was read
	NiPoint3 face{0.0f, 0.0f, 0.0f};
	bool facingKnown = false;    // the player's facing was read
	Heading facing{};            // the last world frame's levelled heading (core/Rotation.h)
	float distanceUnits = 0.0f;  // how far in front of the face the eyes stand
};

struct ChargenViewVerdict {
	bool active = false;
	NiPoint3 pos{0.0f, 0.0f, 0.0f};           // the eyes' base
	NiMatrix33 rot = NiMatrix33::Identity();  // the base rotation: level, facing the character
};

inline bool ChargenNumber(float v) { return v == v && v < 1.0e9f && v > -1.0e9f; }

inline ChargenViewVerdict ChargenView(const ChargenViewInput& in) {
	ChargenViewVerdict v;
	if (!in.wanted || !in.raceMenuUp || !in.faceKnown || !in.facingKnown) {
		return v;
	}
	if (!ChargenNumber(in.face.x) || !ChargenNumber(in.face.y) || !ChargenNumber(in.face.z)) {
		return v;
	}
	if (!ChargenNumber(in.distanceUnits) || !(in.distanceUnits > 1.0f)) {
		return v;
	}
	if (!ChargenNumber(in.facing.cosine) || !ChargenNumber(in.facing.sine)) {
		return v;
	}
	const float length = math::Sqrt(in.facing.cosine * in.facing.cosine + in.facing.sine * in.facing.sine);
	if (!(length > 0.5f) || !ChargenNumber(length)) {
		return v;
	}
	const Heading facing{in.facing.cosine / length, in.facing.sine / length};
	// Where the player faces and their right, level (core/Rotation.h: a
	// heading's forward is column 1, its right column 0).
	const NiPoint3 forward{-facing.sine, facing.cosine, 0.0f};
	const NiPoint3 right{facing.cosine, facing.sine, 0.0f};
	v.pos = in.face + forward * in.distanceUnits;
	// The view aimed beside the face as the engine's is - 11 units to the
	// player's right for its 30 - so the face sits at the right of the
	// picture: the player's right is the viewer's left.
	const float side = in.distanceUnits * (kEngineChargenAimSideUnits / kEngineChargenDistanceUnits);
	const NiPoint3 aim = in.face + right * side;
	NiPoint3 look = aim - v.pos;
	look.z = 0.0f;
	const float lookLength = math::Sqrt(look.x * look.x + look.y * look.y);
	if (!(lookLength > 0.001f)) {
		return v;
	}
	look = look * (1.0f / lookLength);
	// A heading whose forward is the look: forward = (-sine, cosine).
	v.rot = RotationFromHeading(Heading{look.y, -look.x});
	v.active = true;
	return v;
}

}  // namespace obvr::camera
