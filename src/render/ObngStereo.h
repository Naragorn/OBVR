#pragma once
// OBNG.dll's single-pass stereo interface, bound at run time
// (render/ObngStereoLogic.h decides the path). OBNG's
// plugin/src/StereoEyesApi.h is the contract: three plain C functions found
// with GetProcAddress, the eye data a versioned struct copied here so OBVR
// needs no OBNG header. Render thread only.

#include <cstdint>

#include "render/ObngStereoLogic.h"

namespace obvr::render {

// StereoEyesApi.h's ObngStereoViewport and ObngStereoEyes, layout for layout
// (version 1): D3DVIEWPORT9's order, the offset in world units from eye 0
// (the pass camera) to eye 1.
struct ObngViewport {
	std::uint32_t x = 0;
	std::uint32_t y = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	float minZ = 0.0f;
	float maxZ = 1.0f;
};

struct ObngEyes {
	std::uint32_t size = sizeof(ObngEyes);
	std::uint32_t version = 1;
	float secondEyeOffset[3] = {0.0f, 0.0f, 0.0f};
	ObngViewport viewport[2];
};

static_assert(sizeof(ObngViewport) == 24, "StereoEyesApi.h's ObngStereoViewport");
static_assert(sizeof(ObngEyes) == 68, "StereoEyesApi.h's ObngStereoEyes");

// The eye data for a side-by-side frame: eye 0 the left half (what the half
// port leaves as the engine's own viewport, step 3), eye 1 the right half,
// both the full depth range; the offset from the pass camera to eye 1 in
// world units (OBVR's g_dualShift).
inline ObngEyes EyesFor(std::uint32_t frameWidth, std::uint32_t frameHeight, const float offset[3]) {
	ObngEyes eyes;
	eyes.secondEyeOffset[0] = offset[0];
	eyes.secondEyeOffset[1] = offset[1];
	eyes.secondEyeOffset[2] = offset[2];
	const std::uint32_t half = WorldPictureWidth(frameWidth, true);
	eyes.viewport[0] = ObngViewport{0, 0, half, frameHeight, 0.0f, 1.0f};
	eyes.viewport[1] = ObngViewport{half, 0, half, frameHeight, 0.0f, 1.0f};
	return eyes;
}

// The same with the world drawn into `width` x `height` of each half (dynamic
// resolution, render/DynamicResolutionLogic.h): eye 0 at the frame's top-left,
// eye 1 at the right half's top-left, both the layout's extents.
inline ObngEyes EyesFor(std::uint32_t frameWidth, std::uint32_t frameHeight, const float offset[3], std::uint32_t width,
                        std::uint32_t height) {
	ObngEyes eyes = EyesFor(frameWidth, frameHeight, offset);
	const std::uint32_t half = WorldPictureWidth(frameWidth, true);
	eyes.viewport[0] = ObngViewport{0, 0, width, height, 0.0f, 1.0f};
	eyes.viewport[1] = ObngViewport{half, 0, width, height, 0.0f, 1.0f};
	return eyes;
}

// Dynamic resolution ([Render] DynamicResolution, render/DynamicResolution.cpp):
// the eye data for this frame - advances the frame's world render scale from
// the compositor's GPU time first (once per world render, here, before the
// render), then EyesFor with the layout's extents - and the world camera's
// port for the same frame (the left half, or the part of it the scale draws
// into). Both read one layout; with the setting off they are EyesFor and
// WorldPortFor(true).
ObngEyes DynamicResolutionEyes(std::uint32_t frameWidth, std::uint32_t frameHeight, const float offset[3]);
CameraPort DynamicResolutionWorldPort();

// Looks for OBNG.dll and its three functions, once it is loaded (tried again
// on each call until it is, then never again); logs what it found once.
// True when all three are bound.
bool BindObngStereo();

// ObngStereoPlan(), or 0 when unbound.
std::uint32_t ObngPlan();

// ObngStereoSetEyes(eyes); kObngNotBound when unbound.
inline constexpr std::uint32_t kObngNotBound = 0xFFFFFFFFu;
std::uint32_t ObngSetEyes(const ObngEyes& eyes);

// ObngStereoClearEyes(), nothing when unbound.
void ObngClearEyes();

}  // namespace obvr::render
