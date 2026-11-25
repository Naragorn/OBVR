#include "game/GuideReturn.h"

#include "platform/Win32Min.h"

#include <cstdio>

#include "core/Config.h"
#include "core/Log.h"
#include "platform/PluginPath.h"
#include "ui/GuideRevision.h"

namespace obvr::game {
namespace {

// OBVR.ini where Config::Load found it: beside the plugin, or in the game's
// root for an older install (core/Config.cpp's BuildPath).
bool IniPath(char* out, UInt32 size) {
	if (platform::BuildPluginPath("OBVR.ini", out, size) && GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) {
		return true;
	}
	return platform::BuildGamePath("OBVR.ini", out, size);
}

}  // namespace

void ReturnGuideForNewRevision() {
	if (HarnessRunActive()) {
		OBVR_LOG("Onboarding: a harness run - the start guide's revision neither read nor written");
		return;
	}
	char path[512];
	if (!IniPath(path, sizeof(path))) {
		return;
	}
	const UInt32 seen = GetPrivateProfileIntA("Onboarding", "GuideRevision", 0, path);
	if (!ui::GuideComesBack(seen, ui::kGuideRevision)) {
		OBVR_LOG("Onboarding: the start guide's revision %u, as OBVR.ini names it - it does not come back", seen);
		return;
	}
	char value[16];
	std::snprintf(value, sizeof(value), "%u", ui::kGuideRevision);
	const bool switchedOn = SaveSetting("Onboarding", "StartGuide", "1");
	const bool noted = SaveSetting("Onboarding", "GuideRevision", value);
	if (switchedOn) {
		GetConfig().onboardingStartGuide = true;
	}
	OBVR_LOG("Onboarding: this OBVR's start guide is revision %u, OBVR.ini named %u - the guide comes back once%s",
	         ui::kGuideRevision, seen,
	         switchedOn && noted ? "" : " (OBVR.ini could NOT be written - it may come again at the next start)");
}

}  // namespace obvr::game
