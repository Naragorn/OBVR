#include "core/Config.h"

#include "core/Log.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"

namespace obvr {
namespace {

Config g_config;

// Builds the path to the INI. Two locations are tried, in this order:
//
//   1. next to OBVR.dll, so Data/OBSE/Plugins/OBVR.ini
//   2. next to Oblivion.exe, where OBVR looked before
//
// The plugin directory comes first because it is the one Mod Organizer 2
// virtualises. MO2 manages the Data folder and nothing else, so an INI shipped
// there arrives as part of the mod and follows the active profile. An INI in
// the game root can only be delivered through the separate Root Builder
// plugin, and Root Builder's documentation lists .ini files among the usual
// exclusions - it may quietly never be deployed at all.
//
// The game root remains as a fallback so existing installations keep working
// untouched.
//
// An absolute path is needed either way: GetPrivateProfileString resolves a
// relative name against the Windows directory.
bool BuildPath(const char* fileName, char* out, UInt32 outSize) {
	if (platform::BuildPluginPath(fileName, out, outSize) &&
	    GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) {
		return true;
	}

	return platform::BuildGamePath(fileName, out, outSize);
}

// A case insensitive comparison of our own: the CRT variants go by different
// names depending on the environment, and this is only about a handful of
// keywords.
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

// Reads a virtual-key code. Accepts decimal (46) as well as hex with an 0x
// prefix (0x2E).
//
// The hex form matters: Microsoft's virtual-key table lists the codes in hex,
// so that is what people will copy. ReadUInt would quietly fall back on such
// a value, and the key would then simply appear not to work.
UInt32 ReadKeyCode(const char* section, const char* key, UInt32 fallback, const char* path) {
	char buffer[64];
	const DWORD length = GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path);
	if (length == 0) {
		return fallback;
	}

	DWORD i = 0;
	UInt32 base = 10;
	if (length > 2 && buffer[0] == '0' && (buffer[1] == 'x' || buffer[1] == 'X')) {
		base = 16;
		i = 2;
	}

	UInt32 value = 0;
	for (; i < length; ++i) {
		const char c = buffer[i];
		UInt32 digit = 0;

		if (c >= '0' && c <= '9') {
			digit = static_cast<UInt32>(c - '0');
		} else if (base == 16 && c >= 'a' && c <= 'f') {
			digit = static_cast<UInt32>(c - 'a' + 10);
		} else if (base == 16 && c >= 'A' && c <= 'F') {
			digit = static_cast<UInt32>(c - 'A' + 10);
		} else {
			OBVR_LOG("Config: %s.%s=\"%s\" is not a number, keeping the previous key",
			         section, key, buffer);
			return fallback;
		}

		value = value * base + digit;
	}

	// Virtual-key codes fit in a byte. Anything larger is a typo, and polling
	// a key that cannot exist would look exactly like the key not working.
	if (value > 0xFF) {
		OBVR_LOG("Config: %s.%s=%u is not a virtual-key code (0 to 255), keeping the previous key",
		         section, key, value);
		return fallback;
	}

	return value;
}

vr::StereoMode ReadStereoMode(vr::StereoMode fallback, const char* path) {
	char buffer[32];
	if (GetPrivateProfileStringA("Render", "Stereo", "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}

	if (EqualsIgnoreCase(buffer, "none") || EqualsIgnoreCase(buffer, "off")) {
		return vr::StereoMode::None;
	}
	if (EqualsIgnoreCase(buffer, "aer") || EqualsIgnoreCase(buffer, "alternate")) {
		return vr::StereoMode::AlternateEyes;
	}

	if (EqualsIgnoreCase(buffer, "dual") || EqualsIgnoreCase(buffer, "dualpass")) {
		return vr::StereoMode::DualPass;
	}

	OBVR_LOG("Config: unknown Render.Stereo \"%s\", keeping the previous setting", buffer);
	return fallback;
}

const char* StereoModeName(vr::StereoMode mode) {
	switch (mode) {
		case vr::StereoMode::None: return "none";
		case vr::StereoMode::AlternateEyes: return "aer";
		case vr::StereoMode::DualPass: return "dual";
	}
	return "?";
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

	OBVR_LOG("Config: unknown Head.Source \"%s\", keeping the previous setting", buffer);
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

	config.recenterKey = ReadKeyCode("Head", "RecenterKey", config.recenterKey, path);

	config.tracker.positionalTracking =
		ReadBool("Head", "PositionalTracking", config.tracker.positionalTracking, path);
	config.tracker.unitsPerMetre =
		ReadFloat("Head", "UnitsPerMetre", config.tracker.unitsPerMetre, path);
	config.tracker.movementScale =
		ReadFloat("Head", "HeadMovementScale", config.tracker.movementScale, path);
	config.tracker.maxOffsetUnits =
		ReadFloat("Head", "MaxLeanUnits", config.tracker.maxOffsetUnits, path);
	config.tracker.renderToHeadset =
		ReadBool("Render", "Enabled", config.tracker.renderToHeadset, path);
	config.tracker.submitGameFrame =
		ReadBool("Render", "GameFrame", config.tracker.submitGameFrame, path);
	config.tracker.stereo = ReadStereoMode(config.tracker.stereo, path);
	config.tracker.eyeSeparationScale =
		ReadFloat("Render", "EyeSeparationScale", config.tracker.eyeSeparationScale, path);
	config.tracker.gameFovDegrees =
		ReadFloat("Render", "GameFovDegrees", config.tracker.gameFovDegrees, path);
	config.tracker.gameFovIsFor4x3 =
		ReadBool("Render", "GameFovIsFor4x3", config.tracker.gameFovIsFor4x3, path);
	config.tracker.submitAtFrameEnd =
		ReadBool("Render", "SubmitAtFrameEnd", config.tracker.submitAtFrameEnd, path);
	config.tracker.showMenus = ReadBool("Render", "ShowMenus", config.tracker.showMenus, path);
	config.tracker.gameFovOverride =
		ReadFloat("Render", "GameFovOverride", config.tracker.gameFovOverride, path);
	config.tracker.menuScale = ReadFloat("Render", "MenuScale", config.tracker.menuScale, path);
	config.tracker.menuAspect =
		ReadFloat("Render", "MenuAspect", config.tracker.menuAspect, path);
	config.tracker.setRenderSize =
		ReadBool("Render", "SetGameResolution", config.tracker.setRenderSize, path);
	config.tracker.renderWidth =
		ReadUInt("Render", "GameResolutionWidth", config.tracker.renderWidth, path);
	config.tracker.renderHeight =
		ReadUInt("Render", "GameResolutionHeight", config.tracker.renderHeight, path);
	config.tracker.matchHeadsetFov =
		ReadBool("Render", "MatchHeadsetFov", config.tracker.matchHeadsetFov, path);
	config.tracker.hudOverlay =
		ReadBool("Render", "HudOverlay", config.tracker.hudOverlay, path);
	config.tracker.hudBetweenPasses =
		ReadBool("Render", "HudBetweenPasses", config.tracker.hudBetweenPasses, path);
	config.tracker.hudDistanceMetres =
		ReadFloat("Render", "HudDistanceMetres", config.tracker.hudDistanceMetres, path);
	config.tracker.hudWidthMetres =
		ReadFloat("Render", "HudWidthMetres", config.tracker.hudWidthMetres, path);
	config.look.blockVerticalLook =
		ReadBool("Look", "BlockVerticalLook", config.look.blockVerticalLook, path);

	// VerticalLookRange was one value for both directions until it turned out
	// that a range sized for looking up stops at the hips on the way down. It
	// is still read, as the fallback for both halves, so that an INI written
	// before the split keeps meaning what it said rather than quietly
	// reverting to the defaults.
	const float legacyRange =
		ReadFloat("Look", "VerticalLookRange", config.look.verticalLookUpRange, path);
	config.look.verticalLookUpRange = ReadFloat("Look", "VerticalLookUpRange", legacyRange, path);
	config.look.verticalLookDownRange =
		ReadFloat("Look", "VerticalLookDownRange", legacyRange, path);
	config.look.smoothVerticalLook =
		ReadBool("Look", "SmoothVerticalLook", config.look.smoothVerticalLook, path);
	config.look.verticalLookSpeed =
		ReadFloat("Look", "VerticalLookSpeed", config.look.verticalLookSpeed, path);
	config.look.smoothTurning =
		ReadBool("Look", "SmoothTurning", config.look.smoothTurning, path);
	config.look.turnSpeed = ReadFloat("Look", "TurnSpeed", config.look.turnSpeed, path);

	config.logEveryFrames = ReadUInt("Debug", "LogEveryFrames", config.logEveryFrames, path);
	config.reloadEveryFrames =
		ReadUInt("Debug", "ReloadEveryFrames", config.reloadEveryFrames, path);
	config.dualPassProbe = ReadUInt("Debug", "DualPassProbe", config.dualPassProbe, path);
	config.hudProbe = ReadBool("Debug", "HudProbe", config.hudProbe, path);
}

}  // namespace

bool Config::Load(const char* fileName) {
	char path[512];
	if (!BuildPath(fileName, path, sizeof(path))) {
		OBVR_LOG("Config: cannot determine the path to %s, using defaults", fileName);
		return false;
	}

	// Whether the file exists has to be asked separately. GetPrivateProfileString
	// returns the supplied default for a missing file just as it does for a
	// missing key, so without this check a forgotten OBVR.ini looks exactly
	// like one that happens to contain the defaults - and OBVR would silently
	// do nothing at all, since the default source is a fixed rotation of zero
	// degrees.
	//
	// OBVR does not write the file itself. Under Mod Organizer 2 the plugin
	// directory is virtualised, so a generated INI would be redirected into the
	// Overwrite folder, which has the highest priority of any mod - it would
	// shadow the INI shipped with the mod, and editing that one would then have
	// no effect with nothing to indicate why.
	const bool exists = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;

	cameraHookEnabled = ReadBool("Camera", "HookEnabled", cameraHookEnabled, path);
	ReadRuntimeValues(*this, path);

	if (exists) {
		OBVR_LOG("Config: %s", path);
	} else {
		OBVR_LOG("Config: %s not found - every value below is a built-in default", path);
		OBVR_LOG("Config: OBVR.ini belongs next to OBVR.dll, or next to Oblivion.exe");
	}

	OBVR_LOG("Config: HookEnabled=%d Source=%s Fixed=(P %.1f, R %.1f, Y %.1f)",
	         cameraHookEnabled ? 1 : 0,
	         SourceName(tracker.source),
	         static_cast<double>(tracker.fixedPitch),
	         static_cast<double>(tracker.fixedRoll),
	         static_cast<double>(tracker.fixedYaw));
	OBVR_LOG("Config: Simulated=(Yaw %.1f, Pitch %.1f, Period %u) LogEveryFrames=%u ReloadEveryFrames=%u",
	         static_cast<double>(tracker.simulatedYawAmplitude),
	         static_cast<double>(tracker.simulatedPitchAmplitude),
	         tracker.simulatedPeriodFrames,
	         logEveryFrames,
	         reloadEveryFrames);
	// Logged in hex as well, because that is the form the virtual-key tables
	// use - it saves converting in your head when a binding misbehaves.
	OBVR_LOG("Config: RecenterKey=%u (0x%02X)%s", recenterKey, recenterKey,
	         recenterKey == 0 ? " - recentering disabled" : "");
	// The effective figure is logged alongside the two it comes from, because
	// that product is what every lean is actually measured in and working it
	// out by hand while reading a log is exactly the step that gets skipped.
	OBVR_LOG("Config: PositionalTracking=%d UnitsPerMetre=%.2f HeadMovementScale=%.2f "
	         "(effective %.2f) MaxLeanUnits=%.1f",
	         tracker.positionalTracking ? 1 : 0,
	         static_cast<double>(tracker.unitsPerMetre),
	         static_cast<double>(tracker.movementScale),
	         static_cast<double>(tracker.EffectiveUnitsPerMetre()),
	         static_cast<double>(tracker.maxOffsetUnits));
	OBVR_LOG("Config: BlockVerticalLook=%d VerticalLookRange=(up %.1f, down %.1f) "
	         "SmoothVerticalLook=%d (%.1f)",
	         look.blockVerticalLook ? 1 : 0,
	         static_cast<double>(look.verticalLookUpRange),
	         static_cast<double>(look.verticalLookDownRange),
	         look.smoothVerticalLook ? 1 : 0,
	         static_cast<double>(look.verticalLookSpeed));
	OBVR_LOG("Config: SmoothTurning=%d TurnSpeed=%.1f",
	         look.smoothTurning ? 1 : 0,
	         static_cast<double>(look.turnSpeed));
	// Worth its own line despite being one flag: it is the setting that
	// decides whether OBVR takes the headset away from whatever else is
	// using it, and that should be visible in the log without hunting.
	OBVR_LOG("Config: Render.Enabled=%d GameFrame=%d Stereo=%s EyeSeparationScale=%.2f%s",
	         tracker.renderToHeadset ? 1 : 0, tracker.submitGameFrame ? 1 : 0,
	         StereoModeName(tracker.stereo),
	         static_cast<double>(tracker.eyeSeparationScale),
	         tracker.renderToHeadset ? " - OBVR will claim the VR scene" : "");
	OBVR_LOG("Config: Render.GameFovDegrees=%.1f IsFor4x3=%d SubmitAtFrameEnd=%d",
	         static_cast<double>(tracker.gameFovDegrees), tracker.gameFovIsFor4x3 ? 1 : 0,
	         tracker.submitAtFrameEnd ? 1 : 0);
	OBVR_LOG("Config: Render.ShowMenus=%d MenuScale=%.2f GameFovOverride=%.1f",
	         tracker.showMenus ? 1 : 0, static_cast<double>(tracker.menuScale),
	         static_cast<double>(tracker.gameFovOverride));
	OBVR_LOG("Config: Render.MatchHeadsetFov=%d SetGameResolution=%d (%ux%u)",
	         tracker.matchHeadsetFov ? 1 : 0, tracker.setRenderSize ? 1 : 0,
	         tracker.renderWidth, tracker.renderHeight);
	OBVR_LOG("Config: Render.HudOverlay=%d BetweenPasses=%d Distance=%.2fm Width=%.2fm",
	         tracker.hudOverlay ? 1 : 0, tracker.hudBetweenPasses ? 1 : 0,
	         static_cast<double>(tracker.hudDistanceMetres),
	         static_cast<double>(tracker.hudWidthMetres));
	if (tracker.stereo == vr::StereoMode::DualPass) {
		// The known limit, stated up front rather than discovered in the
		// headset: the 2D layer draws after both passes, into the frame the
		// monitor gets, so it is not in either eye's picture yet. Menus and
		// videos still arrive, through the flat path; what is missing in the
		// world is the HUD.
		OBVR_LOG("Config: Render.Stereo=dual - the world is drawn twice per frame, once "
		         "per eye; the HUD draws after both passes and is not in the world "
		         "picture yet");
	}
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
