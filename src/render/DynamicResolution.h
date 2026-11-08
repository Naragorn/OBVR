#pragma once

// Dynamic resolution for the single pass ([Render] DynamicResolution): the
// frame's world render scale, set once per frame from the compositor's GPU
// time of the frame finished one back (HeadsetRenderer::BeginFrame), read by
// the world camera's port, the eye data for OBNG and the eye copy. The
// decisions are render/DynamicResolutionLogic.h's.

#include "render/DynamicResolutionLogic.h"

#include <cstdint>

namespace obvr::render {

// Settings from the config; call once the config is read (and again after a
// reload). Off puts the scale back to one.
void ConfigureDynamicResolution(bool enabled, float minScale, float targetShare);

// The compositor's GPU time of the last finished frame (preSubmitGpuMs) and
// the headset's period, once per frame before the world render. Logs a line
// when the scale moves (at most a few a second).
void UpdateDynamicResolution(float gpuMs, float periodMs);

// Whether the world is drawn smaller than the eye this frame.
bool DynamicResolutionActive();

// The frame's layout for a frame size: the eye's extents and the camera port.
dynres::Layout DynamicResolutionLayout(std::uint32_t frameWidth, std::uint32_t frameHeight);

// The frame's scale, for the log and the tests of the callers.
float DynamicResolutionScale();

}  // namespace obvr::render
