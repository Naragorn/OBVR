#pragma once

#include "core/Types.h"

namespace obvr::render {

// Takes the whole device state and puts it back later.
//
// For the dual pass, whose entire premise is that the second world render is
// the first one again from a different camera. That only holds if the device
// is in the same state both times, and it is not: a render leaves behind
// whatever its last object needed, and Gamebryo keeps its own record of what
// the device is set to and skips the calls it believes are redundant. So the
// second render never puts back what it did not see change.
//
// The failure that named this is documented in the engine's own neighbourhood:
// Oblivion Reloaded renders the first person node twice, and DXVK's issue 2420
// traces the botched second copy to a vertex declaration left holding position
// only - "the game never restores the original vertex declaration after that".
// A declaration without bone indices and weights skins nothing, so every
// vertex of an actor lands on the same point. That is a fault only the render
// downstream of it can have, which under a dual pass means one eye.
//
// Capture returns null when the state could not be taken, and Restore must be
// called exactly once for every non-null Capture: it applies the state and
// releases the block. Blocks are not kept between calls on purpose - a state
// block has to be released before the device can be reset, and Oblivion resets
// its device whenever the video menu changes a setting.
void* CaptureDeviceState(void* device);
void RestoreDeviceState(void* state);

}  // namespace obvr::render
