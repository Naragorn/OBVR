#pragma once
#include "obse/PluginInterface.h"

namespace obvr::game {
void InstallNativeMenuPrototype(const obse::Interface* api);
bool NativeMenuSuppressesLegacy();
bool NativeSettingsAvailable();
bool NativeSettingsOpen();
// The first start's guide (or its comfort page) is up - the guide's stage, ui/GuideStage.h.
bool StartGuideShowing();
void RequestNativeSettingsToggle();
bool TakeNativeRecenterRequest();
}
