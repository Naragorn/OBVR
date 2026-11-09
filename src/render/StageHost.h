#pragma once

#include "core/Config.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {

// Once a Present: OBVR's dome in place of SteamVR's grid (vr/LoadingStage.h)
// and the start guide's stage in it (ui/GuideStage.h) - the dome faded in
// over the main menu while the guide is up, the main menu hidden, the title
// music silent, the soft track. Moved out of CameraHook so the two that share
// the dome live together.
void UpdateStage(const Config& config, const vr::OpenVRBackend& backend);

}  // namespace obvr::render
