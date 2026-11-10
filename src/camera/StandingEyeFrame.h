#pragma once

// The camera hook's part of the view held still against the first-person
// animation while the player stands (camera/StandingEyeHold.h).

#include "game/GameTypes.h"

namespace obvr::camera {

// Reads the player, decides whether the hold is live (`base`: first person
// with a headset in Full VR; not on a horse, sneaking, swimming or in a menu)
// and writes the held eye into the camera node's local position. At a hand
// script's mark the engine's eye and the held one are logged.
void ApplyStandingEyeHold(NiAVObject* cameraNode, bool base, float dtSeconds);

}  // namespace obvr::camera
