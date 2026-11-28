#pragma once

// The top and bottom rows' anchors for one frame (vr/ViewAnchor.h): the
// camera hook's part - the step, the anchor handed to the frame, and the log
// line that proves in a headset run that a row glided after a nod.

#include "core/Types.h"
#include "vr/HandHud.h"

namespace obvr::camera {

// Steps row `row` (0 the top, 1 the bottom) of the view and writes its
// anchor into `frame`. Logs the first few times a row starts to glide, with
// the angles that started it, and never more than a handful a session.
void StepViewRow(vr::HudViewAnchor& anchor, UInt32 row, bool shown, const vr::HandHudSettings& settings,
                 vr::HandHudFrame& frame, float dt);

}  // namespace obvr::camera
