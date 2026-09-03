#include "core/Config.h"

#include "core/Log.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "render/MenuShade.h"

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

// Reads HudAnchor, which names a place rather than answers a yes-or-no: the
// setting is "head" or "world", and a word is what a reader of the file will
// understand without looking the key up.
//
// An unrecognised word keeps the previous setting and says so, the same way
// Stereo does - silently defaulting would leave the HUD somewhere the person
// did not ask for with nothing in the log to explain it.
bool ReadAnchorIsWorld(const char* section, const char* key, bool fallback,
                       const char* path) {
	char buffer[32];
	if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}

	if (EqualsIgnoreCase(buffer, "head") || EqualsIgnoreCase(buffer, "hmd")) {
		return false;
	}
	if (EqualsIgnoreCase(buffer, "world") || EqualsIgnoreCase(buffer, "room")) {
		return true;
	}

	// The section is named as well as the key. This reader is used from more
	// than one section now, and "unknown Render.Anchor" for a setting in
	// [SettingsMenu] sends the reader to the wrong part of the file.
	OBVR_LOG("Config: unknown %s.%s \"%s\", keeping the previous setting", section, key, buffer);
	return fallback;
}

// Reads Menus, which names a place the same way HudAnchor does: "cinema" or
// "world". Kept as its own reader rather than folded into the one above,
// because the words are not the same ones - a menu is not on a head - and a
// reader who writes "head" here should be told, not quietly given the screen.
bool ReadMenusInWorld(const char* section, const char* key, bool fallback, const char* path) {
	char buffer[32];
	if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}

	if (EqualsIgnoreCase(buffer, "cinema") || EqualsIgnoreCase(buffer, "screen") ||
	    EqualsIgnoreCase(buffer, "flat")) {
		return false;
	}
	if (EqualsIgnoreCase(buffer, "world") || EqualsIgnoreCase(buffer, "room")) {
		return true;
	}

	OBVR_LOG("Config: unknown Render.%s \"%s\", keeping the previous setting", key, buffer);
	return fallback;
}

// Reads a colour written as six hex digits (RRGGBB, with or without '#').
// The parse itself lives in MenuShade.h with the rest of the shade logic; a
// value that does not parse keeps the previous colour and says so, the same
// way every other worded setting here behaves.
UInt32 ReadHexColor(const char* section, const char* key, UInt32 fallback, const char* path) {
	char buffer[32];
	if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}

	UInt32 rgb = 0;
	if (!render::ParseHexColor(buffer, rgb)) {
		OBVR_LOG("Config: %s.%s=\"%s\" is not an RRGGBB colour, keeping the previous one",
		         section, key, buffer);
		return fallback;
	}
	return rgb;
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
	config.tracker.uiFollowsFrameSize = ReadBool("Render", "UiFollowsFrameSize",
	                                             config.tracker.uiFollowsFrameSize, path);
	config.tracker.hudOverlay =
		ReadBool("Render", "HudOverlay", config.tracker.hudOverlay, path);
	config.tracker.hudBetweenPasses =
		ReadBool("Render", "HudBetweenPasses", config.tracker.hudBetweenPasses, path);
	config.tracker.hudAnchorWorld =
		ReadAnchorIsWorld("Render", "HudAnchor", config.tracker.hudAnchorWorld, path);
	config.tracker.menuShade =
		ReadBool("Render", "MenuShade", config.tracker.menuShade, path);
	config.tracker.menuShadeColorRgb =
		ReadHexColor("Render", "MenuShadeColor", config.tracker.menuShadeColorRgb, path);
	config.tracker.menuShadeStrength =
		ReadFloat("Render", "MenuShadeStrength", config.tracker.menuShadeStrength, path);
	config.tracker.menuSingleBorder =
		ReadBool("Render", "MenuSingleBorder", config.tracker.menuSingleBorder, path);
	config.dialogZoom = ReadBool("Look", "DialogZoom", config.dialogZoom, path);
	config.dialogFirstPerson =
		ReadBool("Look", "DialogFirstPerson", config.dialogFirstPerson, path);
	config.tracker.menusInWorld =
		ReadMenusInWorld("Render", "Menus", config.tracker.menusInWorld, path);
	config.tracker.liveMenuBackground = ReadBool(
		"Render", "LiveMenuBackground", config.tracker.liveMenuBackground, path);
	config.tracker.hudDistanceMetres =
		ReadFloat("Render", "HudDistanceMetres", config.tracker.hudDistanceMetres, path);
	config.tracker.hudWidthMetres =
		ReadFloat("Render", "HudWidthMetres", config.tracker.hudWidthMetres, path);
	config.tracker.menuStandIn =
		ReadBool("Render", "MenuStandIn", config.tracker.menuStandIn, path);
	config.tracker.crosshair = ReadBool("Render", "Crosshair", config.tracker.crosshair, path);
	config.tracker.crosshairDynamic =
		ReadBool("Render", "CrosshairDynamic", config.tracker.crosshairDynamic, path);
	config.tracker.crosshairDistanceMetres =
		ReadFloat("Render", "CrosshairDistanceMetres",
	              config.tracker.crosshairDistanceMetres, path);
	config.tracker.crosshairDepthSpeed =
		ReadFloat("Render", "CrosshairDepthSpeed", config.tracker.crosshairDepthSpeed, path);
	config.tracker.crosshairProbe =
		ReadBool("Render", "CrosshairProbe", config.tracker.crosshairProbe, path);
	config.tracker.crosshairSizeAtOneMetre =
		ReadFloat("Render", "CrosshairSizeAtOneMetre",
	              config.tracker.crosshairSizeAtOneMetre, path);
	config.tracker.crosshairSourceShare =
		ReadFloat("Render", "CrosshairSourceShare", config.tracker.crosshairSourceShare, path);
	config.tracker.crosshairOnlyWhenNeeded = ReadBool(
		"Render", "CrosshairOnlyWhenNeeded", config.tracker.crosshairOnlyWhenNeeded, path);
	config.tracker.crosshairInThirdPerson = ReadBool(
		"Render", "CrosshairInThirdPerson", config.tracker.crosshairInThirdPerson, path);
	config.tracker.crosshairPersistentCache = ReadBool(
		"Render", "CrosshairPersistentCache", config.tracker.crosshairPersistentCache, path);
	config.tracker.crosshairOnlyWhenNeededThirdPerson =
		ReadBool("Render", "CrosshairOnlyWhenNeeded3rdPerson",
	             config.tracker.crosshairOnlyWhenNeededThirdPerson, path);
	config.look.blockVerticalLook =
		ReadBool("Look", "BlockVerticalLook", config.look.blockVerticalLook, path);
	config.aimFollowsGaze =
		ReadBool("Look", "AimFollowsGaze", config.aimFollowsGaze, path);
	config.aimInThirdPerson =
		ReadBool("Look", "AimInThirdPerson", config.aimInThirdPerson, path);
	config.aimAtSource = ReadBool("Look", "AimAtSource", config.aimAtSource, path);
	config.thirdPersonAimVisualPercent =
		ReadFloat("Look", "ThirdPersonAimVisualPercent",
		          config.thirdPersonAimVisualPercent, path);
	config.thirdPersonBodyFollowsGazeUnarmed =
		ReadBool("Look", "ThirdPersonBodyFollowsGazeUnarmed",
		         config.thirdPersonBodyFollowsGazeUnarmed, path);
	config.thirdPersonHeadFollowsGaze =
		ReadBool("Look", "ThirdPersonHeadFollowsGaze",
		         config.thirdPersonHeadFollowsGaze, path);
	// ReadKeyCode rather than ReadUInt, which takes decimal only and returns the
	// FALLBACK on anything else - so a virtual-key code written the way every
	// table prints them would have been silently ignored. That trap was harmless
	// while this was the only key here and the INI said 1; it stopped being
	// harmless the moment AimCastKey=0x43 appeared underneath it.
	config.aimAttackKey = ReadKeyCode("Look", "AimAttackKey", config.aimAttackKey, path);
	config.aimTurnSpeed = ReadFloat("Look", "AimTurnSpeed", config.aimTurnSpeed, path);
	config.aimReturnOnRelease =
		ReadBool("Look", "AimReturnOnRelease", config.aimReturnOnRelease, path);
	config.aimTurnOnShotOnly =
		ReadBool("Look", "AimTurnOnShotOnly", config.aimTurnOnShotOnly, path);
	config.aimShotTrace = ReadBool("Look", "AimShotTrace", config.aimShotTrace, path);
	config.aimWeaponFollowsGaze =
		ReadBool("Look", "AimWeaponFollowsGaze", config.aimWeaponFollowsGaze, path);
	config.aimCastFollowsGaze =
		ReadBool("Look", "AimCastFollowsGaze", config.aimCastFollowsGaze, path);
	config.aimCastKey = ReadKeyCode("Look", "AimCastKey", config.aimCastKey, path);
	config.aimCastHoldSeconds =
		ReadFloat("Look", "AimCastHoldSeconds", config.aimCastHoldSeconds, path);
	config.aimCastTrace = ReadBool("Look", "AimCastTrace", config.aimCastTrace, path);
	config.aimCastAtSpawn =
		ReadBool("Look", "AimCastAtSpawn", config.aimCastAtSpawn, path);
	config.aimCastTurnAfterSeconds =
		ReadFloat("Look", "AimCastTurnAfterSeconds", config.aimCastTurnAfterSeconds, path);
	config.aimCastTurnMarginSeconds =
		ReadFloat("Look", "AimCastTurnMarginSeconds", config.aimCastTurnMarginSeconds, path);
	config.aimCastArmLimitSeconds =
		ReadFloat("Look", "AimCastArmLimitSeconds", config.aimCastArmLimitSeconds, path);

	config.settingsMenuKey =
		ReadKeyCode("SettingsMenu", "Key", config.settingsMenuKey, path);
	config.settingsMenuDistanceMetres =
		ReadFloat("SettingsMenu", "DistanceMetres", config.settingsMenuDistanceMetres, path);
	config.settingsMenuWidthMetres =
		ReadFloat("SettingsMenu", "WidthMetres", config.settingsMenuWidthMetres, path);
	config.settingsMenuInWorld =
		ReadAnchorIsWorld("SettingsMenu", "Anchor", config.settingsMenuInWorld, path);

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
	config.swapEyeOrder = ReadBool("Debug", "SwapEyeOrder", config.swapEyeOrder, path);
	config.hudProbe = ReadBool("Debug", "HudProbe", config.hudProbe, path);
	config.aimProbe = ReadBool("Debug", "AimProbe", config.aimProbe, path);
	config.thirdPersonProbe =
		ReadBool("Debug", "ThirdPersonProbe", config.thirdPersonProbe, path);
	config.menuWorldProbe = ReadBool("Debug", "MenuWorldProbe", config.menuWorldProbe, path);
	config.layoutProbe = ReadBool("Debug", "LayoutProbe", config.layoutProbe, path);
	config.cursorProbe = ReadBool("Debug", "CursorProbe", config.cursorProbe, path);
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
	         "SmoothVerticalLook=%d (%.1f) AimFollowsGaze=%d AimInThirdPerson=%d AimAtSource=%d ThirdPersonAimVisual=%.0f%% BodyUnarmed=%d HeadFollows=%d AimAttackKey=%u "
	         "AimTurnSpeed=%.1f",
	         look.blockVerticalLook ? 1 : 0,
	         static_cast<double>(look.verticalLookUpRange),
	         static_cast<double>(look.verticalLookDownRange),
	         look.smoothVerticalLook ? 1 : 0,
	         static_cast<double>(look.verticalLookSpeed),
	         aimFollowsGaze ? 1 : 0,
	         aimInThirdPerson ? 1 : 0,
	         aimAtSource ? 1 : 0,
	         static_cast<double>(thirdPersonAimVisualPercent),
	         thirdPersonBodyFollowsGazeUnarmed ? 1 : 0,
	         thirdPersonHeadFollowsGaze ? 1 : 0,
	         aimAttackKey,
	         static_cast<double>(aimTurnSpeed));
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
	OBVR_LOG("Config: Render.MatchHeadsetFov=%d SetGameResolution=%d (%ux%u) "
	         "UiFollowsFrameSize=%d",
	         tracker.matchHeadsetFov ? 1 : 0, tracker.setRenderSize ? 1 : 0,
	         tracker.renderWidth, tracker.renderHeight,
	         tracker.uiFollowsFrameSize ? 1 : 0);
	OBVR_LOG("Config: Render.HudOverlay=%d BetweenPasses=%d Anchor=%s Distance=%.2fm "
	         "Width=%.2fm",
	         tracker.hudOverlay ? 1 : 0, tracker.hudBetweenPasses ? 1 : 0,
	         tracker.hudAnchorWorld ? "world" : "head",
	         static_cast<double>(tracker.hudDistanceMetres),
	         static_cast<double>(tracker.hudWidthMetres));
	OBVR_LOG("Config: Render.LiveMenuBackground=%d MenuStandIn=%d MenuShade=%d Color=%06X "
	         "Strength=%.2f MenuSingleBorder=%d Look.DialogZoom=%d DialogFirstPerson=%d",
	         tracker.liveMenuBackground ? 1 : 0, tracker.menuStandIn ? 1 : 0,
	         tracker.menuShade ? 1 : 0, tracker.menuShadeColorRgb,
	         static_cast<double>(tracker.menuShadeStrength),
	         tracker.menuSingleBorder ? 1 : 0, dialogZoom ? 1 : 0,
	         dialogFirstPerson ? 1 : 0);
	if (menuWorldProbe) {
		// Named at load because the probe changes what the monitor shows on
		// menu frames, and a run whose log does not say it was a probe run
		// gets its oddities blamed on the wrong code.
		OBVR_LOG("Config: Debug.MenuWorldProbe=1 - a few held menu frames will run a "
		         "self-initiated world render, counted in the log");
	}
	if (layoutProbe) {
		// Named at load for the same reason as the probe above: a run that
		// stalls every couple of seconds should say in its log why.
		OBVR_LOG("Config: Debug.LayoutProbe=1 - the covered rectangle of cinema and menu "
		         "frames is measured and logged every couple of seconds");
	}
	if (cursorProbe) {
		OBVR_LOG("Config: Debug.CursorProbe=1 - the interface cursor's position, sprite "
		         "node and active tile are logged every couple of seconds");
	}
	// The suppressed case is named rather than silently corrected. Menus=world
	// without HudOverlay would deliver the world in stereo with the menu
	// nowhere at all - the eyes are captured before the 2D pass draws, so the
	// overlay is the only route a menu has - and a person who set it would
	// otherwise be looking for a menu that was never going to arrive.
	OBVR_LOG("Config: Render.ShowMenus=%d Menus=%s (videos and loading screens take the "
	         "cinema screen either way)",
	         tracker.showMenus ? 1 : 0,
	         !tracker.menusInWorld ? "cinema"
	                               : (tracker.hudOverlay
	                                      ? "world"
	                                      : "world, but HudOverlay is off, so the cinema "
	                                        "screen is used - the overlay is the only way a "
	                                        "menu reaches the headset"));
	OBVR_LOG("Config: Render.Crosshair=%d InThirdPerson=%d PersistentCache=%d",
	         tracker.crosshair ? 1 : 0, tracker.crosshairInThirdPerson ? 1 : 0,
	         tracker.crosshairPersistentCache ? 1 : 0);
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

bool SaveSetting(const char* section, const char* key, const char* value) {
	if (section == nullptr || key == nullptr || value == nullptr) {
		return false;
	}
	if (section[0] == '\0' || key[0] == '\0') {
		return false;
	}

	// The same file Load and Reload read, found the same way. Writing to a
	// different one would leave the value read from the first and written to
	// the second, which reads as the setting refusing to change.
	char path[512];
	if (!BuildPath("OBVR.ini", path, sizeof(path))) {
		return false;
	}

	return WritePrivateProfileStringA(section, key, value, path) != 0;
}

}  // namespace obvr
