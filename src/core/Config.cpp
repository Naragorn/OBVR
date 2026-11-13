#include "core/Config.h"
#include "core/ConfigKeys.h"
#include "core/ConfigRead.h"
#include "vr/HandSettingsOps.h"

#include <cstdio>
#include <utility>

#include "core/HarnessToken.h"
#include "core/Log.h"
#include "core/ReloadGate.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "render/MenuShade.h"

namespace obvr {
namespace {

Config g_config;

// What the hot reload remembers of the files between two looks at them. See
// ReloadGate.h for why the look is cheap and the read is not.
config::ReloadGate g_reloadGate;

// The button bindings' own file (vr/ButtonBindings.h), found as OBVR.ini is.
constexpr const char* kControlsFileName = "OBVR_Controls.ini";

bool BuildPath(const char* fileName, char* out, UInt32 outSize);

// The files as they are now: the INI at the given path, the test overlay
// beside the plugin, which ReadTestOverlay reads over it, and OBVR_Controls.ini.
config::FileStamp StampFiles(const char* iniPath) {
	config::FileStamp stamp;
	stamp.iniKnown = ReadFileStamp(iniPath, stamp.iniWrite, stamp.iniSize);
	char overlay[512];
	if (platform::BuildPluginPath("OBVR-test.ini", overlay, sizeof(overlay))) {
		// A missing overlay leaves both at zero, which is its state.
		ReadFileStamp(overlay, stamp.overlayWrite, stamp.overlaySize);
	}
	char controls[512];
	if (BuildPath(kControlsFileName, controls, sizeof(controls))) {
		ReadFileStamp(controls, stamp.controlsWrite, stamp.controlsSize);
	}
	return stamp;
}

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

using config_read::EqualsIgnoreCase;
using config_read::ReadText;
using config_read::ReadAnchorIsWorld;
using config_read::ReadLegacyWordBool;
using config_read::ReadFloat;
using config_read::ReadUInt;
using config_read::ReadBool;
using config_read::ReadKeyCode;
using config_read::ReadStereoMode;
using config_read::ReadMenusInWorld;
using config_read::ReadHexColor;

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

// The reads that are not one key into one member: a word parsed into an
// enum, a value clamped, one setting following another. Each a step row of
// the table, at the place it had in the hand-written read.

void ReadTrackerSourceStep(Config& config, const char* path) {
	config.tracker.source = ReadSource(config.tracker.source, path);
}

void ReadStereoStep(Config& config, const char* path) {
	config.tracker.stereo = ReadStereoMode(config.tracker.stereo, path);
}

void ReadWaterModeStep(Config& config, const char* path) {
	const UInt32 waterMode = ReadUInt("Render", "WaterReflectionMode",
		static_cast<UInt32>(config.waterReflectionMode), path);
	if (waterMode <= static_cast<UInt32>(render::WaterReflectionMode::Reprojected))
		config.waterReflectionMode = static_cast<render::WaterReflectionMode>(waterMode);
	if (!config.stableWaterReflections)
		config.waterReflectionMode = render::WaterReflectionMode::Vanilla;
}

void ClampFlatDepthStep(Config& config, const char*) {
	if (!(config.tracker.flatDepthMetres >= 0.0f && config.tracker.flatDepthMetres <= 50.0f)) {
		config.tracker.flatDepthMetres = 2.0f;
	}
}

void ReadDialogPanelSideStep(Config& config, const char* path) {
	vr::DialogPanelSettings& dp = config.dialogPanel;
	char word[16] = {};
	ReadText("Look", "DialogPanelSide", word, sizeof(word), path);
	if (word[0] != 0 && !vr::ParseDialogPanelSide(word, dp.side)) {
		OBVR_LOG("Config: [Look] DialogPanelSide=%s is not centre, right or left - kept %s", word,
		         vr::kDialogPanelSideNames[static_cast<UInt32>(dp.side)]);
	}
}

// AimAtSource writes the player's pitch from the gaze, and with the
// vertical look unblocked the engine builds that pitch into the camera -
// on top of the headset's own, so every look up or down is counted
// twice and a turn of the head tips the view. Seen in a headset on
// 2026-09-07 after a laser click had switched BlockVerticalLook off:
// "hochschauen zu weit hoch, runter zu weit runter, links/rechts dreht
// die Sicht hoch". The two cannot be combined, so the block wins.
// With the headset steering nothing, the mouse and the stick are the only
// look there is, so the vertical look must reach the camera: the block
// goes, whatever AimAtSource says - the headset supplies no pitch then,
// and the pitch AimAtSource writes is the camera's own (Nexus, imoody,
// 2026-10-19: the "Head steers the view" row "doesn't appear to do
// anything - and the mod quickly re-enables the option", which was this
// override).
void SettleVerticalLookBlockStep(Config& config, const char*) {
	if (config.aimAtSource && !config.look.blockVerticalLook && config.tracker.headLook) {
		config.look.blockVerticalLook = true;
		OBVR_LOG("Config: Look.BlockVerticalLook=0 is overridden to 1 because AimAtSource=1 "
		         "writes the pitch the headset already supplies - unblocked, the camera "
		         "would carry it twice");
	}
	if (!config.tracker.headLook && config.look.blockVerticalLook) {
		config.look.blockVerticalLook = false;
		OBVR_LOG("Config: Head.HeadLook=0 - the headset steers nothing, the view is the game's own "
		         "camera under the mouse and the stick; Look.BlockVerticalLook is off with it");
	}
}

// The one stagger and knockdown speed handed to every way of striking
// (vr::ApplyImpact), after the keys are read.
// The headset turning the view only (Nexus, imoody, 2026-11-02: the mouse and
// the stick aim, the headset turns and tilts the camera on top, not the aim):
// seated, with the headset steering the view, the aim follows no gaze, the
// source aim is off and the vertical look is the mouse's - the three that
// would hand the aim to the head. Full VR (the hands aim) leaves them be.
void HeadTurnsViewOnlyStep(Config& config, const char*) {
	if (!config.headTurnsViewOnly || !config.tracker.headLook || config.fullVrMode) {
		return;
	}
	if (config.aimFollowsGaze || config.aimAtSource || config.look.blockVerticalLook) {
		OBVR_LOG("Config: Look.HeadTurnsViewOnly=1 - the headset turns the view only: AimFollowsGaze, AimAtSource "
		         "and BlockVerticalLook are off with it (were %d %d %d)",
		         config.aimFollowsGaze ? 1 : 0, config.aimAtSource ? 1 : 0, config.look.blockVerticalLook ? 1 : 0);
	}
	config.aimFollowsGaze = false;
	config.aimAtSource = false;
	config.look.blockVerticalLook = false;
}

void ApplyImpactStep(Config& config, const char*) { vr::ApplyImpact(config.hands); }

void NormalizePerformanceStep(Config& config, const char*) {
	config.performance = perf::NormalizeSettings(config.performance);
}

void ReadScriptClockThresholdStep(Config& config, const char* path) {
	config.scriptTimeTraceMs = game::ScriptClockThresholdMs(
		ReadFloat("Debug", "ScriptTimeTraceMs", config.scriptTimeTraceMs, path));
}

void ClampNearPlaneStep(Config& config, const char*) {
	if (!(config.body.nearPlaneUnits >= 0.0f)) {
		config.body.nearPlaneUnits = 0.0f;
	}
}

void ClampArmStretchStep(Config& config, const char*) {
	Config::BodySettings& b = config.body;
	if (!(b.armStretch >= 1.0f)) {
		b.armStretch = 1.0f;
	} else if (b.armStretch > 1.5f) {
		b.armStretch = 1.5f;
	}
}

void HandsFollowFullVrStep(Config& config, const char*) {
	config.hands.enabled = config.fullVrMode;
}

void ReadSneakIndicatorStep(Config& config, const char* path) {
	vr::HandSettings& h = config.hands;
	char sneakWord[32] = {};
	ReadText("Hands", "SneakIndicator", sneakWord, sizeof(sneakWord), path);
	if (sneakWord[0] != 0 && !vr::ParseSneakIndicator(sneakWord, h.sneakIndicator)) {
		OBVR_LOG("Config: [Hands] SneakIndicator=%s is not vignette or eye - kept %s", sneakWord,
		         vr::kSneakIndicatorNames[static_cast<UInt32>(h.sneakIndicator)]);
	}
}

void ReachCloseFollowsOpenStep(Config& config, const char*) {
	vr::HandSettings& h = config.hands;
	h.reachOpen.closeMetres = vr::ReachCloseMetresFor(h.reachOpen.openMetres, h.reachOpen.closeMetres);
}

void DoorGripMoveClampStep(Config& config, const char*) {
	config.hands.doorGrip.moveMetres = vr::DoorMoveMetresFor(config.hands.doorGrip.moveMetres);
}

void ReachWalkAwayFollowsCloseStep(Config& config, const char*) {
	vr::HandSettings& h = config.hands;
	h.reachOpen.walkAwayMetres = vr::ReachWalkAwayMetresFor(h.reachOpen.closeMetres, h.reachOpen.walkAwayMetres);
}

void LaserNeedsOneStep(Config& config, const char*) {
	vr::HandSettings& h = config.hands;
	if (!h.laserBeam && !h.laserDot) {
		// The laser needs something to show; the settings menu refuses this
		// (SettingEditRefusal), a hand-edited INI gets the beam back.
		h.laserBeam = true;
		OBVR_LOG("Config: Hands.LaserBeam=0 and LaserDot=0 - the beam is kept on, the laser "
		         "needs one of the two");
	}
}

void KickOnBStep(Config& config, const char*) {
	config.hands.kickOnB = config.hands.kick.enabled;
}

// One [Buttons] key per action into the file's bindings: absent leaves what
// is there, a value that does not parse is said and leaves it too. An inline
// comment (";" on, which GetPrivateProfileString keeps) is not part of it.
void ReadButtonKeys(vr::ControlBindingFile& file, const char* path, const char* fileName) {
	for (UInt32 i = 0; i < vr::kControlActionCount; ++i) {
		const char* key = vr::ControlActionName(static_cast<vr::ControlAction>(i));
		char text[160];
		GetPrivateProfileStringA("Buttons", key, "\x01", text, sizeof(text), path);
		if (text[0] == '\x01') {
			continue;
		}
		for (char* c = text; *c != '\0'; ++c) {
			if (*c == ';') {
				*c = '\0';
				break;
			}
		}
		vr::ControlSource sources[vr::kSourcesPerAction];
		if (!vr::ParseControlSources(text, sources)) {
			OBVR_LOG("Config: %s [Buttons] %s=%s not understood - the default kept (names: RightA, RightAHold, "
			         "RightB, RightBHold, LeftA, LeftAHold, LeftB, LeftBHold, RightStickUp, RightStickDown, "
			         "RightStickClick, LeftStickClick, LeftTrackpad, None; three at most)",
			         fileName, key, text);
			continue;
		}
		file.set[i] = true;
		for (UInt32 j = 0; j < vr::kSourcesPerAction; ++j) {
			file.sources[i][j] = sources[j];
		}
	}
}

// The button bindings: OBVR_Controls.ini, read whole on OBVR.ini's pass; the
// test overlay's [Buttons] over it on the overlay's pass (a harness scenario's
// "ini Buttons Jump=RightB"). What came out is said, with every input the
// laying-over moved (vr::ResolveControlBindings).
void ReadButtonBindingsStep(Config& config, const char* path) {
	char overlay[512];
	const bool overlayPass = platform::BuildPluginPath("OBVR-test.ini", overlay, sizeof(overlay)) &&
	                         EqualsIgnoreCase(overlay, path);
	vr::ControlBindingFile& file = config.hands.buttons;
	if (overlayPass) {
		ReadButtonKeys(file, path, "OBVR-test.ini");
	} else {
		file = vr::ControlBindingFile{};
		char controls[512];
		if (BuildPath(kControlsFileName, controls, sizeof(controls)) &&
		    GetFileAttributesA(controls) != INVALID_FILE_ATTRIBUTES) {
			ReadButtonKeys(file, controls, kControlsFileName);
		}
	}
	vr::BindingNotes notes;
	const vr::ControlBindings resolved = vr::ResolveControlBindings(file, config.hands.kickOnB, &notes);
	if (resolved == vr::DefaultControlBindings(config.hands.kickOnB) && notes.count == 0) {
		return;
	}
	char line[512];
	UInt32 at = 0;
	for (UInt32 i = 0; i < vr::kControlActionCount; ++i) {
		at += static_cast<UInt32>(std::snprintf(line + at, sizeof(line) - at, "%s%s=",
		                                         i == 0 ? "" : " ", vr::ControlActionName(static_cast<vr::ControlAction>(i))));
		for (UInt32 j = 0; j < vr::kSourcesPerAction && at < sizeof(line); ++j) {
			const vr::ControlSource s = resolved.sources[i][j];
			if (s == vr::ControlSource::None && j > 0) {
				break;
			}
			at += static_cast<UInt32>(
				std::snprintf(line + at, sizeof(line) - at, "%s%s", j == 0 ? "" : ",", vr::ControlSourceName(s)));
		}
		if (at >= sizeof(line)) {
			at = sizeof(line) - 1;
			break;
		}
	}
	OBVR_LOG("Config: buttons %s", line);
	for (UInt32 n = 0; n < notes.count; ++n) {
		const vr::BindingNote& note = notes.notes[n];
		switch (note.kind) {
		case vr::BindingNoteKind::Taken:
			OBVR_LOG("Config: buttons - %s is %s's already, not %s's too", vr::ControlSourceName(note.source),
			         vr::ControlActionName(note.owner), vr::ControlActionName(note.action));
			break;
		case vr::BindingNoteKind::Moved:
			OBVR_LOG("Config: buttons - %s went from %s to %s", vr::ControlSourceName(note.source),
			         vr::ControlActionName(note.action), vr::ControlActionName(note.owner));
			break;
		case vr::BindingNoteKind::KickOff:
			OBVR_LOG("Config: buttons - Kick=%s unused: [Hands] Kick=0", vr::ControlSourceName(note.source));
			break;
		}
	}
}

void ArrowWithGripStep(Config& config, const char* path) {
	vr::HandSettings& h = config.hands;
	h.archery.takeWithTrigger = !ReadBool("Hands", "ArrowWithGrip", !h.archery.takeWithTrigger, path);
}

void ReadHandHudElementsStep(Config& config, const char* path) {
	vr::HandHudSettings& hh = config.hands.handHud;
	for (UInt32 e = 0; e < vr::kHudElementCount; ++e) {
		char key[40];
		char word[16] = {};
		std::snprintf(key, sizeof(key), "%sPlace", vr::kHudElementKeys[e]);
		ReadText("HandHud", key, word, sizeof(word), path);
		if (word[0] != 0 && !vr::ParseHudPlace(word, hh.element[e].place)) {
			OBVR_LOG("Config: [HandHud] %s=%s is not view, left, right, sky or off - kept %s", key, word,
			         vr::kHudPlaceNames[static_cast<UInt32>(hh.element[e].place)]);
		}
		std::snprintf(key, sizeof(key), "%sOpacity", vr::kHudElementKeys[e]);
		hh.element[e].opacity = ReadFloat("HandHud", key, hh.element[e].opacity, path);
		std::snprintf(key, sizeof(key), "%sSize", vr::kHudElementKeys[e]);
		hh.element[e].size = ReadFloat("HandHud", key, hh.element[e].size, path);
	}
}

void ReadWalkDirectionStep(Config& config, const char* path) {
	vr::HandSettings& h = config.hands;
	char word[32] = "";
	ReadText("Hands", "WalkDirection", word, sizeof(word), path);
	if (word[0] != 0 && !vr::ParseWalkDirection(word, h.walkDirection)) {
		OBVR_LOG("Config: [Hands] WalkDirection=%s is not head, right, left or blend - kept %s", word,
		         vr::kWalkDirectionNames[static_cast<UInt32>(h.walkDirection)]);
	}
}

void ReadCrosshairPlaceStep(Config& config, const char* path) {
	vr::HandSettings& h = config.hands;
	char word[32] = "";
	ReadText("Hands", "CrosshairPlace", word, sizeof(word), path);
	if (word[0] != 0 && !vr::ParseCrosshairPlace(word, h.crosshairPlace)) {
		OBVR_LOG("Config: [Hands] CrosshairPlace=%s is not laser, target or view - kept %s", word,
		         vr::kCrosshairPlaceNames[static_cast<UInt32>(h.crosshairPlace)]);
	}
}

void TorchReleaseFollowsWeaponStep(Config& config, const char*) {
	vr::HandSettings& h = config.hands;
	h.torchRelease.releaseSeconds = h.weaponRelease.releaseSeconds;
	h.torchRelease.throwStrength = h.weaponRelease.throwStrength;
}

void TorchSpinFollowsWeaponStep(Config& config, const char*) {
	config.hands.torchRelease.throwSpinStrength = config.hands.weaponRelease.throwSpinStrength;
}

void ReadHolsterSidesStep(Config& config, const char* path) {
	vr::HolsterSettings& hs = config.hands.holster;
	struct SideKey {
		const char* key;
		vr::HolsterSide* side;
	};
	const SideKey sides[] = {{"HolsterOneHandSide", &hs.oneHandSide},
	                         {"HolsterTwoHandSide", &hs.twoHandSide},
	                         {"HolsterBowSide", &hs.bowSide}};
	for (const SideKey& entry : sides) {
		char word[16] = {};
		ReadText("Hands", entry.key, word, sizeof(word), path);
		if (word[0] != 0 && !vr::ParseHolsterSide(word, *entry.side)) {
			OBVR_LOG("Config: [Hands] %s=%s is not auto, right or left - kept %s", entry.key, word,
			         vr::kHolsterSideNames[static_cast<UInt32>(*entry.side)]);
		}
	}
}

void ReadTeleportSoundStep(Config& config, const char* path) {
	vr::TeleportSettings& tp = config.hands.teleport;
	char word[32] = "";
	ReadText("Locomotion", "TeleportSound", word, sizeof(word), path);
	if (word[0] != 0 && !vr::ParseTeleportSound(word, tp.sound)) {
		OBVR_LOG("Config: [Locomotion] TeleportSound=%s is not dodge, dodgeback, landing or swish - kept %s",
		         word, vr::kTeleportSoundNames[static_cast<UInt32>(tp.sound)]);
	}
}

// A row: the key, its kind and the member it lands in; the member's
// address and size come from the path alone.
#define OBVR_KEY(kind, section, key, member)                                                        \
	{                                                                                               \
		section, key, obvr::config::IniKind::kind, [](Config& c) -> void* { return &c.member; },         \
			static_cast<UInt32>(sizeof(std::declval<Config&>().member)), nullptr, nullptr, nullptr, "" \
	}
#define OBVR_LEGACY_WORDS(section, key, member, falseWord, trueWord)                                    \
	{                                                                                                   \
		section, key, obvr::config::IniKind::LegacyWordBool, [](Config& c) -> void* { return &c.member; },   \
			static_cast<UInt32>(sizeof(std::declval<Config&>().member)), falseWord, trueWord, nullptr, "" \
	}
// A step row: the block's name, and the keys it reads of its own (space
// separated, in `section`; "<Element>" stands for each HUD element's key).
#define OBVR_STEP(name, section, reads) \
	{ section, #name, obvr::config::IniKind::Step, nullptr, 0, nullptr, nullptr, &name##Step, reads }

// Every key the INI has, in the order it is read (core/ConfigKeys.h).
const obvr::config::IniKey kConfigKeys[] = {
	OBVR_STEP(ReadTrackerSource, "Head", "Source"),
	OBVR_KEY(Float, "Head", "FixedPitch", tracker.fixedPitch),
	OBVR_KEY(Float, "Head", "FixedRoll", tracker.fixedRoll),
	OBVR_KEY(Float, "Head", "FixedYaw", tracker.fixedYaw),
	OBVR_KEY(Float, "Head", "SimulatedYawDegrees", tracker.simulatedYawAmplitude),
	OBVR_KEY(Float, "Head", "SimulatedPitchDegrees", tracker.simulatedPitchAmplitude),
	OBVR_KEY(UInt, "Head", "SimulatedPeriodFrames", tracker.simulatedPeriodFrames),
	OBVR_KEY(KeyCode, "Head", "RecenterKey", recenterKey),
	OBVR_KEY(Bool, "Head", "PositionalTracking", tracker.positionalTracking),
	OBVR_KEY(Bool, "Head", "RealHeight", tracker.realHeight),
	OBVR_KEY(Bool, "Head", "HeadLook", tracker.headLook),
	OBVR_KEY(Float, "Head", "UnitsPerMetre", tracker.unitsPerMetre),
	OBVR_KEY(Float, "Head", "HeadMovementScale", tracker.movementScale),
	OBVR_KEY(Float, "Head", "MaxLeanUnits", tracker.maxOffsetUnits),
	OBVR_KEY(Bool, "Render", "Enabled", tracker.renderToHeadset),
	OBVR_KEY(Bool, "Render", "GameFrame", tracker.submitGameFrame),
	OBVR_STEP(ReadStereo, "Render", "Stereo"),
	OBVR_KEY(Float, "Render", "EyeSeparationScale", tracker.eyeSeparationScale),
	OBVR_KEY(Float, "Render", "GameFovDegrees", tracker.gameFovDegrees),
	OBVR_KEY(Bool, "Render", "GameFovIsFor4x3", tracker.gameFovIsFor4x3),
	OBVR_KEY(Bool, "Render", "SubmitAtFrameEnd", tracker.submitAtFrameEnd),
	OBVR_KEY(Bool, "Render", "ShowMenus", tracker.showMenus),
	OBVR_KEY(Float, "Render", "GameFovOverride", tracker.gameFovOverride),
	OBVR_KEY(Float, "Render", "MenuScale", tracker.menuScale),
	OBVR_KEY(Float, "Render", "MenuAspect", tracker.menuAspect),
	OBVR_KEY(Bool, "Render", "SetGameResolution", tracker.setRenderSize),
	OBVR_KEY(Bool, "Render", "MonitorVsync", tracker.monitorVsync),
	OBVR_KEY(Bool, "Render", "SubmitPastVsync", tracker.submitPastVsync),
	OBVR_KEY(Bool, "Render", "PreciseFrameClock", tracker.preciseFrameClock),
	OBVR_KEY(Bool, "Render", "FastStreamCopy", tracker.fastStreamCopy),
	OBVR_KEY(Bool, "Render", "StreamReuse", tracker.streamReuse),
	OBVR_KEY(UInt, "Render", "StreamRingBytes", tracker.streamRingBytes),
	OBVR_KEY(UInt, "Render", "GameResolutionWidth", tracker.renderWidth),
	OBVR_KEY(UInt, "Render", "GameResolutionHeight", tracker.renderHeight),
	OBVR_KEY(Bool, "Render", "MatchHeadsetFov", tracker.matchHeadsetFov),
	OBVR_KEY(Bool, "Render", "UiFollowsFrameSize", tracker.uiFollowsFrameSize),
	OBVR_KEY(Bool, "Render", "HudOverlay", tracker.hudOverlay),
	OBVR_KEY(Bool, "Render", "HudBetweenPasses", tracker.hudBetweenPasses),
	OBVR_KEY(Bool, "Render", "StableLeafBillboards", stableLeafBillboards),
	OBVR_KEY(Bool, "Render", "KeepBillboardTime", keepBillboardTime),
	OBVR_KEY(Bool, "Render", "FirstPersonParticlesAfterPins", firstPersonParticlesAfterPins),
	OBVR_KEY(Bool, "Render", "GrassAllAround", grassAllAround),
	OBVR_KEY(Float, "Render", "GrassDistance", grassDistance),
	OBVR_KEY(Float, "Render", "GrassDensity", grassDensity),
	OBVR_KEY(Bool, "Render", "StableDistantTrees", stableDistantTrees),
	OBVR_KEY(Bool, "Render", "LoadingStage", loadingStage),
	OBVR_KEY(Bool, "Render", "StableWaterReflections", stableWaterReflections),
	OBVR_KEY(Bool, "Render", "SinglePassStereo", singlePassStereo),
	OBVR_KEY(Bool, "Render", "DynamicResolution", dynamicResolution),
	OBVR_KEY(Float, "Render", "DynamicResolutionMin", dynamicResolutionMin),
	OBVR_KEY(Float, "Render", "DynamicResolutionTarget", dynamicResolutionTarget),
	OBVR_KEY(Bool, "Render", "ShareActorShadows", shareActorShadows),
	OBVR_KEY(Bool, "Render", "WarmOverlays", warmOverlays),
	OBVR_STEP(ReadWaterMode, "Render", "WaterReflectionMode"),
	OBVR_KEY(Bool, "Render", "WaterCaptureFollowsHead", waterCaptureFollowsHead),
	OBVR_KEY(Float, "Render", "WaterCaptureMargin", waterCaptureMargin),
	OBVR_KEY(UInt, "Render", "WaterReflectionMapSize", waterReflectionMapSize),
	OBVR_KEY(AnchorIsWorld, "Render", "HudAnchor", tracker.hudAnchorWorld),
	OBVR_KEY(Float, "Render", "FlatFollowDegrees", tracker.flatFollowDegrees),
	OBVR_KEY(Float, "Render", "FlatDepthMetres", tracker.flatDepthMetres),
	OBVR_STEP(ClampFlatDepth, "", ""),
	OBVR_KEY(Bool, "Render", "MenuShade", tracker.menuShade),
	OBVR_KEY(Bool, "Render", "LoadingBlack", tracker.loadingBlack),
	OBVR_KEY(HexColor, "Render", "MenuShadeColor", tracker.menuShadeColorRgb),
	OBVR_KEY(Float, "Render", "MenuShadeStrength", tracker.menuShadeStrength),
	OBVR_KEY(Bool, "Render", "MenuSingleBorder", tracker.menuSingleBorder),
	OBVR_KEY(Bool, "Look", "DialogZoom", dialogZoom),
	OBVR_KEY(Bool, "Look", "DialogFirstPerson", dialogFirstPerson),
	OBVR_KEY(Bool, "Look", "DialogRecentre", dialogPanel.recentre),
	OBVR_STEP(ReadDialogPanelSide, "Look", "DialogPanelSide"),
	OBVR_KEY(Float, "Look", "DialogPanelSideDegrees", dialogPanel.sideDegrees),
	OBVR_KEY(Float, "Look", "DialogPanelScale", dialogPanel.scale),
	OBVR_KEY(Bool, "Look", "ContainerInWorld", containerPanel.inWorld),
	OBVR_KEY(Float, "Look", "ContainerPanelScale", containerPanel.scale),
	OBVR_KEY(Float, "Look", "ContainerPanelRaiseMetres", containerPanel.raiseMetres),
	OBVR_KEY(MenusInWorld, "Render", "Menus", tracker.menusInWorld),
	OBVR_KEY(Bool, "Render", "LiveMenuBackground", tracker.liveMenuBackground),
	OBVR_KEY(Float, "Render", "HudDistanceMetres", tracker.hudDistanceMetres),
	OBVR_KEY(Float, "Render", "HudWidthMetres", tracker.hudWidthMetres),
	OBVR_KEY(Bool, "Render", "MenuStandIn", tracker.menuStandIn),
	OBVR_KEY(Bool, "Render", "Crosshair", tracker.crosshair),
	OBVR_LEGACY_WORDS("Render", "CrosshairDynamic", tracker.crosshairDynamic, "fixed", "follows"),
	OBVR_KEY(Float, "Render", "CrosshairDistanceMetres", tracker.crosshairDistanceMetres),
	OBVR_KEY(Float, "Render", "CrosshairDepthSpeed", tracker.crosshairDepthSpeed),
	OBVR_KEY(Bool, "Render", "CrosshairProbe", tracker.crosshairProbe),
	OBVR_KEY(Float, "Render", "CrosshairSizeAtOneMetre", tracker.crosshairSizeAtOneMetre),
	OBVR_KEY(Float, "Render", "CrosshairSourceShare", tracker.crosshairSourceShare),
	OBVR_LEGACY_WORDS("Render", "CrosshairOnlyWhenNeeded", tracker.crosshairOnlyWhenNeeded, "always", "when needed"),
	OBVR_KEY(Bool, "Render", "CrosshairInThirdPerson", tracker.crosshairInThirdPerson),
	OBVR_KEY(Bool, "Render", "CrosshairTooltips1stPerson", tracker.crosshairTooltipsFirstPerson),
	OBVR_KEY(Bool, "Render", "CrosshairTooltips3rdPerson", tracker.crosshairTooltipsThirdPerson),
	OBVR_KEY(Bool, "Render", "CrosshairTooltipsAboveName", tracker.crosshairTooltipsAboveName),
	OBVR_KEY(Bool, "Render", "CrosshairPersistentCache", tracker.crosshairPersistentCache),
	OBVR_KEY(Bool, "Render", "MirrorMenusToMonitor", tracker.mirrorMenusToMonitor),
	OBVR_KEY(Bool, "Render", "MonitorMenusOnly", tracker.monitorMenusOnly),
	OBVR_KEY(Bool, "Render", "MenuModelFit", tracker.menuModelFit),
	OBVR_KEY(Bool, "Render", "UnpausedMenus", tracker.unpausedMenus),
	OBVR_LEGACY_WORDS("Render", "CrosshairOnlyWhenNeeded3rdPerson", tracker.crosshairOnlyWhenNeededThirdPerson, "always", "when needed"),
	OBVR_KEY(Bool, "Look", "BlockVerticalLook", look.blockVerticalLook),
	OBVR_KEY(Bool, "Look", "AimFollowsGaze", aimFollowsGaze),
	OBVR_KEY(Bool, "Look", "AimInThirdPerson", aimInThirdPerson),
	OBVR_KEY(Bool, "Look", "AimAtSource", aimAtSource),
	OBVR_KEY(Bool, "Look", "HeadTurnsViewOnly", headTurnsViewOnly),
	OBVR_STEP(SettleVerticalLookBlock, "", ""),
	OBVR_KEY(Float, "Look", "ThirdPersonAimVisualPercent", thirdPersonAimVisualPercent),
	OBVR_KEY(Bool, "Look", "ThirdPersonBodyFollowsGazeUnarmed", thirdPersonBodyFollowsGazeUnarmed),
	OBVR_KEY(Bool, "Look", "ThirdPersonHeadFollowsGaze", thirdPersonHeadFollowsGaze),
	OBVR_KEY(KeyCode, "Look", "AimAttackKey", aimAttackKey),
	OBVR_KEY(Float, "Look", "AimTurnSpeed", aimTurnSpeed),
	OBVR_KEY(Bool, "Look", "AimReturnOnRelease", aimReturnOnRelease),
	OBVR_LEGACY_WORDS("Look", "AimTurnOnShotOnly", aimTurnOnShotOnly, "while aiming", "for the shot"),
	OBVR_KEY(Bool, "Look", "AimShotTrace", aimShotTrace),
	OBVR_KEY(Bool, "Debug", "LoadTrace", loadTrace),
	OBVR_KEY(UInt, "Debug", "LockpickTrace", lockpickTraceLines),
	OBVR_KEY(Bool, "Look", "AimWeaponFollowsGaze", aimWeaponFollowsGaze),
	OBVR_KEY(Bool, "Look", "AimCastFollowsGaze", aimCastFollowsGaze),
	OBVR_KEY(KeyCode, "Look", "AimCastKey", aimCastKey),
	OBVR_KEY(Float, "Look", "AimCastHoldSeconds", aimCastHoldSeconds),
	OBVR_KEY(Bool, "Look", "AimCastTrace", aimCastTrace),
	OBVR_KEY(Bool, "Look", "AimCastAtSpawn", aimCastAtSpawn),
	OBVR_KEY(Float, "Look", "AimCastTurnAfterSeconds", aimCastTurnAfterSeconds),
	OBVR_KEY(Float, "Look", "AimCastTurnMarginSeconds", aimCastTurnMarginSeconds),
	OBVR_KEY(Float, "Look", "AimCastArmLimitSeconds", aimCastArmLimitSeconds),
	OBVR_KEY(KeyCode, "SettingsMenu", "Key", settingsMenuKey),
	OBVR_KEY(Float, "SettingsMenu", "DistanceMetres", settingsMenuDistanceMetres),
	OBVR_KEY(Float, "SettingsMenu", "WidthMetres", settingsMenuWidthMetres),
	OBVR_KEY(AnchorIsWorld, "SettingsMenu", "Anchor", settingsMenuInWorld),
	// VerticalLookRange was one value for both directions until it turned out
	// that a range sized for looking up stops at the hips on the way down. It
	// is still read, as the fallback for both halves, so that an INI written
	// before the split keeps meaning what it said rather than quietly
	// reverting to the defaults. Each half falls back on its own current
	// value, so a file without either key (OBVR-test.ini, read over OBVR.ini)
	// leaves both as they were.
	OBVR_KEY(Float, "Look", "VerticalLookRange", look.verticalLookUpRange),
	OBVR_KEY(Float, "Look", "VerticalLookRange", look.verticalLookDownRange),
	OBVR_KEY(Float, "Look", "VerticalLookUpRange", look.verticalLookUpRange),
	OBVR_KEY(Float, "Look", "VerticalLookDownRange", look.verticalLookDownRange),
	OBVR_KEY(Bool, "Look", "SmoothVerticalLook", look.smoothVerticalLook),
	OBVR_KEY(Float, "Look", "VerticalLookSpeed", look.verticalLookSpeed),
	OBVR_KEY(Bool, "Look", "SnapTurning", look.snapTurning),
	OBVR_KEY(Float, "Look", "SnapTurnAngle", look.snapTurnAngle),
	OBVR_KEY(Bool, "Look", "SnapTurnInstant", look.snapTurnInstant),
	OBVR_KEY(Float, "Look", "SnapTurnSpeed", look.snapTurnSpeed),
	OBVR_KEY(Float, "Look", "SnapTurnVignetteStrength", look.snapTurnVignetteStrength),
	OBVR_KEY(Float, "Look", "SnapTurnVignetteRadius", look.snapTurnVignetteRadius),
	OBVR_KEY(Bool, "Look", "SnapTurnVignette", look.snapTurnVignette),
	OBVR_KEY(Bool, "Look", "FirstPersonAnimations", look.firstPersonAnimations),
	OBVR_KEY(Bool, "Look", "AnimationVignette", look.animationVignette),
	OBVR_KEY(Float, "Look", "AnimationVignetteStrength", look.animationVignetteStrength),
	OBVR_KEY(Bool, "Look", "DeathViewStill", look.deathViewStill),
	OBVR_KEY(Bool, "Look", "DeathGrey", look.deathGrey),
	OBVR_KEY(Bool, "Look", "DeathMenuTint", look.deathMenuTint),
	OBVR_KEY(Bool, "Look", "DeathBodyView", look.deathBodyView),
	OBVR_KEY(Float, "Look", "DeathViewBackMetres", look.deathViewBackMetres),
	OBVR_KEY(Float, "Look", "DeathBodyAheadMetres", look.deathBodyAheadMetres),
	OBVR_KEY(Float, "Look", "DeathBodyUpMetres", look.deathBodyUpMetres),
	OBVR_KEY(Bool, "Look", "HideHudWhenDead", look.hideHudWhenDead),
	OBVR_KEY(Bool, "Look", "FadesInView", look.fadesInView),
	OBVR_KEY(Bool, "Look", "NoPlayerStagger", look.noPlayerStagger),
	OBVR_KEY(Bool, "Look", "NoPlayerKnockdown", look.noPlayerKnockdown),
	OBVR_KEY(Bool, "Look", "Exhaustion", look.exhaustion.enabled),
	OBVR_KEY(Float, "Look", "ExhaustionBelowPercent", look.exhaustion.belowPercent),
	OBVR_KEY(Float, "Look", "ExhaustionAbovePercent", look.exhaustion.abovePercent),
	OBVR_KEY(Float, "Look", "ExhaustionWeight", look.exhaustion.weight),
	OBVR_KEY(Bool, "Look", "ExhaustionSounds", look.exhaustion.sounds),
	OBVR_KEY(Bool, "Look", "NoHitBlur", look.noHitBlur),
	OBVR_KEY(Bool, "Look", "AutoVanityCamera", look.autoVanityCamera),
	OBVR_KEY(Float, "Look", "HorseBob", look.horseBob),
	OBVR_KEY(Bool, "Look", "ChargenView", look.chargenView),
	OBVR_KEY(Bool, "Look", "ChargenFacesHead", look.chargenFacesHead),
	OBVR_KEY(Float, "Look", "ChargenDistanceMetres", look.chargenDistanceMetres),
	OBVR_KEY(Bool, "Look", "BlockFacesView", look.blockFacesView),
	OBVR_KEY(Float, "Look", "SnapTurnDeadZone", look.snapTurnDeadZone),
	OBVR_KEY(Bool, "Look", "SmoothTurning", look.smoothTurning),
	OBVR_KEY(Float, "Look", "TurnSpeed", look.turnSpeed),
	OBVR_KEY(UInt, "Debug", "LogEveryFrames", logEveryFrames),
	OBVR_KEY(UInt, "Debug", "ReloadEveryFrames", reloadEveryFrames),
	OBVR_KEY(Bool, "Performance", "Enabled", performance.enabled),
	OBVR_KEY(Bool, "Performance", "GpuTiming", performance.gpuTiming),
	OBVR_KEY(UInt, "Performance", "CaptureSeconds", performance.captureSeconds),
	OBVR_KEY(UInt, "Performance", "MaxRecords", performance.maxRecords),
	OBVR_KEY(UInt, "Performance", "StartAfterSeconds", performance.startAfterSeconds),
	OBVR_KEY(Bool, "Performance", "Sampler", performance.sampler),
	OBVR_KEY(Bool, "Performance", "CallClock", performance.callClock),
	OBVR_STEP(NormalizePerformance, "", ""),
	OBVR_KEY(UInt, "Debug", "DualPassProbe", dualPassProbe),
	OBVR_KEY(Bool, "Debug", "DualContentSums", dualContentSums),
	OBVR_KEY(Bool, "Debug", "SwapEyeOrder", swapEyeOrder),
	OBVR_KEY(Bool, "Debug", "HudProbe", hudProbe),
	OBVR_KEY(Bool, "Debug", "PoolWriteFingerprints", poolWriteFingerprints),
	OBVR_KEY(Bool, "Debug", "FrameClockTrace", frameClockTrace),
	OBVR_KEY(Bool, "Debug", "ScriptTimeTrace", scriptTimeTrace),
	OBVR_KEY(Float, "Debug", "TorchTakeBackSeconds", torchTakeBackSeconds),
	OBVR_KEY(Bool, "Debug", "DropTurnAsHeld", dropTurnAsHeld),
	OBVR_STEP(ReadScriptClockThreshold, "Debug", "ScriptTimeTraceMs"),
	OBVR_KEY(Bool, "Debug", "StreamCopyCheck", streamCopyCheck),
	OBVR_KEY(Bool, "Debug", "StreamReuseCheck", streamReuseCheck),
	OBVR_KEY(Bool, "Debug", "HudTileProbe", hudTileProbe),
	OBVR_KEY(Bool, "Debug", "EyeDump", eyeDump),
	OBVR_KEY(UInt, "Debug", "EyeDumpFrames", eyeDumpFrames),
	OBVR_KEY(UInt, "Debug", "EyeDumpDivisor", eyeDumpDivisor),
	OBVR_KEY(Bool, "Debug", "LayerDump", layerDump),
	OBVR_KEY(Bool, "Debug", "WaterCaptureTrace", waterCaptureTrace),
	OBVR_KEY(Bool, "Debug", "ShoveTrace", shoveTrace),
	OBVR_KEY(Bool, "Debug", "AimProbe", aimProbe),
	OBVR_KEY(Bool, "Debug", "ThirdPersonProbe", thirdPersonProbe),
	OBVR_KEY(Bool, "Debug", "FirstPersonTreeProbe", firstPersonTreeProbe),
	OBVR_KEY(Bool, "Debug", "FlameProbe", flameProbe),
	OBVR_KEY(Bool, "Debug", "HorseTurnProbe", horseTurnProbe),
	OBVR_KEY(Bool, "Debug", "MenuWorldProbe", menuWorldProbe),
	OBVR_KEY(Bool, "Debug", "NativeMenuLifecycleProbe", nativeMenuLifecycleProbe),
	OBVR_KEY(Bool, "Debug", "D3D9ExProbe", d3d9ExProbe),
	OBVR_KEY(Bool, "Debug", "VRTestSuite", vrTestSuite),
	OBVR_KEY(Bool, "Debug", "VRTestWaterOnly", vrTestWaterOnly),
	OBVR_KEY(Bool, "Debug", "VRTestWaterCoverageDiagnostic", vrTestWaterCoverageDiagnostic),
	OBVR_KEY(Float, "Debug", "VRTestWaterPitch", vrTestWaterPitch),
	OBVR_KEY(Text, "Debug", "HandScript", handScript),
	OBVR_KEY(Float, "Debug", "PoseRecordSeconds", poseRecordSeconds),
	OBVR_KEY(Bool, "Debug", "HandScriptConsole", handScriptConsole),
	OBVR_KEY(Text, "Debug", "HandScriptLoad", handScriptLoad),
	OBVR_KEY(Bool, "Hands", "Enabled", fullVrMode),
	OBVR_STEP(HeadTurnsViewOnly, "", ""),
	OBVR_KEY(Bool, "Onboarding", "ShowAtStart", onboardingShowAtStart),
	OBVR_KEY(Bool, "Onboarding", "StartGuide", onboardingStartGuide),
	OBVR_KEY(Bool, "Onboarding", "GuideDome", guideDome),
	OBVR_KEY(Float, "Onboarding", "GuideMusicDelay", guideMusicDelay),
	OBVR_KEY(Float, "Onboarding", "GuideMusicFade", guideMusicFade),
	OBVR_KEY(Float, "Onboarding", "GuideMusicVolume", guideMusicVolume),
	OBVR_KEY(Text, "Onboarding", "GuideMusicFile", guideMusicFile),
	OBVR_KEY(Float, "Onboarding", "GuideMusicStart", guideMusicStart),
	OBVR_KEY(Float, "Onboarding", "GuideMusicEnd", guideMusicEnd),
	OBVR_KEY(Bool, "Onboarding", "NativePrototype", nativeOnboardingPrototype),
	OBVR_KEY(Bool, "Updates", "CheckForUpdates", checkForUpdates),
	OBVR_KEY(Bool, "Debug", "ForceUpdateNotice", forceUpdateNotice),
	OBVR_KEY(Bool, "SelfCheck", "ShowPanel", selfCheckPanel),
	OBVR_KEY(Bool, "Debug", "ForceSelfCheckProblem", forceSelfCheckProblem),
	OBVR_KEY(Bool, "Body", "Visible", body.visible),
	OBVR_KEY(Bool, "Body", "HideHead", body.hideHead),
	OBVR_KEY(Bool, "Body", "CullHiddenHead", body.cullHiddenHead),
	OBVR_KEY(Bool, "Body", "HideArms", body.hideArms),
	OBVR_KEY(Float, "Body", "EyeForward", body.eyeForward),
	OBVR_KEY(Float, "Body", "EyeUp", body.eyeUp),
	OBVR_KEY(Float, "Body", "NearPlane", body.nearPlaneUnits),
	OBVR_STEP(ClampNearPlane, "", ""),
	OBVR_KEY(Bool, "Body", "PovSwitchPatch", body.povSwitchPatch),
	OBVR_KEY(Bool, "Debug", "BodyTreeProbe", body.treeProbe),
	OBVR_KEY(UInt, "Debug", "BodyTrace", body.traceFrames),
	OBVR_KEY(Bool, "Debug", "BodyView", body.viewCheck),
	OBVR_KEY(Bool, "Debug", "WornShapes", body.wornShapes),
	OBVR_KEY(Bool, "Debug", "WearCheck", body.wearCheck),
	OBVR_KEY(Bool, "Body", "IK", body.ik),
	OBVR_KEY(Bool, "Body", "IKArms", body.ikArms),
	OBVR_KEY(Bool, "Body", "IKTorso", body.ikTorso),
	OBVR_KEY(Bool, "Body", "IKLegs", body.ikLegs),
	OBVR_KEY(Bool, "Body", "IKLean", body.ikLean),
	OBVR_KEY(Bool, "Body", "PlayerHeadTracking", body.playerHeadTracking),
	OBVR_KEY(Float, "Body", "TorsoFollowDegrees", body.torsoFollowDegrees),
	OBVR_KEY(Float, "Body", "TorsoFollowSeconds", body.torsoFollowSeconds),
	OBVR_KEY(Bool, "Body", "SoftPush", body.softPush),
	OBVR_KEY(Bool, "Body", "KeepInView", body.keepInView),
	OBVR_KEY(Float, "Body", "SoftPushReach", body.softPushReachUnits),
	OBVR_KEY(Float, "Body", "SoftPushMost", body.softPushMostUnits),
	OBVR_KEY(Float, "Body", "ArmStretch", body.armStretch),
	OBVR_KEY(Bool, "Body", "IKElbowSwing", body.ikElbowSwing),
	OBVR_KEY(Float, "Body", "IKStanceSpeed", body.ikStanceSpeed),
	OBVR_STEP(ClampArmStretch, "", ""),
	OBVR_STEP(HandsFollowFullVr, "", ""),
	OBVR_KEY(Float, "Hands", "BlockMinUp", hands.gestures.blockMinUp),
	OBVR_KEY(Float, "Hands", "BlockMinForward", hands.gestures.blockMinForward),
	OBVR_KEY(Float, "Hands", "ReachBackMaxForward", hands.gestures.reachBackMaxForward),
	OBVR_KEY(Float, "Hands", "ReachBackMinUp", hands.gestures.reachBackMinUp),
	OBVR_KEY(Float, "Hands", "SwingLight", hands.gestures.swingLight),
	OBVR_KEY(Float, "Hands", "SwingMinMetres", hands.gestures.swingMinMetres),
	OBVR_KEY(Float, "Hands", "PowerSwingMetres", hands.gestures.powerSwingMetres),
	OBVR_KEY(Float, "Hands", "PowerThrustMetres", hands.gestures.powerThrustMetres),
	OBVR_KEY(Float, "Hands", "HeavyHoldSeconds", hands.gestures.heavyHoldSeconds),
	OBVR_KEY(Bool, "Hands", "BowNeedsReachBack", hands.gestures.bowNeedsReachBack),
	OBVR_KEY(Bool, "Hands", "MotionHits", hands.motionHits),
	OBVR_KEY(Bool, "Hands", "ControllerMenus", hands.controllerMenus),
	OBVR_KEY(Bool, "Hands", "GamepadLayout", hands.gamepadLayout),
	OBVR_KEY(Bool, "Hands", "LaserBeam", hands.laserBeam),
	OBVR_KEY(Float, "Hands", "LaserPitchDegrees", hands.laserPitchDegrees),
	OBVR_KEY(Float, "Hands", "LaserYawDegrees", hands.laserYawDegrees),
	OBVR_KEY(Float, "Hands", "LaserOriginMetres", hands.laserOriginMetres),
	OBVR_KEY(Float, "Hands", "LaserRightMetres", hands.laserOffsetRightMetres),
	OBVR_KEY(Float, "Hands", "LaserUpMetres", hands.laserOffsetUpMetres),
	OBVR_KEY(Bool, "Hands", "LaserDragScroll", hands.laserDragScroll),
	OBVR_KEY(Float, "Hands", "LaserStickyPixels", hands.laserStickyPixels),
	OBVR_KEY(Bool, "Hands", "LaserDot", hands.laserDot),
	OBVR_KEY(Bool, "Hands", "AimWithHand", hands.aimWithHand),
	OBVR_KEY(Bool, "Hands", "SpellFromCastingHand", hands.spellAim.enabled),
	OBVR_KEY(Float, "Hands", "SpellRaiseBelowEyesMetres", hands.spellAim.raiseBelowEyesMetres),
	OBVR_KEY(Float, "Hands", "SpellRaiseAheadMetres", hands.spellAim.raiseAheadMetres),
	OBVR_KEY(Float, "Hands", "SneakVignetteStrength", hands.sneakVignetteStrength),
	OBVR_KEY(Float, "Hands", "SneakEyeSize", hands.sneakEyeSize),
	OBVR_STEP(ReadSneakIndicator, "Hands", "SneakIndicator"),
	OBVR_KEY(Bool, "Hands", "LeftHanded", hands.leftHanded),
	OBVR_KEY(Bool, "Hands", "SneakHold", hands.sneakHold),
	OBVR_KEY(Bool, "Hands", "CrouchSneak", hands.crouchSneak),
	OBVR_KEY(Float, "Hands", "CrouchDropMetres", hands.crouchDropMetres),
	OBVR_KEY(Bool, "Hands", "KeepEyeHeightSneaking", hands.keepEyeHeightSneaking),
	OBVR_KEY(Float, "Hands", "WeaponWeight", hands.weaponWeightPercent),
	OBVR_KEY(Bool, "Hands", "WeaponSwingThrough", hands.weaponSwingThrough),
	OBVR_KEY(Float, "Hands", "OneHandTwoHanderWeight", hands.oneHandTwoHander.weightPercent),
	OBVR_KEY(Float, "Hands", "OneHandTwoHanderFatigue", hands.oneHandTwoHander.fatiguePercent),
	OBVR_KEY(Bool, "Hands", "OneHandTwoHanderHoldCosts", hands.oneHandTwoHander.holdCosts),
	OBVR_KEY(Float, "Hands", "OneHandTwoHanderHoldFatigue", hands.oneHandTwoHander.holdPerSecond),
	OBVR_KEY(Float, "Hands", "HipBelowEyesMetres", hands.oneHandTwoHander.hipBelowEyes),
	OBVR_KEY(Bool, "Hands", "WeaponStopsAtWalls", hands.weaponStopsAtWalls),
	OBVR_KEY(Float, "Hands", "WeaponLetGoMetres", hands.weaponLetGoMetres),
	OBVR_KEY(Float, "Hands", "WeaponLetGoDegrees", hands.weaponLetGoDegrees),
	OBVR_KEY(Float, "Hands", "BodyLetGoMetres", hands.bodyLetGoMetres),
	OBVR_KEY(Float, "Hands", "BodyLetGoDegrees", hands.bodyLetGoDegrees),
	OBVR_KEY(Bool, "Hands", "WeaponStopsAtBodies", hands.weaponStopsAtBodies),
	OBVR_KEY(Bool, "Hands", "WeaponHitStop", hands.weaponHitStop),
	OBVR_KEY(Bool, "Hands", "HandsStopAtWalls", hands.handsStopAtWalls),
	OBVR_KEY(Bool, "Hands", "HandsStopAtBodies", hands.handsStopAtBodies),
	OBVR_KEY(Bool, "Hands", "SwingsStopAtBodies", hands.swingsStopAtBodies),
	OBVR_KEY(Bool, "Hands", "BladeSlidesOffPeople", hands.bladeSlidesOffPeople),
	OBVR_KEY(Bool, "Hands", "BladeEntry", hands.bladeEntry.enabled),
	OBVR_KEY(Bool, "Hands", "BladeEntryCutMarks", hands.bladeEntry.cutMarks),
	OBVR_KEY(Bool, "Hands", "BladeEntryBlood", hands.bladeEntry.blood),
	OBVR_KEY(Bool, "Hands", "BladeSticks", hands.bladeEntry.stick.enabled),
	OBVR_KEY(Float, "Hands", "BladeSticksEaseIn", hands.bladeEntry.stick.easeInPerSecond),
	OBVR_KEY(Float, "Hands", "BladeSticksEaseOut", hands.bladeEntry.stick.easeOutPerSecond),
	OBVR_KEY(Float, "Hands", "BladeSticksTearUnits", hands.bladeEntry.stick.tearUnits),
	OBVR_KEY(Float, "Hands", "BladeSticksTearDegrees", hands.bladeEntry.stick.tearDegrees),
	OBVR_KEY(Float, "Hands", "BladeSticksPullOutUnits", hands.bladeEntry.stick.pullOutUnits),
	OBVR_KEY(Float, "Hands", "BladeSticksRagdollSpeed", hands.bladeEntry.stick.ragdollSpeedMetres),
	OBVR_KEY(Float, "Hands", "BladeSticksRagdollTearUnits", hands.bladeEntry.stick.ragdollTearUnits),
	OBVR_KEY(Float, "Hands", "BladeSticksDeepUnits", hands.bladeEntry.stick.deepUnits),
	OBVR_KEY(Bool, "Hands", "BladeSticksSkewer", hands.bladeEntry.stick.skewer),
	OBVR_KEY(Bool, "Hands", "NeckBleeds", hands.bladeEntry.neck.enabled),
	OBVR_KEY(Float, "Hands", "NeckBleedPulseSeconds", hands.bladeEntry.neck.pulseSeconds),
	OBVR_KEY(UInt, "Hands", "NeckBleedPulses", hands.bladeEntry.neck.livingPulses),
	OBVR_KEY(Float, "Hands", "NeckBleedDeadSeconds", hands.bladeEntry.neck.deadSeconds),
	OBVR_KEY(UInt, "Hands", "NeckBleedMostWounds", hands.bladeEntry.neck.mostWounds),
	OBVR_KEY(Float, "Hands", "BladeEntryStabSkin", hands.bladeEntry.stabSpeed[1]),
	OBVR_KEY(Float, "Hands", "BladeEntryStabCloth", hands.bladeEntry.stabSpeed[2]),
	OBVR_KEY(Float, "Hands", "BladeEntryStabLight", hands.bladeEntry.stabSpeed[3]),
	OBVR_KEY(Float, "Hands", "BladeEntrySlashSkin", hands.bladeEntry.slashSpeed[1]),
	OBVR_KEY(Float, "Hands", "BladeEntrySlashCloth", hands.bladeEntry.slashSpeed[2]),
	OBVR_KEY(Float, "Hands", "BladeEntrySlashLight", hands.bladeEntry.slashSpeed[3]),
	OBVR_KEY(Float, "Hands", "BladeEntryLightPerPoint", hands.bladeEntry.lightPerPoint),
	OBVR_KEY(Float, "Hands", "BladeEntryLightMostExtra", hands.bladeEntry.lightMostExtra),
	OBVR_KEY(Float, "Hands", "BladeEntryPressSkin", hands.bladeEntry.pressUnits[1]),
	OBVR_KEY(Float, "Hands", "BladeEntryPressCloth", hands.bladeEntry.pressUnits[2]),
	OBVR_KEY(Float, "Hands", "BladeEntryPressLight", hands.bladeEntry.pressUnits[3]),
	OBVR_KEY(Float, "Hands", "BladeEntryPressSecondsLight", hands.bladeEntry.pressSeconds[3]),
	OBVR_KEY(Bool, "Hands", "FittedBodyCapsules", hands.fittedBodyCapsules),
	OBVR_KEY(Bool, "Hands", "FittedWeaponShapes", hands.fittedWeaponShapes),
	OBVR_KEY(Float, "Hands", "FitLengthSplitRatio", hands.fitLengthSplitRatio),
	OBVR_KEY(Bool, "Hands", "FittedHands", hands.fittedHands),
	OBVR_KEY(Bool, "Debug", "CapsuleTrace", hands.capsuleTrace),
	OBVR_KEY(Bool, "Debug", "FitDump", hands.fitDump),
	OBVR_KEY(UInt, "Debug", "MeshGapTrace", hands.meshGapTraceLines),
	OBVR_KEY(Float, "Debug", "MenuNearShapes", hands.menuNearShapesUnits),
	OBVR_KEY(Bool, "Hands", "PunchesStopAtBodies", hands.punchesStopAtBodies),
	OBVR_KEY(UInt, "Debug", "ContactTrace", hands.contactTraceLines),
	OBVR_KEY(Bool, "Debug", "GripTrace", hands.gripTrace),
	OBVR_KEY(Bool, "Debug", "WeaponNodeTrace", hands.weaponNodeTrace),
	OBVR_KEY(Bool, "Hands", "HandsInMenus", hands.handsInMenus),
	OBVR_KEY(Bool, "Hands", "WeaponParries", hands.weaponParries),
	OBVR_KEY(Bool, "Hands", "ParryStopsAll", hands.parryStopsAll),
	OBVR_KEY(Float, "Hands", "ParryFatigue", hands.parryFatigue),
	OBVR_KEY(Bool, "Hands", "BlockByAngle", hands.blockByAngle),
	OBVR_KEY(Float, "Hands", "BlockBladeDegrees", hands.blockBladeDegrees),
	OBVR_KEY(Float, "Hands", "BlockShieldDegrees", hands.blockShieldDegrees),
	OBVR_KEY(Float, "Hands", "BlockReachUnits", hands.blockReachUnits),
	OBVR_KEY(Float, "Hands", "BlockBodyDegrees", hands.blockBodyDegrees),
	OBVR_KEY(Float, "Hands", "BlockInterposeUnits", hands.blockInterposeUnits),
	OBVR_KEY(Float, "Hands", "CombatReach", hands.combatReach),
	OBVR_KEY(Bool, "Hands", "SlowApproach", hands.slowApproach),
	OBVR_KEY(Float, "Hands", "SlowApproachMetres", hands.slowApproachMetres),
	OBVR_KEY(Bool, "Hands", "CombatRing", hands.combatRing),
	OBVR_KEY(Float, "Hands", "CombatRingMetres", hands.combatRingMetres),
	OBVR_KEY(Int, "Hands", "MeleeAttackersAtOnce", hands.meleeAttackersAtOnce),
	OBVR_KEY(Float, "Hands", "CombatRingTurnSeconds", hands.combatRingTurnSeconds),
	OBVR_KEY(Float, "Hands", "CombatRingTurnMaxSeconds", hands.combatRingTurnMaxSeconds),
	OBVR_KEY(Bool, "Hands", "CombatRingByLevel", hands.combatRingByLevel),
	OBVR_KEY(Bool, "Hands", "CombatRingHoldsBlows", hands.combatRingHoldsBlows),
	OBVR_KEY(Bool, "Hands", "CombatRingRangedShoot", hands.combatRingRangedShoot),
	OBVR_KEY(Bool, "Hands", "GrabFromInventory", hands.grabFromInventory),
	OBVR_KEY(Bool, "Hands", "WalkersShake", hands.walkersShake),
	OBVR_KEY(Bool, "Hands", "WalkersDisguise", hands.disguise.enabled),
	OBVR_KEY(Bool, "Hands", "DisguiseBloodyHands", hands.disguise.bloodyHands),
	OBVR_KEY(Float, "Hands", "DisguiseVignette", hands.disguise.vignetteStrength),
	OBVR_KEY(Float, "Hands", "DisguiseDarken", hands.disguise.darken),
	OBVR_KEY(Float, "Hands", "DisguiseBodyReachUnits", hands.disguise.corpseReachUnits),
	OBVR_KEY(Bool, "Hands", "WalkerGrip", hands.walkerGrip),
	OBVR_KEY(Bool, "Hands", "ThrowNoise", hands.throwNoise),
	OBVR_KEY(Float, "Hands", "ShakeStrokeMetres", hands.shake.strokeMetres),
	OBVR_KEY(Float, "Hands", "ShakeStrokeSpeed", hands.shake.strokeSpeed),
	OBVR_KEY(UInt, "Debug", "TestWalkersHold", hands.testWalkersHold),
	OBVR_KEY(Bool, "Debug", "TestCut", hands.testCut),
	OBVR_KEY(Text, "Hands", "CombatRingExceptions", hands.combatRingExceptions),
	OBVR_KEY(Text, "Hands", "AttackTempoExceptions", hands.attackTempoExceptions),
	OBVR_KEY(Bool, "Hands", "StrikesTrainSkills", hands.strikesTrainSkills),
	OBVR_KEY(Int, "Hands", "DismemberHeadSkillUses", hands.dismemberHeadSkillUses),
	OBVR_KEY(Int, "Hands", "DismemberHandSkillUses", hands.dismemberHandSkillUses),
	OBVR_KEY(Float, "Hands", "EnemyAttackSpeed", hands.enemyAttackSpeed),
	OBVR_KEY(Float, "Hands", "EnemyAttackFrequency", hands.enemyAttackFrequency),
	OBVR_KEY(Float, "Hands", "EnemyKnockback", hands.enemyKnockbackPercent),
	OBVR_KEY(Bool, "Hands", "EnemyKnockbackPowerOnly", hands.enemyKnockbackPowerOnly),
	OBVR_KEY(Bool, "Hands", "StaggerSides", hands.staggerSides),
	OBVR_KEY(Bool, "Hands", "PotionBelt", hands.potionBelt),
	OBVR_KEY(Bool, "Hands", "OffHandStrikes", hands.grapple.strikes),
	OBVR_KEY(Bool, "Hands", "OffHandGrabs", hands.grapple.grabs),
	OBVR_KEY(Float, "Hands", "GrabHoldSeconds", hands.grapple.holdSeconds),
	OBVR_KEY(Float, "Hands", "GrabFatigue", hands.grapple.fatigue),
	OBVR_KEY(Bool, "Hands", "GrabRagdoll", hands.grapple.ragdoll),
	OBVR_KEY(Float, "Hands", "GrabThrowStrength", hands.grapple.throwStrength),
	OBVR_KEY(Float, "Hands", "GrabWeight", hands.grapple.weight),
	OBVR_KEY(Bool, "Hands", "ReachOpens", hands.reachOpen.enabled),
	OBVR_KEY(Bool, "Hands", "LockpickByHand", hands.lockpickByHand),
	OBVR_KEY(Float, "Hands", "ReachOpenMetres", hands.reachOpen.openMetres),
	OBVR_KEY(Float, "Hands", "ReachCloseMetres", hands.reachOpen.closeMetres),
	OBVR_STEP(ReachCloseFollowsOpen, "", ""),
	OBVR_KEY(Float, "Hands", "ReachWalkAwayMetres", hands.reachOpen.walkAwayMetres),
	OBVR_STEP(ReachWalkAwayFollowsClose, "", ""),
	OBVR_KEY(Bool, "Hands", "DoorsByGrip", hands.doorGrip.enabled),
	OBVR_KEY(Float, "Hands", "DoorGripMoveMetres", hands.doorGrip.moveMetres),
	OBVR_STEP(DoorGripMoveClamp, "", ""),
	OBVR_KEY(Bool, "Hands", "RunToggle", hands.runToggle),
	OBVR_KEY(Float, "Hands", "HandoverReachMetres", hands.handoverReachMetres),
	OBVR_KEY(Float, "Hands", "HandoverThingMetres", hands.handoverThingMetres),
	OBVR_KEY(Bool, "Hands", "ArmSwingRun", hands.armSwing.enabled),
	OBVR_KEY(Float, "Hands", "ArmSwingSpeed", hands.armSwing.speedMetres),
	OBVR_KEY(Float, "Hands", "ArmSwingHoldSeconds", hands.armSwing.holdSeconds),
	OBVR_STEP(LaserNeedsOne, "", ""),
	OBVR_KEY(Float, "Hands", "HitBoundFactor", hands.hitBoundFactor),
	OBVR_KEY(Float, "Hands", "HitPadUnits", hands.hitPadUnits),
	OBVR_KEY(Bool, "Hands", "MotionHitsByTouch", hands.hitsByTouch),
	OBVR_KEY(Float, "Hands", "PowerStrikeSpeed", hands.powerStrikeSpeed),
	OBVR_KEY(Float, "Hands", "RestHandRight", hands.restHandRight),
	OBVR_KEY(Float, "Hands", "RestHandForward", hands.restHandForward),
	OBVR_KEY(Float, "Hands", "RestHandUp", hands.restHandUp),
	OBVR_KEY(Bool, "Hands", "ArmsFollowPitch", hands.armsFollowPitch),
	OBVR_KEY(Bool, "Hands", "ArmsFollowPosition", hands.armsFollowPosition),
	OBVR_KEY(Float, "Hands", "ArmOffsetScale", hands.armOffsetScale),
	OBVR_KEY(Float, "Hands", "WristUp", hands.wristUp),
	OBVR_KEY(Float, "Hands", "WristBack", hands.wristBack),
	OBVR_KEY(Float, "Hands", "WristTiltDegrees", hands.wristTiltDegrees),
	OBVR_KEY(Float, "Hands", "WristHudWidth", hands.wristHudWidth),
	OBVR_KEY(Float, "Hands", "WristMenuWidth", hands.wristMenuWidth),
	OBVR_KEY(Bool, "Hands", "WristHud", hands.wristHud),
	OBVR_KEY(Bool, "Hands", "WristMenu", hands.wristMenu),
	OBVR_KEY(Bool, "Hands", "PinHands", hands.pinHands),
	OBVR_KEY(Text, "Hands", "RightHandBone", hands.rightHandBone),
	OBVR_KEY(Text, "Hands", "LeftHandBone", hands.leftHandBone),
	OBVR_KEY(Float, "Hands", "RightHandRoll", hands.rightHandRoll),
	OBVR_KEY(Float, "Hands", "RightHandPitch", hands.rightHandPitch),
	OBVR_KEY(Float, "Hands", "RightHandYaw", hands.rightHandYaw),
	OBVR_KEY(Float, "Hands", "LeftHandRoll", hands.leftHandRoll),
	OBVR_KEY(Float, "Hands", "LeftHandPitch", hands.leftHandPitch),
	OBVR_KEY(Float, "Hands", "LeftHandYaw", hands.leftHandYaw),
	OBVR_KEY(Float, "Hands", "RightHandGripX", hands.rightHandGripX),
	OBVR_KEY(Float, "Hands", "RightHandGripY", hands.rightHandGripY),
	OBVR_KEY(Float, "Hands", "RightHandGripZ", hands.rightHandGripZ),
	OBVR_KEY(Float, "Hands", "LeftHandGripX", hands.leftHandGripX),
	OBVR_KEY(Float, "Hands", "LeftHandGripY", hands.leftHandGripY),
	OBVR_KEY(Float, "Hands", "LeftHandGripZ", hands.leftHandGripZ),
	OBVR_KEY(Bool, "Hands", "AdjustHands", hands.adjustHands),
	OBVR_KEY(Bool, "Hands", "HandsAdjusted", hands.handsAdjusted),
	OBVR_KEY(Float, "Hands", "HandGripUpMetres", hands.handGripUpMetres),
	OBVR_KEY(Float, "Hands", "HandGripForwardMetres", hands.handGripForwardMetres),
	OBVR_KEY(Float, "Hands", "GrabReachMetres", hands.grabReachMetres),
	OBVR_KEY(Float, "Hands", "PullReachMetres", hands.pullReachMetres),
	OBVR_KEY(Bool, "Hands", "LaserInWorld", hands.laserInWorld),
	OBVR_KEY(Bool, "Hands", "TargetOutline", hands.targetOutline),
	OBVR_KEY(Bool, "Hands", "TargetGlow", hands.targetGlow),
	OBVR_KEY(Float, "Hands", "WorldLaserMetres", hands.worldLaserMetres),
	OBVR_KEY(Float, "Hands", "PullSpeedMetres", hands.pullSpeedMetres),
	OBVR_KEY(Float, "Hands", "ReachNearSideMetres", hands.reachNearSideMetres),
	OBVR_KEY(Float, "Hands", "HeldObjectMetres", hands.heldObjectMetres),
	OBVR_KEY(Float, "Hands", "GripCurlDegrees", hands.gripCurlDegrees),
	OBVR_KEY(Bool, "Hands", "FingerTracking", hands.fingerTracking),
	OBVR_KEY(Float, "Hands", "ThrowStrength", hands.throwStrength),
	OBVR_KEY(Bool, "Hands", "HeldObjectsPush", hands.heldObjectsPush),
	OBVR_KEY(Bool, "Hands", "PushWorld", hands.pushWorld),
	OBVR_KEY(Bool, "Hands", "BodyCollision", hands.bodyCollision),
	OBVR_KEY(Float, "Hands", "PhysicsRate", hands.physicsRate),
	OBVR_KEY(Float, "Hands", "BodyRadiusScale", hands.bodyRadiusScale),
	OBVR_KEY(Bool, "Hands", "PushPeople", hands.pushPeople),
	OBVR_KEY(Bool, "Hands", "Shove", hands.shove.enabled),
	OBVR_KEY(Float, "Hands", "StaggerSpeed", hands.impact.staggerSpeed),
	OBVR_KEY(Float, "Hands", "KnockdownSpeed", hands.impact.knockdownSpeed),
	OBVR_KEY(Bool, "Hands", "VanillaKnockdown", hands.impact.vanillaKnockdown),
	OBVR_KEY(Float, "Hands", "ShoveHardForce", hands.shove.hardForce),
	OBVR_KEY(Float, "Hands", "ShoveHardForceMost", hands.shove.hardForceMost),
	OBVR_KEY(Float, "Hands", "ShoveDistance", hands.shove.distance),
	OBVR_KEY(Bool, "Hands", "ShoveCountsAsHit", hands.shove.countsAsHit),
	OBVR_KEY(Float, "Hands", "SlapDisposition", hands.shove.dispositionFace),
	OBVR_KEY(Float, "Hands", "SlapSpeed", hands.shove.slapSpeed),
	OBVR_KEY(Bool, "Hands", "Kick", hands.kick.enabled),
	OBVR_KEY(Float, "Hands", "KickCooldownSeconds", hands.kick.cooldownSeconds),
	OBVR_KEY(Float, "Hands", "KickOutSeconds", hands.kick.outSeconds),
	OBVR_KEY(Float, "Hands", "KickHoldSeconds", hands.kick.holdSeconds),
	OBVR_KEY(Float, "Hands", "KickHoldMaxSeconds", hands.kick.holdMaxSeconds),
	OBVR_KEY(Float, "Hands", "KickBackSeconds", hands.kick.backSeconds),
	OBVR_KEY(Float, "Hands", "KickReachShare", hands.kick.reachShare),
	OBVR_KEY(Float, "Hands", "KickDropDegrees", hands.kick.dropDegrees),
	OBVR_KEY(Float, "Hands", "KickLookShare", hands.kick.lookShare),
	OBVR_KEY(Float, "Hands", "KickHighestDegrees", hands.kick.highestDegrees),
	OBVR_KEY(Float, "Hands", "KickLowestDegrees", hands.kick.lowestDegrees),
	OBVR_KEY(Float, "Hands", "KickAimUnits", hands.kick.aimUnits),
	OBVR_KEY(Float, "Hands", "KickFatigue", hands.kick.fatigue),
	OBVR_KEY(Float, "Hands", "FlyCamSpeed", hands.flyCam.speedUnits),
	OBVR_KEY(Float, "Hands", "FlyCamTurnDegrees", hands.flyCam.turnDegrees),
	OBVR_KEY(Bool, "Hands", "RoomScaleBody", hands.roomFollow.enabled),
	OBVR_KEY(Float, "Hands", "RoomScaleMaxStep", hands.roomFollow.maxStepUnits),
	OBVR_KEY(Float, "Hands", "RoomScaleStart", hands.roomFollow.startUnits),
	OBVR_KEY(Bool, "Hands", "AnalogWalk", hands.analogWalk.enabled),
	OBVR_KEY(Float, "Hands", "AnalogRunFrom", hands.analogWalk.runFrom),
	OBVR_KEY(Float, "Hands", "AnalogDeadZone", hands.analogWalk.deadZone),
	OBVR_KEY(Float, "Hands", "AnalogCurve", hands.analogWalk.curve),
	OBVR_KEY(Float, "Hands", "SprintFactor", hands.analogWalk.sprintFactor),
	OBVR_KEY(Float, "Hands", "SprintFatigue", hands.analogWalk.sprintFatiguePerSecond),
	OBVR_KEY(Bool, "Hands", "Dismemberment", hands.dismember.enabled),
	OBVR_KEY(Float, "Hands", "DismemberMinSpeed", hands.dismember.minSpeedMetres),
	OBVR_KEY(Float, "Hands", "DismemberLimbMinSpeed", hands.dismember.limbMinSpeedMetres),
	OBVR_KEY(Float, "Hands", "DismemberTwoHandedShare", hands.dismember.twoHandedShare),
	OBVR_KEY(Bool, "Hands", "DismemberCutKills", hands.dismember.cutKills),
	OBVR_KEY(Bool, "Hands", "DismemberBlood", hands.dismember.blood),
	OBVR_KEY(Bool, "Hands", "DismemberCaps", hands.dismember.caps),
	OBVR_KEY(Bool, "Hands", "DismemberSound", hands.dismember.sound),
	OBVR_KEY(Int, "Hands", "DismemberMostCuts", hands.dismember.most),
	OBVR_KEY(Bool, "Hands", "ShieldBash", hands.shieldBash.enabled),
	OBVR_KEY(Bool, "Hands", "TorchBlocks", hands.torchBlocks),
	OBVR_KEY(Bool, "Hands", "Yield", hands.yield),
	OBVR_KEY(Bool, "Hands", "PommelStrikes", hands.pommel.enabled),
	OBVR_KEY(Bool, "Hands", "HalfSwording", hands.halfSword.enabled),
	OBVR_KEY(Float, "Hands", "HalfSwordFromUnits", hands.halfSword.fromUnits),
	OBVR_KEY(Float, "Hands", "HalfSwordTipInsetUnits", hands.halfSword.tipInsetUnits),
	OBVR_KEY(Bool, "Hands", "HalfSwordRightHand", hands.halfSword.rightHand),
	OBVR_KEY(Bool, "Hands", "ChokeUp", hands.halfSword.chokeUp),
	OBVR_KEY(Float, "Hands", "ShieldBashFatigue", hands.shieldBash.fatigue),
	OBVR_STEP(KickOnB, "", ""),
	OBVR_STEP(ReadButtonBindings, "Buttons", ""),
	OBVR_KEY(Float, "Hands", "MenuHoldSeconds", hands.menuHoldSeconds),
	OBVR_KEY(Float, "Hands", "MenuRingSeconds", hands.menuRingSeconds),
	OBVR_KEY(Float, "Hands", "MenuTapSeconds", hands.menuTapSeconds),
	OBVR_KEY(Bool, "Hands", "MiddleFinger", hands.middleFinger),
	OBVR_KEY(Float, "Hands", "MiddleFingerDisposition", hands.middleFingerDisposition),
	OBVR_KEY(Bool, "Hands", "LeadByHand", hands.lead.enabled),
	OBVR_KEY(Bool, "Hands", "ThrowHits", hands.throwHit.enabled),
	OBVR_KEY(Bool, "Hands", "TwoHandGrip", hands.twoHand.enabled),
	OBVR_KEY(Float, "Hands", "TwoHandReachUnits", hands.twoHand.reachUnits),
	OBVR_KEY(Float, "Hands", "TwoHandBlendSeconds", hands.twoHand.blendSeconds),
	OBVR_KEY(Float, "Hands", "TwoHandPreshapeUnits", hands.twoHand.preshapeUnits),
	OBVR_KEY(Float, "Hands", "TwoHandApproachOpen", hands.twoHand.approachOpenShare),
	OBVR_KEY(Float, "Hands", "TwoHandCloseAt", hands.twoHand.closeAt),
	OBVR_KEY(Float, "Hands", "ArmorWearMult", gearWear.armorWearMult),
	OBVR_KEY(Float, "Hands", "WeaponWearMult", gearWear.weaponWearMult),
	OBVR_KEY(UInt, "Hands", "PlayerGearBreaksOff", gearWear.playerGearBreaksOff),
	OBVR_KEY(Float, "Hands", "GearKnockFlightSeconds", gearWear.knockFlightSeconds),
	OBVR_KEY(Bool, "Hands", "TwoHandOneHanders", hands.twoHand.oneHanders),
	OBVR_KEY(Bool, "Hands", "TwoHandSlide", hands.twoHand.slide),
	OBVR_KEY(Float, "Hands", "TwoHandReleaseSeconds", hands.twoHand.releaseSeconds),
	OBVR_KEY(Float, "Hands", "TwoHandSlackUnits", hands.twoHand.slackUnits),
	OBVR_KEY(Bool, "Hands", "BowByHand", hands.archery.enabled),
	OBVR_KEY(Float, "Hands", "QuiverX", hands.archery.quiverZone.x),
	OBVR_KEY(Float, "Hands", "QuiverForward", hands.archery.quiverZone.y),
	OBVR_KEY(Float, "Hands", "QuiverUp", hands.archery.quiverZone.z),
	OBVR_KEY(Float, "Hands", "QuiverRadius", hands.archery.quiverRadius),
	OBVR_KEY(Bool, "Hands", "QuiverAtHips", hands.archery.quiverAtHips),
	OBVR_KEY(Bool, "Hands", "HolsterLabels", hands.holster.labels),
	OBVR_KEY(Float, "Hands", "NockMetres", hands.archery.nockMetres),
	OBVR_KEY(Float, "Hands", "BowStringMetres", hands.archery.stringMetres),
	OBVR_KEY(Float, "Hands", "BowGuideMetres", hands.archery.guideMetres),
	OBVR_KEY(Float, "Hands", "BowDrawStartMetres", hands.archery.drawStartMetres),
	OBVR_KEY(Float, "Hands", "BowUnnockMetres", hands.archery.unnockMetres),
	OBVR_STEP(ArrowWithGrip, "Hands", "ArrowWithGrip"),
	OBVR_KEY(Bool, "Hands", "ArrowStabs", hands.archery.stabs),
	OBVR_KEY(Float, "Hands", "LeadStrangerMetres", hands.lead.strangerMetres),
	OBVR_KEY(Float, "Hands", "LeadTakeUnits", hands.lead.takeUnits),
	OBVR_KEY(Float, "Hands", "LeadBreakUnits", hands.lead.breakUnits),
	OBVR_KEY(UInt, "Hands", "LeadFollowUnits", hands.lead.followUnits),
	OBVR_KEY(Float, "Hands", "LeadDispositionPerMetre", hands.lead.dispositionPerMetre),
	OBVR_KEY(Bool, "HandHud", "Enabled", hands.handHud.enabled),
	OBVR_STEP(ReadHandHudElements, "HandHud", "<Element>Place <Element>Opacity <Element>Size"),
	OBVR_KEY(Bool, "HandHud", "EnemyHealthBars", hands.enemyHealth.enabled),
	OBVR_KEY(Float, "HandHud", "EnemyHealthMetres", hands.enemyHealth.rangeMetres),
	OBVR_KEY(Float, "HandHud", "EnemyHealthWidthMetres", hands.enemyHealth.widthMetres),
	OBVR_KEY(Float, "HandHud", "EnemyHealthAboveMetres", hands.enemyHealth.aboveMetres),
	OBVR_KEY(Float, "HandHud", "EnemyHealthShowSeconds", hands.enemyHealth.showSeconds),
	OBVR_KEY(Float, "HandHud", "PanelUp", hands.handHud.panelUp),
	OBVR_KEY(Float, "HandHud", "PanelBack", hands.handHud.panelBack),
	OBVR_KEY(Float, "HandHud", "PanelTiltDegrees", hands.handHud.panelTiltDegrees),
	OBVR_KEY(Float, "HandHud", "UnitMetres", hands.handHud.unitMetres),
	OBVR_KEY(Float, "HandHud", "GapMetres", hands.handHud.gapMetres),
	OBVR_KEY(Float, "HandHud", "CompassElevationDegrees", hands.handHud.compassElevationDegrees),
	OBVR_KEY(Float, "HandHud", "CompassDistanceMetres", hands.handHud.compassDistanceMetres),
	OBVR_KEY(Float, "HandHud", "SkyUnitMetres", hands.handHud.skyUnitMetres),
	OBVR_KEY(Float, "HandHud", "CompassFadeStartDegrees", hands.handHud.compassFadeStartDegrees),
	OBVR_KEY(Float, "HandHud", "CompassFadeFullDegrees", hands.handHud.compassFadeFullDegrees),
	OBVR_KEY(Float, "HandHud", "ViewDistanceMetres", hands.handHud.viewDistanceMetres),
	OBVR_KEY(Float, "HandHud", "ViewTopDegrees", hands.handHud.viewTopDegrees),
	OBVR_KEY(Float, "HandHud", "ViewBottomDegrees", hands.handHud.viewBottomDegrees),
	OBVR_KEY(Float, "HandHud", "ViewUnitMetres", hands.handHud.viewUnitMetres),
	OBVR_KEY(Float, "HandHud", "ViewFollowDegrees", hands.handHud.viewFollowDegrees),
	OBVR_KEY(Float, "HandHud", "ViewFollowSeconds", hands.handHud.viewFollowSeconds),
	OBVR_KEY(Bool, "HandHud", "ViewLockedToHead", hands.handHud.viewLockedToHead),
	OBVR_KEY(Bool, "HandHud", "ShowOnLook", hands.handHud.showOnLook),
	OBVR_KEY(Float, "HandHud", "LookGazeDegrees", hands.handHud.lookGazeDegrees),
	OBVR_KEY(Float, "HandHud", "LookPalmDegrees", hands.handHud.lookPalmDegrees),
	OBVR_KEY(Float, "HandHud", "LookFadeSeconds", hands.handHud.lookFadeSeconds),
	OBVR_KEY(Float, "HandHud", "PalmLiftMetres", hands.handHud.palmLiftMetres),
	OBVR_KEY(Float, "HandHud", "PalmDownMetres", hands.handHud.palmDownMetres),
	OBVR_KEY(Float, "HandHud", "OffsetRightMetres", hands.handHud.offsetRightMetres),
	OBVR_KEY(Float, "HandHud", "OffsetUpMetres", hands.handHud.offsetUpMetres),
	OBVR_KEY(Bool, "Hands", "LevitateObjects", hands.levitateObjects),
	OBVR_KEY(Bool, "Hands", "AttachSmallObjects", hands.attachSmallObjects),
	OBVR_KEY(Bool, "Hands", "ReachTooltip", hands.reachTooltip),
	OBVR_KEY(Float, "Hands", "TooltipScale", hands.tooltipScale),
	OBVR_KEY(Bool, "Hands", "ReachRing", hands.reachRing),
	OBVR_KEY(Float, "Hands", "ReachMarkerMetres", hands.reachMarkerMetres),
	OBVR_KEY(Float, "Hands", "ReachMarkerOpacity", hands.reachMarkerOpacity),
	OBVR_KEY(Bool, "Hands", "MenuOnRight", hands.menuOnRight),
	OBVR_KEY(Float, "Hands", "PokeTipForward", hands.pokeTipForward),
	OBVR_KEY(Float, "Hands", "PokeHover", hands.poke.hover),
	OBVR_KEY(Float, "Hands", "PokePress", hands.poke.press),
	OBVR_KEY(Float, "Hands", "PokeRelease", hands.poke.release),
	OBVR_KEY(Float, "Hands", "PokeThrough", hands.poke.through),
	OBVR_KEY(Bool, "Hands", "HideArms", hands.hideArms),
	OBVR_KEY(Text, "Hands", "HideFirstPersonNodes", hands.hideNodes),
	OBVR_KEY(Bool, "Hands", "HideSheaths", hands.hideSheaths),
	OBVR_KEY(Bool, "Hands", "HideWornCuirass", hands.hideWornCuirass),
	OBVR_KEY(Bool, "Hands", "ClosedHands", hands.closedHands),
	OBVR_KEY(Float, "Hands", "ClosedHandsBrightness", hands.closedHandsBrightness),
	OBVR_KEY(Bool, "Hands", "CloseBareWrists", hands.closeBareWrists),
	OBVR_KEY(Float, "Hands", "LaserGain", hands.laserGain),
	OBVR_KEY(Float, "Hands", "LaserMaxStep", hands.laserMaxStep),
	OBVR_KEY(Bool, "Hands", "LaserCursorDirect", hands.laserCursorDirect),
	OBVR_KEY(Float, "Hands", "StickHoldSeconds", hands.stickHoldSeconds),
	OBVR_KEY(Float, "Hands", "StickDeadZone", hands.stickDeadZone),
	OBVR_KEY(Float, "Hands", "TurnSpeed", hands.turnSpeed),
	OBVR_KEY(Bool, "Hands", "VrKeyboard", hands.vrKeyboard),
	OBVR_KEY(Bool, "Hands", "FirstPersonAfterLoad", hands.firstPersonAfterLoad),
	OBVR_KEY(Bool, "Hands", "FirstPersonAfterKnockdown", hands.firstPersonAfterKnockdown),
	OBVR_KEY(Bool, "Hands", "MenuHaptics", hands.menuHaptics),
	OBVR_KEY(Bool, "Hands", "ContactHaptics", hands.contactHaptics),
	OBVR_KEY(Bool, "Hands", "HitHaptics", hands.hitHaptics),
	OBVR_KEY(Bool, "Hands", "BackgroundInput", hands.backgroundInput),
	OBVR_KEY(Bool, "Hands", "LaserPicksItems", hands.laserPicksItems),
	OBVR_STEP(ReadWalkDirection, "Hands", "WalkDirection"),
	OBVR_STEP(ReadCrosshairPlace, "Hands", "CrosshairPlace"),
	OBVR_KEY(Bool, "Hands", "QuickMenu", hands.quickMenu.enabled),
	OBVR_KEY(Float, "Hands", "QuickMenuDeadZoneMetres", hands.quickMenu.deadZoneMetres),
	OBVR_KEY(Float, "Hands", "QuickMenuTapSeconds", hands.quickMenu.tapSeconds),
	OBVR_KEY(Float, "Hands", "QuickMenuRingMetres", hands.quickMenu.ringMetres),
	OBVR_KEY(Bool, "Hands", "QuickMenuIcons", hands.quickMenu.icons),
	OBVR_KEY(Bool, "Hands", "SpellRing", hands.spellRing),
	OBVR_KEY(Bool, "Hands", "Holsters", hands.holster.enabled),
	OBVR_KEY(Bool, "Hands", "HolsterShieldTorch", hands.holster.offHandItems),
	OBVR_KEY(Bool, "Hands", "GripByPull", hands.gripHold.byPull),
	OBVR_KEY(Float, "Hands", "GripCloseAt", hands.gripHold.closeAt),
	OBVR_KEY(Float, "Hands", "GripOpenAt", hands.gripHold.openAt),
	OBVR_KEY(Bool, "Hands", "GrabEquipsWeapons", hands.weaponGrab.enabled),
	OBVR_KEY(Bool, "Hands", "GrabEquipsTorches", hands.weaponGrab.torches),
	OBVR_KEY(Float, "Hands", "WeaponGripReachMetres", hands.weaponGrab.gripReachMetres),
	OBVR_KEY(Bool, "Hands", "PullToHand", hands.weaponGrab.pullToHand),
	OBVR_KEY(Bool, "Hands", "TakeDrawsAtOnce", hands.weaponGrab.drawAtOnce),
	OBVR_KEY(Bool, "Hands", "WeaponHold", hands.weaponRelease.enabled),
	OBVR_KEY(Float, "Hands", "WeaponReleaseSeconds", hands.weaponRelease.releaseSeconds),
	OBVR_KEY(Float, "Hands", "WeaponThrowStrength", hands.weaponRelease.throwStrength),
	OBVR_KEY(Bool, "Hands", "TorchDrop", hands.torchRelease.enabled),
	OBVR_KEY(Float, "Hands", "TorchGraceSeconds", hands.torchRelease.graceSeconds),
	OBVR_STEP(TorchReleaseFollowsWeapon, "", ""),
	OBVR_KEY(Float, "Hands", "ThrowSpinStrength", hands.weaponRelease.throwSpinStrength),
	OBVR_STEP(TorchSpinFollowsWeapon, "", ""),
	OBVR_KEY(Bool, "Hands", "TorchWithTwoHander", hands.torchWithTwoHander),
	OBVR_KEY(Bool, "Hands", "TorchKnockdownPowerOnly", hands.torchKnockdownPowerOnly),
	OBVR_STEP(ApplyImpact, "Hands", "StaggerSpeed"),
	OBVR_KEY(Float, "Hands", "HolsterOneHandX", hands.holster.oneHandZone.x),
	OBVR_KEY(Float, "Hands", "HolsterOneHandForward", hands.holster.oneHandZone.y),
	OBVR_KEY(Float, "Hands", "HolsterOneHandUp", hands.holster.oneHandZone.z),
	OBVR_KEY(Float, "Hands", "HolsterOneHandRadius", hands.holster.oneHandRadius),
	OBVR_KEY(Float, "Hands", "HolsterTwoHandX", hands.holster.twoHandZone.x),
	OBVR_KEY(Float, "Hands", "HolsterTwoHandForward", hands.holster.twoHandZone.y),
	OBVR_KEY(Float, "Hands", "HolsterTwoHandUp", hands.holster.twoHandZone.z),
	OBVR_KEY(Float, "Hands", "HolsterTwoHandRadius", hands.holster.twoHandRadius),
	OBVR_KEY(Float, "Hands", "HolsterBowX", hands.holster.bowZone.x),
	OBVR_KEY(Float, "Hands", "HolsterBowForward", hands.holster.bowZone.y),
	OBVR_KEY(Float, "Hands", "HolsterBowUp", hands.holster.bowZone.z),
	OBVR_KEY(Float, "Hands", "HolsterBowRadius", hands.holster.bowRadius),
	OBVR_STEP(ReadHolsterSides, "Hands", "HolsterOneHandSide HolsterTwoHandSide HolsterBowSide"),
	OBVR_KEY(Bool, "Hands", "Fists", hands.fist.enabled),
	OBVR_KEY(Float, "Hands", "FistCloseCurl", hands.fist.closeCurl),
	OBVR_KEY(Float, "Hands", "FistOpenCurl", hands.fist.openCurl),
	OBVR_KEY(Float, "Hands", "FistGripForce", hands.fist.gripForce),
	OBVR_KEY(Float, "Hands", "FistHoldSeconds", hands.fist.holdSeconds),
	OBVR_KEY(Float, "Hands", "WeaponDrawSpeed", hands.weaponDrawSpeed),
	OBVR_KEY(Bool, "Hands", "StowAtBody", hands.stow.enabled),
	OBVR_KEY(Bool, "Hands", "TakeOnlyByHand", hands.stow.takeOnlyByHand),
	OBVR_KEY(Float, "Hands", "StowRight", hands.stow.centreRight),
	OBVR_KEY(Float, "Hands", "StowForward", hands.stow.centreForward),
	OBVR_KEY(Float, "Hands", "StowUp", hands.stow.centreUp),
	OBVR_KEY(Float, "Hands", "StowRadius", hands.stow.radius),
	OBVR_KEY(Bool, "Hands", "StowSpotVisible", hands.stow.spotVisible),
	OBVR_KEY(Bool, "Hands", "EatAtMouth", hands.stow.eatAtMouth),
	OBVR_KEY(Float, "Hands", "MouthDownMetres", hands.stow.mouthDown),
	OBVR_KEY(Float, "Hands", "MouthForwardMetres", hands.stow.mouthForward),
	OBVR_KEY(Float, "Hands", "MouthReachMetres", hands.stow.mouthRadius),
	OBVR_KEY(Float, "Hands", "MouthThingReachMetres", hands.stow.mouthThingReach),
	OBVR_KEY(Bool, "Locomotion", "Teleport", hands.teleport.enabled),
	OBVR_KEY(Bool, "Locomotion", "TeleportInstant", hands.teleport.instant),
	OBVR_KEY(Float, "Locomotion", "TeleportRangeMetres", hands.teleport.rangeMetres),
	OBVR_KEY(Float, "Locomotion", "TeleportGlideSpeed", hands.teleport.glideMetresPerSecond),
	OBVR_KEY(Float, "Locomotion", "TeleportFadeSeconds", hands.teleport.fadeSeconds),
	OBVR_KEY(Bool, "Locomotion", "TeleportInstantFade", hands.teleport.instantFade),
	OBVR_KEY(Bool, "Locomotion", "TeleportInCombat", hands.teleport.inCombat),
	OBVR_KEY(Bool, "Locomotion", "TeleportVignette", hands.teleport.vignette),
	OBVR_KEY(Bool, "Locomotion", "TeleportBlink", hands.teleport.blink),
	OBVR_KEY(Bool, "Locomotion", "TeleportMakesNoise", hands.teleport.makesNoise),
	OBVR_STEP(ReadTeleportSound, "Locomotion", "TeleportSound"),
	OBVR_KEY(Float, "Locomotion", "TeleportFatigueMult", hands.teleport.fatigueMult),
	OBVR_KEY(Float, "Locomotion", "TeleportBlinkFatiguePerMetreUp", hands.teleport.blinkFatiguePerMetreUp),
	OBVR_KEY(Float, "Locomotion", "TeleportStickStart", hands.teleport.startThreshold),
	OBVR_KEY(Float, "Locomotion", "TeleportStickRelease", hands.teleport.releaseThreshold),
	OBVR_KEY(Float, "Locomotion", "TeleportStickConeDegrees", hands.teleport.coneDegrees),
	OBVR_KEY(Float, "Locomotion", "TeleportHoldSeconds", hands.teleport.holdSeconds),
	OBVR_KEY(KeyCode, "Hands", "AttackKey", handKeys.attack),
	OBVR_KEY(KeyCode, "Hands", "BlockKey", handKeys.block),
	OBVR_KEY(KeyCode, "Hands", "CastKey", handKeys.cast),
	OBVR_KEY(KeyCode, "Hands", "ActivateKey", handKeys.activate),
	OBVR_KEY(KeyCode, "Hands", "GrabKey", handKeys.grab),
	OBVR_KEY(KeyCode, "Hands", "JumpKey", handKeys.jump),
	OBVR_KEY(KeyCode, "Hands", "SneakKey", handKeys.sneak),
	OBVR_KEY(KeyCode, "Hands", "RunKey", handKeys.run),
	OBVR_KEY(KeyCode, "Hands", "ReadyWeaponKey", handKeys.readyWeapon),
	OBVR_KEY(KeyCode, "Hands", "MenuKey", handKeys.menu),
	OBVR_KEY(KeyCode, "Hands", "EscapeKey", handKeys.escape),
	OBVR_KEY(KeyCode, "Hands", "QuickMenuKey", handKeys.quickMenu),
	OBVR_KEY(KeyCode, "Hands", "QuickSaveKey", handKeys.quickSave),
	OBVR_KEY(KeyCode, "Hands", "TogglePovKey", handKeys.togglePov),
	OBVR_KEY(KeyCode, "Hands", "WaitKey", handKeys.wait),
	OBVR_KEY(KeyCode, "Hands", "ForwardKey", handKeys.forward),
	OBVR_KEY(KeyCode, "Hands", "BackKey", handKeys.back),
	OBVR_KEY(KeyCode, "Hands", "LeftKey", handKeys.left),
	OBVR_KEY(KeyCode, "Hands", "RightKey", handKeys.right),
	OBVR_KEY(Bool, "Debug", "LayoutProbe", layoutProbe),
	OBVR_KEY(Bool, "Debug", "CursorProbe", cursorProbe),
};

#undef OBVR_KEY
#undef OBVR_LEGACY_WORDS
#undef OBVR_STEP

void ReadRuntimeValues(Config& config, const char* path) {
	for (const obvr::config::IniKey& row : kConfigKeys) {
		switch (row.kind) {
			case obvr::config::IniKind::Float: {
				float& v = *static_cast<float*>(row.at(config));
				v = ReadFloat(row.section, row.key, v, path);
				break;
			}
			case obvr::config::IniKind::Bool: {
				bool& v = *static_cast<bool*>(row.at(config));
				v = ReadBool(row.section, row.key, v, path);
				break;
			}
			case obvr::config::IniKind::UInt: {
				UInt32& v = *static_cast<UInt32*>(row.at(config));
				v = ReadUInt(row.section, row.key, v, path);
				break;
			}
			case obvr::config::IniKind::Int: {
				int& v = *static_cast<int*>(row.at(config));
				v = static_cast<int>(ReadUInt(row.section, row.key, static_cast<UInt32>(v), path));
				break;
			}
			case obvr::config::IniKind::KeyCode: {
				UInt32& v = *static_cast<UInt32*>(row.at(config));
				v = ReadKeyCode(row.section, row.key, v, path);
				break;
			}
			case obvr::config::IniKind::HexColor: {
				UInt32& v = *static_cast<UInt32*>(row.at(config));
				v = ReadHexColor(row.section, row.key, v, path);
				break;
			}
			case obvr::config::IniKind::AnchorIsWorld: {
				bool& v = *static_cast<bool*>(row.at(config));
				v = ReadAnchorIsWorld(row.section, row.key, v, path);
				break;
			}
			case obvr::config::IniKind::MenusInWorld: {
				bool& v = *static_cast<bool*>(row.at(config));
				v = ReadMenusInWorld(row.section, row.key, v, path);
				break;
			}
			case obvr::config::IniKind::LegacyWordBool: {
				bool& v = *static_cast<bool*>(row.at(config));
				v = ReadLegacyWordBool(row.section, row.key, v, row.falseWord, row.trueWord, path);
				break;
			}
			case obvr::config::IniKind::Text:
				ReadText(row.section, row.key, static_cast<char*>(row.at(config)), row.size, path);
				break;
			case obvr::config::IniKind::Step:
				row.step(config, path);
				break;
		}
	}
}

// A second file read over OBVR.ini: OBVR-test.ini beside it, which only the
// test runners write (tools/hand-script-run.py, .ps1), so that a run never
// has to touch the player's own OBVR.ini. Only the keys it has change
// anything; the runner deletes it when the run ends.
//
// Its gate (core/HarnessToken.h): the file counts - is read over OBVR.ini,
// and takes a setting's change - only when the HarnessToken it names is the
// OBVR_HARNESS the runner put into this game's environment. A game started
// through Steam carries none, so neither a file left behind by a run cut
// short (2026-09-30) nor another run's file beside the player's game
// (2026-10-23, the tester's measurement recorded nothing) ever turns it into
// a harness game. Said in the log whenever the verdict changes: once for a
// run, once for a file that should not be there.
bool TestOverlayActive(const char* path) {
	if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
		return false;
	}
	char fileToken[64]{};
	GetPrivateProfileStringA("Debug", "HarnessToken", "", fileToken, sizeof(fileToken), path);
	char envToken[64]{};
	GetEnvironmentVariableA("OBVR_HARNESS", envToken, sizeof(envToken));
	const HarnessOverlayVerdict verdict = JudgeHarnessOverlay(fileToken, envToken);
	static bool said = false;
	static HarnessOverlayVerdict saidVerdict = HarnessOverlayVerdict::Accepted;
	if (!said || saidVerdict != verdict) {
		said = true;
		saidVerdict = verdict;
		OBVR_LOG("Config: TEST OVERLAY %s %s", path, HarnessOverlayVerdictText(verdict));
	}
	return verdict == HarnessOverlayVerdict::Accepted;
}

void ReadTestOverlay(Config& config) {
	char path[512];
	if (!platform::BuildPluginPath("OBVR-test.ini", path, sizeof(path)) || !TestOverlayActive(path)) {
		return;
	}
	ReadRuntimeValues(config, path);
}

}  // namespace

namespace config {

const IniKey* ConfigKeys(UInt32& count) {
	count = static_cast<UInt32>(sizeof(kConfigKeys) / sizeof(kConfigKeys[0]));
	return kConfigKeys;
}

}  // namespace config

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
	ReadTestOverlay(*this);
	// Stamped after the read, so a write that lands in between is seen by the
	// first reload rather than lost; read twice is the lesser fault.
	config::Remember(g_reloadGate, StampFiles(path));

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
	OBVR_LOG("Config: Performance.Enabled=%d GpuTiming=%d CaptureSeconds=%u MaxRecords=%u",
	         performance.enabled ? 1 : 0, performance.gpuTiming ? 1 : 0,
	         performance.captureSeconds, performance.maxRecords);
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
	OBVR_LOG("Config: SnapTurning=%d Angle=%.0f° Instant=%d Speed=%.1f Vignette=%d DeadZone=%.2f "
	         "SmoothTurning=%d TurnSpeed=%.1f",
	         look.snapTurning ? 1 : 0, static_cast<double>(look.snapTurnAngle),
	         look.snapTurnInstant ? 1 : 0, static_cast<double>(look.snapTurnSpeed),
	         look.snapTurnVignette ? 1 : 0, static_cast<double>(look.snapTurnDeadZone),
	         look.smoothTurning ? 1 : 0, static_cast<double>(look.turnSpeed));
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
	OBVR_LOG("Config: Debug.NativeMenuLifecycleProbe=%d", nativeMenuLifecycleProbe ? 1 : 0);
	if (nativeMenuLifecycleProbe) {
		OBVR_LOG("Config: Debug.NativeMenuLifecycleProbe=1 - guarded native personal-menu "
		         "lifecycle diagnostics enabled; F9 requests open and F10 requests close");
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
	OBVR_LOG("Config: Render.Crosshair=%d InThirdPerson=%d PersistentCache=%d "
	         "Tooltips=(1st %d, 3rd %d, above-name %d)",
	         tracker.crosshair ? 1 : 0, tracker.crosshairInThirdPerson ? 1 : 0,
	         tracker.crosshairPersistentCache ? 1 : 0,
	         tracker.crosshairTooltipsFirstPerson ? 1 : 0,
	         tracker.crosshairTooltipsThirdPerson ? 1 : 0,
	         tracker.crosshairTooltipsAboveName ? 1 : 0);
	OBVR_LOG("Config: Body.Visible=%d HideHead=%d CullHiddenHead=%d HideArms=%d Eyes=(forward %.1f, up %.1f) NearPlane=%.1f "
	         "PovSwitchPatch=%d",
	         body.visible ? 1 : 0, body.hideHead ? 1 : 0, body.cullHiddenHead ? 1 : 0, body.hideArms ? 1 : 0,
	         static_cast<double>(body.eyeForward), static_cast<double>(body.eyeUp),
	         static_cast<double>(body.nearPlaneUnits), body.povSwitchPatch ? 1 : 0);
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

	// Asked before read: the read is 374 GetPrivateProfileString calls over a
	// file of over a hundred kilobytes, 75 ms measured, and it runs on the
	// frame's own thread. The question is one directory read.
	if (config::Decide(g_reloadGate, StampFiles(path)) != config::ReloadVerdict::Changed) {
		return false;
	}

	const long long started = ReadPerformanceCounter();
	ReadRuntimeValues(*this, path);
	ReadTestOverlay(*this);
	const long long frequency = ReadPerformanceFrequency();
	const double milliseconds =
		frequency > 0 ? (ReadPerformanceCounter() - started) * 1000.0 / frequency : 0.0;
	// Said each time, because each time is a change someone made - and the
	// figure is the stutter that change cost, which should stay rare.
	OBVR_LOG("Config: %s changed - re-read in %.1f ms", fileName, milliseconds);
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
	// During a test run the change goes to OBVR-test.ini, which is read over
	// OBVR.ini and deleted when the run ends: a run that changes a setting
	// (the weapon places' fit) leaves the player's file as it was. Only the
	// run's own overlay (TestOverlayActive): a file left behind takes nothing.
	char overlay[512];
	const bool toOverlay =
		platform::BuildPluginPath("OBVR-test.ini", overlay, sizeof(overlay)) && TestOverlayActive(overlay);
	// The writer has the value in the running config already: the file after
	// this write is remembered as read, so the hot reload does not read the
	// whole INI again for it (ReloadGate.h RememberOwnWrite).
	const config::FileStamp before = StampFiles(path);
	const bool written = WritePrivateProfileStringA(section, key, value, toOverlay ? overlay : path) != 0;
	const bool remembered = config::RememberOwnWrite(g_reloadGate, before, StampFiles(path));
	static UInt32 s_lines = 8;
	if (written && s_lines > 0) {
		--s_lines;
		OBVR_LOG("Config: OBVR wrote %s.%s=%s - %s", section, key, value,
		         remembered ? "remembered as read, no re-read for it"
		                    : "an outside change is pending, so the next check re-reads the file");
	}
	return written;
}

}  // namespace obvr
