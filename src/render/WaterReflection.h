#pragma once

#include "core/Types.h"

namespace obvr::render {

struct WaterReflectionConstant {
	UInt32 registerIndex = 8;
	float value[4]{};
};

// Classifies SetRenderTarget calls seen while Oblivion renders the water
// reflection. The reflection map is an off-screen colour target: target 0,
// readable, non-empty, and neither the main back buffer nor OBVR's HUD
// substitute. Kept pure so every refusal path can be exercised without a
// Direct3D device.
inline bool IsWaterReflectionTargetCandidate(UInt32 index, bool surfacePresent,
                                             bool isBackBuffer,
                                             bool isHudSubstitute,
                                             bool descriptionReadable,
                                             UInt32 width, UInt32 height) {
	return index == 0 && surfacePresent && !isBackBuffer && !isHudSubstitute &&
	       descriptionReadable && width != 0 && height != 0;
}

// The stock water pixel shaders use c8.y only as the mix amount between the
// projective ReflectionMap and ReflectionColor. Replacing that one component
// leaves normal/detail scrolling, depth, Fresnel, fog and sun lighting intact.
inline bool BuildStableWaterReflectionConstant(bool enabled, bool waterShader,
                                                UInt32 startRegister,
                                                const float* data,
                                                UInt32 vector4fCount,
                                                WaterReflectionConstant& out) {
	constexpr UInt32 kReflectionAmountsRegister = 8;
	if (!enabled || !waterShader || data == nullptr || vector4fCount == 0 ||
	    startRegister > kReflectionAmountsRegister ||
	    vector4fCount <= kReflectionAmountsRegister - startRegister) {
		return false;
	}
	const float* source = data + (kReflectionAmountsRegister - startRegister) * 4;
	out.registerIndex = kReflectionAmountsRegister;
	for (unsigned i = 0; i < 4; ++i) out.value[i] = source[i];
	out.value[1] = 0.0f;
	return true;
}

}  // namespace obvr::render