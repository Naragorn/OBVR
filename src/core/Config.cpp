#include "core/Config.h"

#include "core/Log.h"
#include "platform/Win32Min.h"

namespace obvr {
namespace {

Config g_config;

// Baut den Pfad zur INI neben Oblivion.exe. GetPrivateProfileString wuerde
// einen relativen Namen sonst im Windows-Verzeichnis suchen.
bool BuildPath(const char* fileName, char* out, UInt32 outSize) {
	const DWORD moduleLength = GetModuleFileNameA(nullptr, out, outSize);
	if (moduleLength == 0 || moduleLength >= outSize) {
		return false;
	}

	UInt32 cut = moduleLength;
	while (cut > 0 && out[cut - 1] != '\\' && out[cut - 1] != '/') {
		--cut;
	}

	UInt32 i = 0;
	while (fileName[i] != '\0') {
		if (cut + i + 1 >= outSize) {
			return false;
		}
		out[cut + i] = fileName[i];
		++i;
	}
	out[cut + i] = '\0';
	return true;
}

// Eigener Vergleich ohne Gross- und Kleinschreibung: die CRT-Varianten heissen
// je nach Umgebung anders, und es geht nur um eine Handvoll Schluesselwoerter.
bool EqualsIgnoreCase(const char* a, const char* b) {
	while (*a != '\0' && *b != '\0') {
		char ca = *a;
		char cb = *b;
		if (ca >= 'A' && ca <= 'Z') {
			ca = static_cast<char>(ca - 'A' + 'a');
		}
		if (cb >= 'A' && cb <= 'Z') {
			cb = static_cast<char>(cb - 'A' + 'a');
		}
		if (ca != cb) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

float ReadFloat(const char* section, const char* key, float fallback, const char* path) {
	char buffer[64];
	if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}
	return static_cast<float>(atof(buffer));
}

UInt32 ReadUInt(const char* section, const char* key, UInt32 fallback, const char* path) {
	char buffer[64];
	const DWORD length = GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path);
	if (length == 0) {
		return fallback;
	}

	UInt32 value = 0;
	for (DWORD i = 0; i < length; ++i) {
		if (buffer[i] < '0' || buffer[i] > '9') {
			return fallback;
		}
		value = value * 10 + static_cast<UInt32>(buffer[i] - '0');
	}
	return value;
}

bool ReadBool(const char* section, const char* key, bool fallback, const char* path) {
	return ReadUInt(section, key, fallback ? 1 : 0, path) != 0;
}

vr::TrackerSource ReadSource(vr::TrackerSource fallback, const char* path) {
	char buffer[32];
	if (GetPrivateProfileStringA("Head", "Source", "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}

	if (EqualsIgnoreCase(buffer, "none")) {
		return vr::TrackerSource::None;
	}
	if (EqualsIgnoreCase(buffer, "fixed")) {
		return vr::TrackerSource::Fixed;
	}
	if (EqualsIgnoreCase(buffer, "simulated")) {
		return vr::TrackerSource::Simulated;
	}
	if (EqualsIgnoreCase(buffer, "openvr")) {
		return vr::TrackerSource::OpenVR;
	}
	if (EqualsIgnoreCase(buffer, "openxr")) {
		return vr::TrackerSource::OpenXR;
	}

	OBVR_LOG("Config: unbekannte Head.Source \"%s\", behalte bisherige Einstellung", buffer);
	return fallback;
}

const char* SourceName(vr::TrackerSource source) {
	switch (source) {
		case vr::TrackerSource::None: return "none";
		case vr::TrackerSource::Fixed: return "fixed";
		case vr::TrackerSource::Simulated: return "simulated";
		case vr::TrackerSource::OpenVR: return "openvr";
		case vr::TrackerSource::OpenXR: return "openxr";
	}
	return "?";
}

void ReadRuntimeValues(Config& config, const char* path) {
	config.tracker.source = ReadSource(config.tracker.source, path);

	config.tracker.fixedPitch = ReadFloat("Head", "FixedPitch", config.tracker.fixedPitch, path);
	config.tracker.fixedRoll = ReadFloat("Head", "FixedRoll", config.tracker.fixedRoll, path);
	config.tracker.fixedYaw = ReadFloat("Head", "FixedYaw", config.tracker.fixedYaw, path);

	config.tracker.simulatedYawAmplitude =
		ReadFloat("Head", "SimulatedYawDegrees", config.tracker.simulatedYawAmplitude, path);
	config.tracker.simulatedPitchAmplitude =
		ReadFloat("Head", "SimulatedPitchDegrees", config.tracker.simulatedPitchAmplitude, path);
	config.tracker.simulatedPeriodFrames =
		ReadUInt("Head", "SimulatedPeriodFrames", config.tracker.simulatedPeriodFrames, path);

	config.logEveryFrames = ReadUInt("Debug", "LogEveryFrames", config.logEveryFrames, path);
	config.reloadEveryFrames =
		ReadUInt("Debug", "ReloadEveryFrames", config.reloadEveryFrames, path);
}

}  // namespace

bool Config::Load(const char* fileName) {
	char path[512];
	if (!BuildPath(fileName, path, sizeof(path))) {
		OBVR_LOG("Config: Pfad zu %s nicht ermittelbar, benutze Vorgabewerte", fileName);
		return false;
	}

	cameraHookEnabled = ReadBool("Camera", "HookEnabled", cameraHookEnabled, path);
	ReadRuntimeValues(*this, path);

	OBVR_LOG("Config: %s", path);
	OBVR_LOG("Config: HookEnabled=%d Source=%s Fixed=(P %.1f, R %.1f, Y %.1f)",
	         cameraHookEnabled ? 1 : 0,
	         SourceName(tracker.source),
	         static_cast<double>(tracker.fixedPitch),
	         static_cast<double>(tracker.fixedRoll),
	         static_cast<double>(tracker.fixedYaw));
	OBVR_LOG("Config: Simuliert=(Yaw %.1f, Pitch %.1f, Periode %u) LogEveryFrames=%u ReloadEveryFrames=%u",
	         static_cast<double>(tracker.simulatedYawAmplitude),
	         static_cast<double>(tracker.simulatedPitchAmplitude),
	         tracker.simulatedPeriodFrames,
	         logEveryFrames,
	         reloadEveryFrames);
	return true;
}

bool Config::Reload(const char* fileName) {
	char path[512];
	if (!BuildPath(fileName, path, sizeof(path))) {
		return false;
	}
	ReadRuntimeValues(*this, path);
	return true;
}

Config& GetConfig() { return g_config; }

}  // namespace obvr
