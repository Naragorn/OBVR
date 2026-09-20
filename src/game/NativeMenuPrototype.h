#pragma once
#include "obse/PluginInterface.h"

namespace obvr::game {
void InstallNativeMenuPrototype(const obse::Interface* api);
bool NativeMenuSuppressesLegacy();
bool NativeSettingsAvailable();
bool NativeSettingsOpen();
void RequestNativeSettingsToggle();
bool TakeNativeRecenterRequest();
}
