#pragma once

// What OBVR writes onto the bones of the living each world frame, after
// their animation and before the render (camera/CameraHook.cpp,
// BeforeFirstScenePass): a holding walker's hand laid on the player's
// forearm (game/WalkerGrip.h), and the head and the arms giving way to the
// hands and the blade ([Hands] BodiesGive, game/BodyGive.h - a prototype,
// off).

#include "game/HandBodyLogic.h"
#include "game/HandContactLogic.h"
#include "game/NiMath.h"
#include "vr/HandMode.h"

namespace obvr::camera::hands {

// What the step reads of the rest of the frame. Index 0 is the weapon hand,
// 1 the other, as the hands' contact has them.
struct LivingLinks {
	const vr::HandModeResult& hand;
	const NiTransform& camera;  // the cyclopean camera in the world
	bool cameraValid;
	bool inWorld;               // no menu is up
	float dtSeconds;
	NiPoint3 handWanted[2];     // where the controller put each hand this frame, not where contact holds it
	game::HandShape handShape[2];  // what each hand meets the world as
	game::BodySpan handSpan[2];    // each hand's span in its own frame
	game::BodySpan bladeSpan;      // the drawn weapon's
};

void StepLivingBones(const LivingLinks& links);

}  // namespace obvr::camera::hands
