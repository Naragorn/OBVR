#pragma once

// The weapon places shown faintly while the player looks down
// (vr/HolsterGlance.h; [Hands] HolsterGlance): the places' window's four
// circles (HolsterState::placeLayers) at the places the settings hold, faded
// in and out by the head's pitch. Not while the window itself is up - it
// owns the circles then. Harness: holster-glance.txt.

#include "camera/hands/Holster.h"

namespace obvr::camera::hands {

// One frame of it. `allowed`: Full VR in the world, no menu, OBVR's settings
// closed.
void StepHolsterGlance(HolsterState& state, const Config& config, vr::OpenVRBackend& backend,
                       const vr::HandModeFrame& frame, bool allowed);

// The window takes the circles: the glance's opacity given back (full).
void EndHolsterGlance(HolsterState& state, vr::OpenVRBackend& backend);

}  // namespace obvr::camera::hands
