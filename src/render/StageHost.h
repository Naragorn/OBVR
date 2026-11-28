#pragma once

#include "core/Config.h"
#include "vr/OpenVRBackend.h"

namespace obvr::render {

// Once a Present: OBVR's dome in place of SteamVR's grid (vr/LoadingStage.h)
// and the start guide's stage in it (ui/GuideStage.h) - the dome faded in
// over the main menu while the guide is up, the main menu hidden, the title
// music silent, the soft track. Moved out of CameraHook so the two that share
// the dome live together.
// What the frame says: whether the engine drew the world, whether a load runs or its menu is up, and how the
// frame was delivered (for the log of the main menu's dome, ui/MainMenuDome.h).
struct StageFrame {
	bool worldFrame = false;
	bool loading = false;
	const char* delivery = "";
};

void UpdateStage(const Config& config, const vr::OpenVRBackend& backend, const StageFrame& frame);

}  // namespace obvr::render
