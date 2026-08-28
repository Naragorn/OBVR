#include "camera/CameraHook.h"
#include "core/Config.h"
#include "core/Log.h"
#include "game/DialogZoom.h"
#include "game/GameAddresses.h"
#include "obse/PluginInterface.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"

namespace {

constexpr const char* kPluginName = "OBVR";

// Reported to OBSE. Still 1, unchanged since 0.0.1 - it identifies the
// plugin, not the milestone.
constexpr UInt32 kPluginVersion = 1;

// xOBSE reports itself as version 22. Older OBSE revisions do not know some
// fields of the interface struct, none of which OBVR touches - so the check
// is only a coarse lower bound.
constexpr UInt32 kMinimumObseVersion = 20;

}  // namespace

extern "C" {

__declspec(dllexport) bool OBSEPlugin_Query(const obvr::obse::Interface* obse,
                                            obvr::obse::Info* info) {
	info->infoVersion = obvr::obse::Info::kInfoVersion;
	info->name = kPluginName;
	info->version = kPluginVersion;

	obvr::log::Open("OBVR.log");
	OBVR_LOG("OBVR %u - Query", kPluginVersion);

	if (obse->isEditor != 0) {
		// There is no game camera to hook in the Construction Set. The plugin
		// still loads, but does nothing.
		OBVR_LOG("Editor detected, OBVR stays inactive");
		return true;
	}

	OBVR_LOG("OBSE version %u, Oblivion version %08X", obse->obseVersion, obse->oblivionVersion);

	if (obse->obseVersion < kMinimumObseVersion) {
		OBVR_LOG("OBSE too old: %u, needs at least %u", obse->obseVersion, kMinimumObseVersion);
		return false;
	}

	if (obse->oblivionVersion != obvr::addr::kOblivionVersion_1_2_416) {
		// Every hook address applies to 1.2.0.416 and nothing else.
		OBVR_LOG("Wrong Oblivion version: %08X, needs %08X",
		         obse->oblivionVersion, obvr::addr::kOblivionVersion_1_2_416);
		return false;
	}

	return true;
}

__declspec(dllexport) bool OBSEPlugin_Load(const obvr::obse::Interface* obse) {
	OBVR_LOG("OBVR %u - Load", kPluginVersion);

	if (obse->isEditor != 0) {
		return true;
	}

	obvr::GetConfig().Load("OBVR.ini");

	// Before the hooks and regardless of them: the zoom patch stands on its
	// own, and a session whose camera hook failed still deserves dialogues
	// without the dead pause.
	obvr::game::ApplyDialogZoom(obvr::GetConfig().dialogZoom);

	if (!obvr::GetConfig().cameraHookEnabled) {
		OBVR_LOG("Camera hook disabled by configuration");
		return true;
	}

	if (!obvr::camera::Install()) {
		// The hook fails softly on purpose: Oblivion should stay startable
		// without the VR camera rather than die while loading.
		OBVR_LOG("Camera hook not installed, OBVR stays inactive");
		return true;
	}

	OBVR_LOG("OBVR ready");
	return true;
}

}  // extern "C"

extern "C" int __stdcall DllMain(void* module, unsigned long reason, void* /*reserved*/) {
	constexpr unsigned long kProcessDetach = 0;
	constexpr unsigned long kProcessAttach = 1;

	if (reason == kProcessAttach) {
		// The only point at which OBVR is handed its own module handle.
		// Everything that locates a file next to OBVR.dll depends on it, so it
		// is stored before any of that can run.
		obvr::platform::SetPluginModule(module);
	} else if (reason == kProcessDetach) {
		obvr::log::Close();
	}

	return 1;
}
