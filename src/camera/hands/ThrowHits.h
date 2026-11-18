#pragma once

// Thrown things that hit people (game/ThrowLogic.h): an object let go with a
// throw's speed, or a weapon or torch thrown from the hand, is followed;
// passing through someone's body fast enough it staggers them or knocks them
// down - the shove's effects, once per throw. Gear that hits comes back to
// the hand that threw it (game/BounceBackLogic.h; [Hands] BounceBack, on by
// default): sent back in an arc at the hit, steered each frame until it is
// near the hand, caught or its time is up. Harness: throw-hit.txt,
// throw-hit-weapon.txt, throw-bounce-back.txt.

#include "camera/hands/Hints.h"
#include "core/Config.h"
#include "core/Types.h"
#include "game/NiMath.h"
#include "game/ShoveLogic.h"
#include "vr/HandMode.h"

namespace obvr::camera::hands {

// Where the hands are this frame, in the world.
struct BounceHands {
	const NiTransform& camera;  // the cyclopean camera's world transform
	bool cameraValid;
	const vr::HandModeResult& hand;
};

// Each frame: gear on its way back steered, the thing just thrown followed,
// its hit on someone. `hints` counts the throws and the bounces for the
// hints, `cooldown` is the shove's.
void StepThrowHits(const Config& config, const BounceHands& hands, HintsState& hints, game::ShoveCooldown& cooldown,
                   float dt, bool inWorld);

}  // namespace obvr::camera::hands
