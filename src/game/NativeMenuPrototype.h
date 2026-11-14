#pragma once
#include "obse/PluginInterface.h"

namespace obvr::game {
void InstallNativeMenuPrototype(const obse::Interface* api);
bool NativeMenuSuppressesLegacy();
bool NativeSettingsAvailable();
bool NativeSettingsOpen();
// The first start's guide (or its comfort page) is up - the guide's stage, ui/GuideStage.h.
bool StartGuideShowing();
// A GenericMenu (0x3F3) on top that is not one of OBVR's own windows - a
// plugin's, such as the quest-added box Oblivion Walkers' dungeons open.
bool ForeignGenericMenuUp();
void RequestNativeSettingsToggle();
bool TakeNativeRecenterRequest();
}
