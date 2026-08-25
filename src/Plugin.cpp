#include "camera/CameraHook.h"
#include "core/Config.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "obse/PluginInterface.h"
#include "platform/Win32Min.h"

namespace {

constexpr const char* kPluginName = "OBVR";
constexpr UInt32 kPluginVersion = 1;  // 0.0.1

// xOBSE meldet sich als Version 22. Aeltere OBSE-Stände kennen Felder der
// Interface-Struktur nicht, die OBVR ohnehin nicht anfasst - deshalb wird nur
// grob nach unten abgesichert.
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
		// Im Construction Set gibt es keine Spielkamera zu hooken. Das Plugin
		// laedt trotzdem, tut aber nichts.
		OBVR_LOG("Editor erkannt, OBVR bleibt inaktiv");
		return true;
	}

	OBVR_LOG("OBSE-Version %u, Oblivion-Version %08X", obse->obseVersion, obse->oblivionVersion);

	if (obse->obseVersion < kMinimumObseVersion) {
		OBVR_LOG("OBSE zu alt: %u, benoetigt mindestens %u", obse->obseVersion, kMinimumObseVersion);
		return false;
	}

	if (obse->oblivionVersion != obvr::addr::kOblivionVersion_1_2_416) {
		// Saemtliche Hook-Adressen gelten ausschliesslich fuer 1.2.0.416.
		OBVR_LOG("Falsche Oblivion-Version: %08X, benoetigt %08X",
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

	if (!obvr::GetConfig().cameraHookEnabled) {
		OBVR_LOG("Kamera-Hook per Konfiguration abgeschaltet");
		return true;
	}

	if (!obvr::camera::Install()) {
		// Der Hook scheitert bewusst weich: Oblivion soll ohne VR-Kamera
		// startbar bleiben, statt beim Laden zu sterben.
		OBVR_LOG("Kamera-Hook nicht gesetzt, OBVR bleibt inaktiv");
		return true;
	}

	OBVR_LOG("OBVR bereit");
	return true;
}

}  // extern "C"

extern "C" int __stdcall DllMain(void* /*module*/, unsigned long reason, void* /*reserved*/) {
	constexpr unsigned long kProcessDetach = 0;
	if (reason == kProcessDetach) {
		obvr::log::Close();
	}
	return 1;
}
