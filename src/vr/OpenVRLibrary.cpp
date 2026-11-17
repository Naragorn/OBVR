#include "vr/OpenVRLibrary.h"

#include "core/Config.h"
#include "core/Log.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"

namespace obvr::vr {

// The shipped one next to OBVR.dll first, which means Data/OBSE/Plugins. That
// is inside the folder Mod Organizer 2 virtualises, so the library can ship
// as part of the mod rather than needing the Root Builder plugin to place it
// in the game root. The load happens long after USVFS has installed its
// hooks, so the virtual path resolves. Otherwise the plain name, which
// LoadLibraryA resolves next to Oblivion.exe - the layout every non-MO2
// installation uses. It is the x86 build either way: not the x64 one that
// ships with other games, and not the one inside SteamVR's own folder.
//
// A stand-in ([Debug] OpenVRLibrary, the harness without SteamVR) is looked
// for next to OBVR.dll alone, and the shipped library is not tried in its
// place.
void* LoadOpenVRLibrary(const char*& name) {
	const OpenVRLibraryPlan plan = PlanOpenVRLibrary(GetConfig().openVrLibrary);
	name = plan.name;
	if (plan.kind == OpenVRLibrary::Refused) {
		OBVR_LOG("OpenVR: [Debug] OpenVRLibrary=%s is not a plain DLL name (a file beside OBVR.dll, no folder) - "
		         "nothing loaded, camera stays unchanged",
		         plan.name);
		return nullptr;
	}
	void* module = nullptr;
	char path[512];
	if (platform::BuildPluginPath(plan.name, path, sizeof(path))) {
		module = LoadLibraryA(path);
	}
	if (plan.kind == OpenVRLibrary::StandIn) {
		if (module != nullptr) {
			OBVR_LOG("OpenVR: [Debug] OpenVRLibrary=%s loaded in place of openvr_api.dll - a stand-in runtime, "
			         "no SteamVR and no headset behind it",
			         plan.name);
		} else {
			OBVR_LOG("OpenVR: [Debug] OpenVRLibrary=%s not found beside OBVR.dll - camera stays unchanged "
			         "(openvr_api.dll is not tried in its place)",
			         plan.name);
		}
		return module;
	}
	if (module == nullptr) {
		module = LoadLibraryA(plan.name);
	}
	if (module == nullptr) {
		OBVR_LOG("OpenVR: %s not found, camera stays unchanged", plan.name);
		OBVR_LOG("OpenVR: the x86 build belongs next to OBVR.dll or next to Oblivion.exe");
	}
	return module;
}

}  // namespace obvr::vr
