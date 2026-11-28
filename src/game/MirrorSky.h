#pragma once

// The sky on the mirror's camera for the mirror's render alone
// (game/MirrorSkyLogic.h for why; docs/mirror-spec.md 16). Called around the
// mirror's world render in render/MirrorGlassPass.cpp RenderFromMirror, after
// both eyes' captures: nothing here runs while the mirror does not render.

#include "game/MirrorSkyLogic.h"

namespace obvr {
struct NiAVObject;
}

namespace obvr::game {

// What was changed, for the putting back.
struct MirrorSkySwitch {
	bool placed = false;
	MirrorSkyPlan plan;
	NiAVObject* node = nullptr;          // the camera node (the scene graph's first child)
	NiAVObject* roots[2] = {nullptr, nullptr};  // [0x00B333D8] Sky, [0x00B333DC] Weather
	NiPoint3 camera{0.0f, 0.0f, 0.0f};   // the mirror camera's world place
	NiPoint3 nodeLocal{0.0f, 0.0f, 0.0f};  // saved
	NiPoint3 nodeWorld{0.0f, 0.0f, 0.0f};  // saved, to see whether the render moved the node
	NiPoint3 rootLocal[2] = {};            // saved
	NiPoint3 oldCentre{0.0f, 0.0f, 0.0f};  // where the engine's copy would have put the Sky root: the node's own place
};

// Before the mirror's render: the camera node's local translation set so that
// RenderScene's own copy puts the Sky and Weather roots on `camera` - or
// nothing, with the refusal (logged once each time it changes).
void PlaceSkyForMirror(MirrorSkySwitch& s, const NiPoint3& camera);

// Right after it, before the camera's own transform is put back: the node's
// local translation and the roots' put back, the roots updated as RenderScene
// updates them. The first passes of a session log where the roots stood in
// the render. Nothing for a refused switch.
void PutSkyBackAfterMirror(MirrorSkySwitch& s);

}  // namespace obvr::game
