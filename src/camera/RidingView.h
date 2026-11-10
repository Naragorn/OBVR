#pragma once

#include "core/Types.h"
#include "game/GameTypes.h"

namespace obvr::camera {

// On a horse, in first person with the headset: the engine's camera node's
// position replaced by the base of the eyes - held in the horse's frame
// against the stride's bob ([Look] HorseBob) and stood upright over the
// saddle on a slope, always - camera/RidingBase.h. Off the
// horse (or in third person, or without a headset) it leaves the camera as
// it is and forgets the ride. On the riding trace's frames (`traceWrote`,
// cleared here) it logs the base, the saddle and the eyes over it.
void ApplyRidingView(NiAVObject* cameraNode, bool firstPersonWithHeadset, float deltaSeconds, bool& traceWrote,
                     UInt32 traceFrame);

}  // namespace obvr::camera
