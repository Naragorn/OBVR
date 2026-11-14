#pragma once

// The thrown thing's noise for Walkers (game/ThrowNoiseLogic.h): its first
// hard stop, else where it comes to rest - once, as 'WNOI' ([Hands]
// ThrowNoise). Harness: throw-noise-object.txt, throw-noise-weapon.txt.

#include "core/Config.h"
#include "core/Types.h"

namespace obvr::camera::hands {

// A thing just thrown (0 for none) is followed for its noise.
void StartThrowNoiseFor(UInt32 thrown, const Config& config);

// The followed thing's flight read; its noise sent once it lands.
void StepThrowNoiseFor(const Config& config, float dt, bool inWorld);

}  // namespace obvr::camera::hands
