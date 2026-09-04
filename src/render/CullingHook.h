#pragma once

#include "core/Types.h"

namespace obvr::render {

// Detours NiCullingProcess::Process. During a dual frame, the first pass's
// camera positions are recorded and used only while the second pass builds
// its visible sets. The renderer itself keeps the second eye's camera.
bool InstallCullingHook();
bool IsCullingHooked();

void BeginCullingCapture();
void PauseCullingCapture();
void BeginCullingReplay();
void EndCullingSync(UInt32 sceneCall);

}  // namespace obvr::render
