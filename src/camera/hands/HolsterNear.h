#pragma once

// The weapon places shown faintly while the drawn weapon is held near them
// (vr/PlaceNear.h; [Hands] HolsterNear, [Hands] PlaceHintOpacity): the places'
// window's circles, each on its own - only a place that takes what is held,
// and only while the hand that holds it is close. Stepped by
// UpdateHolsterPlacing while the window itself is not up; the window owns the
// circles then.

#include "camera/hands/Holster.h"

namespace obvr::camera::hands {

// `allowed`: Full VR in the world, no menu, the settings not open.
void StepHolsterNear(HolsterState& state, const Config& config, vr::OpenVRBackend& backend,
                     const vr::HandModeFrame& frame, bool allowed);

// The window takes the circles: their opacity given back (full).
void EndHolsterNear(HolsterState& state, vr::OpenVRBackend& backend);

}  // namespace obvr::camera::hands
