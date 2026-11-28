#pragma once

// The sky in the mirror's render (docs/mirror-spec.md 16).
//
// The engine draws the sky around the viewer: RenderScene 0x0040C830 copies the
// camera node's local translation (the scene graph's first child, +0x54..+0x5C)
// into the two follower roots - [0x00B333D8] "Sky" (the Sky's root, 0x005411D0
// stores it at Sky+0x04; "Moons Root" and the rest hang under it) and
// [0x00B333DC] "Weather" (the precipitation's parent, 0x0053D8F0) - and updates
// both (0x00707370(0.0f, 0); 0x0040C95F-0x0040C9F0). Its own water reflection
// does the same for its mirrored camera (0x0049BEF0 writes the reflected
// camera's place into the Sky root, updates, renders, puts it back). So the
// sky stands at infinity for whatever camera the camera node carries. OBVR's
// dual pass moves the camera node to each eye (camera/CameraHook.cpp
// BetweenScenePasses), so each eye's sky is centred on that eye.
//
// The mirror's render moves the NiCamera (the node's leaf), not the node
// (render/MirrorGlassPass.cpp RenderFromMirror, spec 12.2 point 2): the sky
// stayed centred on the eye, a camera's distance (twice the eye's distance from
// the glass, 64-95 units in the traces) from each mirror camera. With depth on,
// both mirror cameras saw one dome round one point: the disparity of a thing at
// the dome's radius, not of a thing at infinity - the sky nearer than the far
// land.
//
// The fix at that place: for the mirror's render the camera node's local
// translation is set so that the engine's own copy puts the Sky root on the
// mirror camera - the camera's world place in the Sky root's parent's space -
// and put back after; the roots are put back with the engine's own update.
//
// Pure; covered by mirror_sky_test. game/MirrorSky.cpp reads the nodes, writes
// and puts back.

#include <cmath>

#include "game/NiMath.h"

namespace obvr::game {

enum class MirrorSkyRefusal {
	None = 0,
	NoCameraNode,      // the scene graph has no first child (no world yet)
	NoSkyRoot,         // [0x00B333D8] empty
	ParentNotUsable,   // the Sky root's parent's world transform is no number, or scales to nothing
	CameraNotANumber,  // the mirror camera's place is no number
};

inline const char* MirrorSkyRefusalText(MirrorSkyRefusal r) {
	switch (r) {
	case MirrorSkyRefusal::None:
		return "placed";
	case MirrorSkyRefusal::NoCameraNode:
		return "no camera node";
	case MirrorSkyRefusal::NoSkyRoot:
		return "no sky root";
	case MirrorSkyRefusal::ParentNotUsable:
		return "the sky root's parent has no usable transform";
	case MirrorSkyRefusal::CameraNotANumber:
		return "the mirror's camera is not a number";
	}
	return "?";
}

struct MirrorSkyInputs {
	bool cameraNode = false;  // the scene graph's first child, whose local translation RenderScene copies
	bool skyRoot = false;     // [0x00B333D8]
	bool skyParent = false;   // the Sky root hangs under a node (without one, its local is its world)
	NiTransform skyParentWorld{};
	NiPoint3 camera{0.0f, 0.0f, 0.0f};  // the mirror camera's world place for this render
};

struct MirrorSkyPlan {
	MirrorSkyRefusal refusal = MirrorSkyRefusal::NoCameraNode;
	// What the camera node's local translation holds for the render: the
	// engine copies it into the roots' local translation.
	NiPoint3 nodeLocal{0.0f, 0.0f, 0.0f};
};

inline bool MirrorSkyFinite(const NiPoint3& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

// Below this a parent's scale or the determinant of its rotation counts as
// collapsed: no local translation would carry the root to the camera.
inline constexpr float kMirrorSkyMinScale = 1.0e-5f;

// The node's local translation for the mirror's render: the mirror camera's
// world place in the Sky root's parent's space (the inverse of the engine's
// update, world = parent.pos + parent.rot * (local * parent.scale)), so that
// the engine's copy of it into the root lands the root on the camera.
inline MirrorSkyPlan PlanMirrorSky(const MirrorSkyInputs& in) {
	MirrorSkyPlan p;
	if (!in.cameraNode) {
		p.refusal = MirrorSkyRefusal::NoCameraNode;
		return p;
	}
	if (!in.skyRoot) {
		p.refusal = MirrorSkyRefusal::NoSkyRoot;
		return p;
	}
	NiPoint3 local = in.camera;
	if (in.skyParent) {
		const NiTransform& w = in.skyParentWorld;
		bool finite = std::isfinite(w.scale) && MirrorSkyFinite(w.pos);
		for (int row = 0; row < 3; ++row) {
			for (int col = 0; col < 3; ++col) {
				finite = finite && std::isfinite(w.rot.data[row][col]);
			}
		}
		const float det = finite ? Determinant(w.rot) : 0.0f;
		if (!finite || !(std::fabs(w.scale) > kMirrorSkyMinScale) || !(std::fabs(det) > kMirrorSkyMinScale)) {
			p.refusal = MirrorSkyRefusal::ParentNotUsable;
			return p;
		}
		local = InverseMatrix(w.rot) * (in.camera - w.pos) * (1.0f / w.scale);
	}
	if (!MirrorSkyFinite(local)) {
		p.refusal = MirrorSkyRefusal::CameraNotANumber;
		return p;
	}
	p.refusal = MirrorSkyRefusal::None;
	p.nodeLocal = local;
	return p;
}

}  // namespace obvr::game
