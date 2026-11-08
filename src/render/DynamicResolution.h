#pragma once

// Dynamic resolution for the single pass ([Render] DynamicResolution): the
// frame's world render scale, advanced once per world render from the
// compositor's GPU time of the frame finished one back, read by the world
// camera's port, the eye data for OBNG (both declared in render/ObngStereo.h)
// and the eye copy's crop (render/SceneRenderHook.h). The decisions are
// render/DynamicResolutionLogic.h's; the state has one owner here.

#include "render/DynamicResolutionLogic.h"

#include <cstdint>

namespace obvr::render {

// Whether the world is drawn smaller than the eye this frame.
bool DynamicResolutionActive();

// The frame's layout for a frame size: the eye's extents and the camera port.
dynres::Layout DynamicResolutionLayout(std::uint32_t frameWidth, std::uint32_t frameHeight);

// The frame's scale, for the log.
float DynamicResolutionScale();

}  // namespace obvr::render
