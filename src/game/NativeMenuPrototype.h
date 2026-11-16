#pragma once
#include "obse/PluginInterface.h"

namespace obvr::game {
void InstallNativeMenuPrototype(const obse::Interface* api);
bool NativeMenuSuppressesLegacy();
bool NativeSettingsAvailable();
bool NativeSettingsOpen();
// The first start's guide (or its comfort page) is up - the guide's stage, ui/GuideStage.h.
bool StartGuideShowing();
// The start guide asked for again (the main menu's VR button, game/MainMenuVr.cpp, where these live): taken by
// the chain's start (ui::FirstStartDue's asked) the next time the main menu is in front.
void RequestStartGuide();
bool StartGuideAsked();
void ClearStartGuideAsked();
// A GenericMenu (0x3F3) on top that is not one of OBVR's own windows - a
// plugin's, such as the quest-added box Oblivion Walkers' dungeons open.
bool ForeignGenericMenuUp();
void RequestNativeSettingsToggle();
bool TakeNativeRecenterRequest();
}
