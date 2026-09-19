#pragma once

#include <cmath>

#include "core/Types.h"
#include "core/Rotation.h"
#include "game/NiMath.h"
#include "render/WaterReprojection.h"

namespace obvr::render {

inline bool UsesStableWaterReflectionShader(WaterReflectionMode mode, bool diagnostic) {
    return UsesWaterReprojectionShader(mode) && !diagnostic;
}

inline float WaterReflectionEdgeWeight(float u, float v, float fadeWidth) {
	if (!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(fadeWidth) ||
	    !(fadeWidth > 0.0f)) return 0.0f;
	float edge = u;
	if (v < edge) edge = v;
	if (1.0f - u < edge) edge = 1.0f - u;
	if (1.0f - v < edge) edge = 1.0f - v;
	if (!(edge > 0.0f)) return 0.0f;
	const float weight = edge / fadeWidth;
	return weight < 1.0f ? weight : 1.0f;
}

inline void BuildWaterReflectionCaptureTransform(const NiTransform& center,
	                                              float yawDegrees,
	                                              NiTransform& output) {
	output = center;
	output.rot = EulerToMatrix(0.0f, 0.0f, yawDegrees) * center.rot;
}
void PrepareWaterReflectionBlend(void* device);
bool WaterReflectionBlendReady();
void* SelectWaterReflectionPixelShader(void* device, void* requested,
	                                    bool waterShader, WaterReflectionMode mode);
void SetWaterReflectionBlendMatrices(const WaterMatrix& left,
	                                  const WaterMatrix& center,
	                                  const WaterMatrix& right, bool valid);
bool StoreWaterReflectionTexture(void* device, void* sourceSurface,
	                              UInt32 width, UInt32 height, UInt32 format,
	                              unsigned slot);
bool WaterReflectionTexturesReady();
void* GetWaterReflectionStoredSurface(unsigned slot);
bool BeginWaterReflectionBlendDraw(void* device);
void EndWaterReflectionBlendDraw(void* device);
UInt32 WaterReflectionBlendDrawSerial();
UInt32 WaterReflectionDiagnosticDrawSerial();

}  // namespace obvr::render
