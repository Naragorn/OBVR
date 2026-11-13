// Checks the INI parsing, above all the virtual-key code parser.
//
// That parser accepts two notations and rejects several kinds of nonsense, and
// every one of those paths ends in the same place: OBVR keeps a previous value
// instead of the one written in the file. A mistake here does not crash
// anything - the key simply does not work, which is the hardest kind of fault
// to attribute. So it gets checked without the game, like everything else that
// can be.
//
// Windows only: Config reads through GetPrivateProfileString.
//
// Each case writes its own file name. Windows caches the contents of the most
// recently used INI, so rewriting one path and reading it again can return the
// previous contents.

#include <cstdio>
#include <cstring>
#include <string>

#include "core/Config.h"
#include "core/ConfigKeys.h"
#include "vr/HandHud.h"
#include "platform/PluginPath.h"

namespace {

int g_failures = 0;

void CheckEqual(UInt32 actual, UInt32 expected, const char* what) {
	if (actual == expected) {
		std::printf("  ok    %s\n", what);
	} else {
		std::printf("  FAIL  %s: %u, expected %u\n", what, actual, expected);
		++g_failures;
	}
}

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

// Writes an INI next to the test executable and loads it.
//
// The path comes from BuildGamePath, which anchors on the running executable.
// In a test that is the test binary itself - SetPluginModule is never called
// here, so the plugin anchor is unavailable and Config falls back to the game
// anchor, which is exactly the fallback path worth exercising.
bool LoadFrom(const char* fileName, const char* contents, obvr::Config& out) {
	char path[512];
	if (!obvr::platform::BuildGamePath(fileName, path, sizeof(path))) {
		std::printf("  FAIL  cannot build a path for %s\n", fileName);
		++g_failures;
		return false;
	}

	if (contents != nullptr) {
		std::FILE* file = std::fopen(path, "wb");
		if (file == nullptr) {
			std::printf("  FAIL  cannot write %s\n", path);
			++g_failures;
			return false;
		}
		std::fputs(contents, file);
		std::fclose(file);
	} else {
		std::remove(path);
	}

	return out.Load(fileName);
}

void TestMissingFileKeepsDefaults() {
	std::printf("Missing file\n");

	obvr::Config config;
	LoadFrom("ConfigTestMissing.ini", nullptr, config);

	// A missing INI must leave every default standing rather than zeroing
	// anything out.
	CheckEqual(config.recenterKey, 0x2E, "RecenterKey stays at the default 0x2E");
	Check(config.tracker.source == obvr::vr::TrackerSource::Fixed,
	      "source stays at the default");
	Check(config.cameraHookEnabled, "the camera hook stays enabled");
}

// The dual pass's content sums (render/InterfaceRenderHook.cpp): every
// constant upload's floats read once more for the "Dual palettes" and "Dual
// bone range" lines - a diagnostic, off unless asked for (OBNG's samples of
// 2026-10-30: about 1.7 % of a pass with eighteen Walkers in view).
void TestDualContentSumsSetting() {
	obvr::Config defaults;
	Check(!defaults.dualContentSums, "the dual pass's content sums are off by default");
	obvr::Config on;
	Check(LoadFrom("config-content-sums-on.ini", "[Debug]\nDualContentSums=1\n", on) && on.dualContentSums,
	      "they can be switched on");
}

void TestStableDistantTreesSetting() {
	obvr::Config defaults;
	Check(defaults.stableDistantTrees, "distant trees faced by their place by default");
	obvr::Config off;
	Check(LoadFrom("config-trees-off.ini", "[Render]\nStableDistantTrees=0\n", off) && !off.stableDistantTrees,
	      "the distant trees' facing can be switched off");
	obvr::Config on;
	on.stableDistantTrees = false;
	Check(LoadFrom("config-trees-on.ini", "[Render]\nStableDistantTrees=1\n", on) && on.stableDistantTrees,
	      "and on again");
}

// The loading stage (vr/LoadingStage.h): OBVR's dome in place of SteamVR's grid while the
// game loads, on by default (Nadi, 2026-10-29).
void TestLoadingStageSetting() {
	obvr::Config defaults;
	Check(defaults.loadingStage, "the loading stage is on by default");
	obvr::Config off;
	Check(LoadFrom("config-stage-off.ini", "[Render]\nLoadingStage=0\n", off) && !off.loadingStage,
	      "the loading stage can be switched off");
	obvr::Config on;
	on.loadingStage = false;
	Check(LoadFrom("config-stage-on.ini", "[Render]\nLoadingStage=1\n", on) && on.loadingStage, "and on again");
}

// Single-pass stereo on by default since OBVR ships OBNG.dll (2026-11-12, single-pass-stereo-spec
// A7); without OBNG every frame falls back to the dual pass (render/ObngStereoLogic.h StereoPathFor).
void TestSinglePassStereoSetting() {
	obvr::Config defaults;
	Check(defaults.singlePassStereo, "single-pass stereo is on by default");
	obvr::Config off;
	Check(LoadFrom("config-single-off.ini", "[Render]\nSinglePassStereo=0\n", off) && !off.singlePassStereo,
	      "single-pass stereo can be switched off");
	obvr::Config on;
	on.singlePassStereo = false;
	Check(LoadFrom("config-single-on.ini", "[Render]\nSinglePassStereo=1\n", on) && on.singlePassStereo,
	      "and on again");
}

// The water's reflection left out for beta 6 (Nadi, 2026-10-22: "releasen wir die neue beta mit
// wasser reflexionen einfach aus erstmal"): the default is the static colour, the stable capture
// still one key away.
// [Render] WaterReflectionMapSize and [Debug] EyeDumpFrames: the engine's
// 256-texel reflection map made bigger (render/WaterReflectionMap.h) and the
// frames in a row after a mark (test/EyeDumpSeries.h).
void TestWaterReflectionMapSizeSetting() {
	obvr::Config defaults;
	Check(defaults.waterReflectionMapSize == 1024u,
	      "the reflection map defaults to 1024 texels a side");
	Check(defaults.eyeDumpFrames == 0u, "no frames in a row after a mark by default");
	obvr::Config engine;
	Check(LoadFrom("config-water-map-engine.ini", "[Render]\nWaterReflectionMapSize=0\n", engine) &&
	          engine.waterReflectionMapSize == 0u,
	      "0 asks for the engine's own 256");
	obvr::Config big;
	Check(LoadFrom("config-water-map-big.ini", "[Render]\nWaterReflectionMapSize=2048\n[Debug]\nEyeDumpFrames=30\n",
	               big) &&
	          big.waterReflectionMapSize == 2048u && big.eyeDumpFrames == 30u,
	      "2048 and thirty frames read");
	Check(defaults.eyeDumpDivisor == 0u && !defaults.layerDump,
	      "the mark's pictures at the default quarter, no layer picture by default");
	obvr::Config dump;
	Check(LoadFrom("config-mark-dump.ini", "[Debug]\nEyeDumpDivisor=1\nLayerDump=1\n", dump) &&
	          dump.eyeDumpDivisor == 1u && dump.layerDump,
	      "full-size pictures and the layer picture read");
}

void TestWaterReflectionModeSetting() {
	obvr::Config defaults;
	Check(defaults.waterReflectionMode == obvr::render::WaterReflectionMode::StaticColor,
	      "the water's reflection defaults to its static colour");
	obvr::Config capture;
	Check(LoadFrom("config-water-capture.ini", "[Render]\nWaterReflectionMode=2\n", capture) &&
	          capture.waterReflectionMode == obvr::render::WaterReflectionMode::CyclopeanCapture,
	      "the stable capture is one key away");
	obvr::Config absent;
	Check(LoadFrom("config-water-absent.ini", "[Render]\n", absent) &&
	          absent.waterReflectionMode == obvr::render::WaterReflectionMode::StaticColor,
	      "without the key the static colour stays");
	obvr::Config wrong;
	Check(LoadFrom("config-water-wrong.ini", "[Render]\nWaterReflectionMode=9\n", wrong) &&
	          wrong.waterReflectionMode == obvr::render::WaterReflectionMode::StaticColor,
	      "a mode past the last keeps the default");
	obvr::Config switchedOff;
	Check(LoadFrom("config-water-off.ini", "[Render]\nStableWaterReflections=0\nWaterReflectionMode=1\n",
	               switchedOff) &&
	          switchedOff.waterReflectionMode == obvr::render::WaterReflectionMode::Vanilla,
	      "StableWaterReflections=0 is the vanilla reflection whatever the mode");
}

void TestStableLeafSetting() {
	obvr::Config defaults;
	Check(defaults.stableLeafBillboards, "leaf correction defaults on");
	obvr::Config disabled;
	Check(LoadFrom("config-leaf-off.ini", "[Render]\nStableLeafBillboards=0\n", disabled), "load leaf off");
	Check(!disabled.stableLeafBillboards, "leaf correction can be disabled");
	obvr::Config enabled;
	enabled.stableLeafBillboards = false;
	Check(LoadFrom("config-leaf-on.ini", "[Render]\nStableLeafBillboards=1\n", enabled), "load leaf on");
	Check(enabled.stableLeafBillboards, "leaf correction can be enabled");
	obvr::Config absent;
	absent.stableLeafBillboards = false;
	Check(LoadFrom("config-leaf-absent.ini", "[Render]\n", absent), "load without leaf key");
	Check(!absent.stableLeafBillboards, "missing leaf key preserves value");
}

void TestKeyCodeNotations() {
	std::printf("RecenterKey notations\n");

	struct Case {
		const char* fileName;
		const char* value;
		UInt32 expected;
		const char* what;
	};

	// 46 == 0x2E. The hex forms matter because Microsoft's virtual-key table
	// lists the codes that way, so that is what people copy out of it.
	const Case cases[] = {
		{"ConfigTestDec.ini", "46", 46, "decimal 46"},
		{"ConfigTestHex.ini", "0x2E", 46, "hex 0x2E"},
		{"ConfigTestHexOdd.ini", "0X2e", 46, "hex with odd casing, 0X2e"},
		{"ConfigTestZero.ini", "0", 0, "0 disables recentering"},
		{"ConfigTestMax.ini", "255", 255, "255, the largest valid code"},
	};

	for (const Case& testCase : cases) {
		char contents[128];
		std::snprintf(contents, sizeof(contents), "[Head]\nRecenterKey=%s\n", testCase.value);

		obvr::Config config;
		LoadFrom(testCase.fileName, contents, config);
		CheckEqual(config.recenterKey, testCase.expected, testCase.what);
	}
}

void TestKeyCodeRejections() {
	std::printf("RecenterKey rejections\n");

	// Every rejected value has to leave the previous setting alone. Silently
	// falling back to zero would disable recentering over a typo.
	struct Case {
		const char* fileName;
		const char* value;
		const char* what;
	};

	const Case cases[] = {
		{"ConfigTestBig.ini", "999", "999 is out of range, previous key kept"},
		{"ConfigTestWord.ini", "Delete", "a key name is not a code, previous key kept"},
		{"ConfigTestJunk.ini", "0xZZ", "invalid hex digits, previous key kept"},
		{"ConfigTestMixed.ini", "12ab", "decimal with hex digits, previous key kept"},
	};

	for (const Case& testCase : cases) {
		char contents[128];
		std::snprintf(contents, sizeof(contents), "[Head]\nRecenterKey=%s\n", testCase.value);

		obvr::Config config;
		LoadFrom(testCase.fileName, contents, config);
		CheckEqual(config.recenterKey, 0x2E, testCase.what);
	}
}

void TestSourceParsing() {
	std::printf("Head.Source\n");

	obvr::Config openVr;
	LoadFrom("ConfigTestSourceVr.ini", "[Head]\nSource=OpenVR\n", openVr);
	Check(openVr.tracker.source == obvr::vr::TrackerSource::OpenVR,
	      "\"OpenVR\" is recognised regardless of casing");

	obvr::Config nonsense;
	LoadFrom("ConfigTestSourceBad.ini", "[Head]\nSource=teapot\n", nonsense);
	Check(nonsense.tracker.source == obvr::vr::TrackerSource::Fixed,
	      "an unknown source keeps the previous setting");
}

void TestAnglesAndFrames() {
	std::printf("Angles and frame counts\n");

	obvr::Config config;
	LoadFrom("ConfigTestValues.ini",
	         "[Camera]\nHookEnabled=0\n"
	         "[Head]\nFixedRoll=-12.5\n"
	         "[Debug]\nReloadEveryFrames=240\n",
	         config);

	Check(!config.cameraHookEnabled, "HookEnabled=0 disables the hook");
	Check(config.tracker.fixedRoll < -12.4f && config.tracker.fixedRoll > -12.6f,
	      "a negative float is read correctly");
	CheckEqual(config.reloadEveryFrames, 240, "ReloadEveryFrames is read");

	// Keys absent from the file must not disturb the ones that are present.
	CheckEqual(config.recenterKey, 0x2E, "an absent key keeps its default");
}

void TestLegacyWordBooleans() {
	std::printf("Legacy words written by toggle rows\n");

	obvr::Config enabled;
	LoadFrom("ConfigTestLegacyWordsOn.ini",
	         "[Render]\nCrosshairDynamic=follows\n"
	         "CrosshairOnlyWhenNeeded=when needed\n"
	         "CrosshairOnlyWhenNeeded3rdPerson=when needed\n"
	         "[Look]\nAimTurnOnShotOnly=for the shot\n",
	         enabled);
	Check(enabled.tracker.crosshairDynamic, "follows still loads as enabled");
	Check(enabled.tracker.crosshairOnlyWhenNeeded, "when needed still enables first-person gating");
	Check(enabled.tracker.crosshairOnlyWhenNeededThirdPerson,
	      "when needed still enables third-person gating");
	Check(enabled.aimTurnOnShotOnly, "for the shot still loads as enabled");

	obvr::Config disabled;
	LoadFrom("ConfigTestLegacyWordsOff.ini",
	         "[Render]\nCrosshairDynamic=fixed\n"
	         "CrosshairOnlyWhenNeeded=always\n"
	         "CrosshairOnlyWhenNeeded3rdPerson=always\n"
	         "[Look]\nAimTurnOnShotOnly=while aiming\n",
	         disabled);
	Check(!disabled.tracker.crosshairDynamic, "fixed still loads as disabled");
	Check(!disabled.tracker.crosshairOnlyWhenNeeded, "always disables first-person gating");
	Check(!disabled.tracker.crosshairOnlyWhenNeededThirdPerson,
	      "always disables third-person gating");
	Check(!disabled.aimTurnOnShotOnly, "while aiming still loads as disabled");

	// The repaired settings menu writes digits. Exercise that path through the
	// same four readers as well, so compatibility with the old values cannot
	// accidentally replace the normal representation.
	obvr::Config numeric;
	LoadFrom("ConfigTestLegacyWordsNumeric.ini",
	         "[Render]\nCrosshairDynamic=0\n"
	         "CrosshairOnlyWhenNeeded=1\n"
	         "CrosshairOnlyWhenNeeded3rdPerson=1\n"
	         "[Look]\nAimTurnOnShotOnly=0\n",
	         numeric);
	Check(!numeric.tracker.crosshairDynamic, "numeric 0 still disables dynamic depth");
	Check(numeric.tracker.crosshairOnlyWhenNeeded, "numeric 1 enables first-person gating");
	Check(numeric.tracker.crosshairOnlyWhenNeededThirdPerson,
	      "numeric 1 enables third-person gating");
	Check(!numeric.aimTurnOnShotOnly, "numeric 0 still disables shot-only turning");
}

void CheckNear(float actual, float expected, const char* what) {
	const float difference = actual - expected;
	if (difference < 0.01f && difference > -0.01f) {
		std::printf("  ok    %s\n", what);
	} else {
		std::printf("  FAIL  %s: %.3f, expected %.3f\n", what, static_cast<double>(actual),
		            static_cast<double>(expected));
		++g_failures;
	}
}

void TestThirdPersonAimVisualPercent() {
	std::printf("Third-person aim visual percentage\n");

	obvr::Config untouched;
	CheckNear(untouched.thirdPersonAimVisualPercent, 70.0f,
	          "the built-in visual starting point is 70 percent");
	Check(!untouched.thirdPersonBodyFollowsGazeUnarmed,
	      "the unarmed upper-body pose defaults off");
	Check(untouched.thirdPersonHeadFollowsGaze,
	      "the third-person head gaze defaults on");

	obvr::Config configured;
	LoadFrom("ConfigTestThirdPersonAim.ini",
	         "[Look]\nThirdPersonAimVisualPercent=55.5\n"
	         "ThirdPersonBodyFollowsGazeUnarmed=1\n"
	         "ThirdPersonHeadFollowsGaze=0\n",
	         configured);
	CheckNear(configured.thirdPersonAimVisualPercent, 55.5f,
	          "ThirdPersonAimVisualPercent is read from Look");
	Check(configured.thirdPersonBodyFollowsGazeUnarmed,
	      "ThirdPersonBodyFollowsGazeUnarmed is read from Look");
	Check(!configured.thirdPersonHeadFollowsGaze,
	      "ThirdPersonHeadFollowsGaze is read from Look");
}

void TestLiveMenuBackground() {
	std::printf("Live 3D pause-menu world\n");

	obvr::Config untouched;
	Check(untouched.tracker.liveMenuBackground,
	      "the built-in fallback enables the live 3D menu path");

	obvr::Config configured;
	LoadFrom("ConfigTestLiveMenu.ini", "[Render]\nLiveMenuBackground=0\n", configured);
	Check(!configured.tracker.liveMenuBackground,
	      "LiveMenuBackground=0 restores the held menu path");
}

void TestPersistentCrosshairCache() {
	std::printf("Persistent genuine crosshair cache\n");

	obvr::Config untouched;
	Check(untouched.tracker.crosshairPersistentCache,
	      "the genuine crosshair cache defaults on");

	obvr::Config configured;
	LoadFrom("ConfigTestCrosshairCache.ini",
	         "[Render]\nCrosshairPersistentCache=0\n", configured);
	Check(!configured.tracker.crosshairPersistentCache,
	      "CrosshairPersistentCache=0 restores the session-only copy");
}

void TestCrosshairCutoutDefault() {
	std::printf("Crosshair cutout default\n");

	obvr::Config untouched;
	CheckNear(untouched.tracker.crosshairSourceShare, 10.0f,
	          "the built-in cutout default covers ten percent of the HUD height");
}

void TestCrosshairTooltips() {
	std::printf("Crosshair tooltip controls\n");

	obvr::Config untouched;
	Check(untouched.tracker.crosshairTooltipsFirstPerson,
	      "first-person tooltips default on");
	Check(untouched.tracker.crosshairTooltipsThirdPerson,
	      "third-person tooltips default on");
	Check(!untouched.tracker.crosshairTooltipsAboveName,
	      "tooltips default to the depth crosshair");

	obvr::Config configured;
	LoadFrom("ConfigTestCrosshairTooltips.ini",
	         "[Render]\nCrosshairTooltips1stPerson=0\n"
	         "CrosshairTooltips3rdPerson=0\nCrosshairTooltipsAboveName=1\n",
	         configured);
	Check(!configured.tracker.crosshairTooltipsFirstPerson,
	      "first-person tooltips can be disabled independently");
	Check(!configured.tracker.crosshairTooltipsThirdPerson,
	      "third-person tooltips can be disabled independently");
	Check(configured.tracker.crosshairTooltipsAboveName,
	      "tooltips can be moved above the target name");
}

void TestMirrorMenusToMonitor() {
	std::printf("Menus mirrored onto the monitor\n");

	obvr::Config untouched;
	Check(untouched.tracker.mirrorMenusToMonitor, "the monitor copy of menus defaults on");

	obvr::Config configured;
	LoadFrom("ConfigTestMirrorMenus.ini", "[Render]\nMirrorMenusToMonitor=0\n", configured);
	Check(!configured.tracker.mirrorMenusToMonitor,
	      "MirrorMenusToMonitor=0 leaves the monitor showing the world alone");
}

void TestMonitorMenusOnly() {
	std::printf("Only the menus on the monitor\n");

	obvr::Config untouched;
	Check(!untouched.tracker.monitorMenusOnly, "the game picture stays on the monitor by default");

	obvr::Config configured;
	LoadFrom("ConfigTestMonitorMenusOnly.ini", "[Render]\nMonitorMenusOnly=1\n", configured);
	Check(configured.tracker.monitorMenusOnly, "MonitorMenusOnly=1 asks for a black monitor with the menus");
}

void TestUnpausedMenus() {
	std::printf("Unpaused menus\n");

	obvr::Config untouched;
	Check(!untouched.tracker.unpausedMenus, "the world pauses behind menus by default");

	obvr::Config configured;
	LoadFrom("ConfigTestUnpausedMenus.ini", "[Render]\nUnpausedMenus=1\n", configured);
	Check(configured.tracker.unpausedMenus, "UnpausedMenus=1 keeps it running");
}

void TestHandTracking() {
	std::printf("Hand tracking\n");

	obvr::Config untouched;
	Check(!untouched.fullVrMode, "the hand-tracked mode is off by default");
	Check(untouched.nativeOnboardingPrototype,
	      "the native MenuQue menus are selected by default");
	// The slice out of people is off by default since 2026-11-10 (it read as
	// the blade jumping through them); the INI still turns it on.
	Check(untouched.hands.bodyLetGoDegrees == 0.0f, "the blade slices out of no one by default");
	obvr::Config slicing;
	LoadFrom("ConfigTestSliceOut.ini", "[Hands]\nBodyLetGoDegrees=45\n", slicing);
	Check(slicing.hands.bodyLetGoDegrees == 45.0f, "Hands.BodyLetGoDegrees=45 slices out at 45 degrees");

	obvr::Config legacyMenus;
	LoadFrom("ConfigTestLegacyMenus.ini", "[Onboarding]\nNativePrototype=0\n", legacyMenus);
	Check(!legacyMenus.nativeOnboardingPrototype,
	      "Onboarding.NativePrototype=0 selects the existing overlay menus");
	obvr::Config nativeMenus;
	LoadFrom("ConfigTestNativeMenus.ini", "[Onboarding]\nNativePrototype=1\n", nativeMenus);
	Check(nativeMenus.nativeOnboardingPrototype,
	      "Onboarding.NativePrototype=1 selects the MenuQue menus");

	obvr::Config configured;
	LoadFrom("ConfigTestHands.ini", "[Hands]\nEnabled=1\n", configured);
	Check(configured.fullVrMode, "Hands.Enabled=1 switches it on");

	Check(untouched.hands.hideWornCuirass, "the worn cuirass's shapes are hidden in first person by default");
	obvr::Config cuirassShown;
	LoadFrom("ConfigTestCuirass.ini", "[Hands]\nHideWornCuirass=0\n", cuirassShown);
	Check(!cuirassShown.hands.hideWornCuirass, "Hands.HideWornCuirass=0 leaves the cuirass to the hide list");

	Check(untouched.hands.spellAim.enabled && untouched.hands.spellAim.raiseBelowEyesMetres == 0.45f &&
	          untouched.hands.spellAim.raiseAheadMetres == 0.15f,
	      "the spell leaves the casting hand by default, raised from 45 cm under the eyes and 15 cm ahead");
	obvr::Config spellAim;
	LoadFrom("ConfigTestSpellAim.ini",
	         "[Hands]\nSpellFromCastingHand=0\nSpellRaiseBelowEyesMetres=0.3\nSpellRaiseAheadMetres=0.25\n",
	         spellAim);
	Check(!spellAim.hands.spellAim.enabled && spellAim.hands.spellAim.raiseBelowEyesMetres == 0.3f &&
	          spellAim.hands.spellAim.raiseAheadMetres == 0.25f,
	      "Hands.SpellFromCastingHand and the two raise limits are read");
	Check(untouched.hands.sneakIndicator == obvr::vr::SneakIndicator::Vignette && untouched.hands.sneakVignetteStrength == 0.8f,
	      "sneaking shows as the vignette at 0.8 by default");
	obvr::Config sneakEye;
	LoadFrom("ConfigTestSneak.ini", "[Hands]\nSneakIndicator=eye\nSneakVignetteStrength=0.5\n", sneakEye);
	Check(sneakEye.hands.sneakIndicator == obvr::vr::SneakIndicator::Eye && sneakEye.hands.sneakVignetteStrength == 0.5f,
	      "Hands.SneakIndicator=eye and the strength are read");
	Check(untouched.hands.sneakEyeSize == 4.5f, "the sneak eye is 4.5 times a crosshair at its distance by default");
	obvr::Config sneakBig;
	LoadFrom("ConfigTestSneakBig.ini", "[Hands]\nSneakEyeSize=6\n", sneakBig);
	Check(sneakBig.hands.sneakEyeSize == 6.0f, "Hands.SneakEyeSize is read");
	Check(untouched.body.softPush && untouched.body.softPushReachUnits == 13.0f && untouched.body.softPushMostUnits == 6.0f,
	      "the soft push on by default, reach 13 units, at most 6");
	obvr::Config softPush;
	LoadFrom("ConfigTestSoftPush.ini", "[Body]\nSoftPush=0\nSoftPushReach=10\nSoftPushMost=3\nKeepInView=0\n", softPush);
	Check(!softPush.body.softPush && softPush.body.softPushReachUnits == 10.0f && softPush.body.softPushMostUnits == 3.0f,
	      "Body.SoftPush, SoftPushReach and SoftPushMost are read");
	Check(untouched.body.keepInView && !softPush.body.keepInView, "Body.KeepInView on by default, and read");
	obvr::Config noLean;
	LoadFrom("ConfigTestNoLean.ini", "[Body]\nIKLean=0\n", noLean);
	Check(untouched.body.ikLean && !noLean.body.ikLean, "Body.IKLean on by default, and read");
	obvr::Config headTrack;
	LoadFrom("ConfigTestHeadTrack.ini", "[Body]\nPlayerHeadTracking=1\n", headTrack);
	Check(!untouched.body.playerHeadTracking && headTrack.body.playerHeadTracking,
	      "Body.PlayerHeadTracking off by default (the engine's head tracking stopped), and read");
	Check(untouched.hands.menuHoldSeconds == 1.25f && untouched.hands.menuRingSeconds == 0.0f &&
	          untouched.hands.menuTapSeconds == 0.25f,
	      "the left B held 1.25 s is the pause menu, its ring from the press, a tap under 0.25 s, by default");
	Check(untouched.hands.kick.holdMaxSeconds == 0.8f && untouched.hands.kick.outSeconds == 0.18f &&
	          untouched.hands.kick.backSeconds == 0.3f,
	      "the kick held up to 0.8 s, out in 0.18 s, back in 0.3 s by default");
	Check(untouched.hands.flyCam.speedUnits == 350.0f && untouched.hands.flyCam.turnDegrees == 120.0f,
	      "the fly camera flies 350 units a second and turns 120 degrees a second by default");
	obvr::Config flyCam;
	LoadFrom("ConfigTestFlyCam.ini", "[Hands]\nFlyCamSpeed=500\nFlyCamTurnDegrees=60\n", flyCam);
	Check(flyCam.hands.flyCam.speedUnits == 500.0f && flyCam.hands.flyCam.turnDegrees == 60.0f,
	      "Hands.FlyCamSpeed and FlyCamTurnDegrees are read");
	Check(untouched.hands.roomFollow.enabled && untouched.hands.roomFollow.maxStepUnits == 20.0f &&
	          untouched.hands.roomFollow.startUnits == 2.0f,
	      "the body following the head through the room: on by default, 20 units a frame at most");
	obvr::Config room;
	LoadFrom("ConfigTestRoom.ini", "[Hands]\nRoomScaleBody=0\nRoomScaleMaxStep=12\nRoomScaleStart=0.5\n", room);
	Check(!room.hands.roomFollow.enabled && room.hands.roomFollow.maxStepUnits == 12.0f &&
	          room.hands.roomFollow.startUnits == 0.5f,
	      "Hands.RoomScaleBody, RoomScaleMaxStep and RoomScaleStart are read");
	Check(untouched.hands.analogWalk.enabled && untouched.hands.analogWalk.runFrom == 0.65f &&
	          untouched.hands.analogWalk.deadZone == 0.15f,
	      "the analog walk: on by default, running from 0.65 of the push, its own dead zone 0.15");
	obvr::Config analog;
	LoadFrom("ConfigTestAnalog.ini", "[Hands]\nAnalogWalk=0\nAnalogRunFrom=0.8\nAnalogDeadZone=0.1\n", analog);
	Check(!analog.hands.analogWalk.enabled && analog.hands.analogWalk.runFrom == 0.8f &&
	          analog.hands.analogWalk.deadZone == 0.1f,
	      "Hands.AnalogWalk, AnalogRunFrom and AnalogDeadZone are read");
	Check(untouched.hands.analogWalk.sprintFactor == 1.4f && untouched.hands.analogWalk.sprintFatiguePerSecond == 10.0f,
	      "the sprint: 1.4 times the run, 10 fatigue a second by default");
	Check(untouched.hands.dismember.enabled, "the head comes off by default (tried in the game 2026-10-31)");
	Check(untouched.hands.torchBlocks, "the raised torch blocks by default");
	Check(untouched.hands.handoverReachMetres == 0.0f && untouched.hands.handoverThingMetres == 0.15f,
	      "the handover off by default (docs/release-hardening.md section 3: the sword's ends the game)");
	obvr::Config handover;
	LoadFrom("ConfigTestHandover.ini", "[Hands]\nHandoverReachMetres=0.12\n", handover);
	Check(handover.hands.handoverReachMetres == 0.12f, "Hands.HandoverReachMetres is read - set, the handover is on");
	Check(untouched.hands.pommel.enabled && untouched.hands.pommel.speed == 3.0f && untouched.hands.pommel.hardSpeed == 6.0f,
	      "the pommel strike on by default, at the one speed: 3 m/s, hard from 6");
	Check(untouched.hands.impact.staggerSpeed == 3.0f && untouched.hands.impact.knockdownSpeed == 6.0f &&
	          !untouched.hands.impact.vanillaKnockdown && !untouched.hands.shove.vanillaKnockdown &&
	          untouched.hands.shove.speed == 3.0f && untouched.hands.shove.hardSpeed == 6.0f &&
	          untouched.hands.halfSword.hiltHardSpeed == 6.0f && untouched.hands.throwHit.staggerSpeed == 3.0f &&
	          untouched.hands.throwHit.knockSpeed == 6.0f && !untouched.hands.torchKnockdownPowerOnly,
	      "one speed for every blow by default: a stagger from 3 m/s, a knockdown from 6 (vanilla's chance off, as in Blade & Sorcery), the torch by its speed");
	obvr::Config impact;
	LoadFrom("ConfigTestImpact.ini",
	         "[Hands]\nStaggerSpeed=2.5\nKnockdownSpeed=5\nVanillaKnockdown=1\nShoveSpeed=9\nPommelHardSpeed=9\n", impact);
	Check(impact.hands.shove.speed == 2.5f && impact.hands.shove.hardSpeed == 5.0f && impact.hands.shove.vanillaKnockdown &&
	          impact.hands.pommel.speed == 2.5f && impact.hands.pommel.hardSpeed == 5.0f &&
	          impact.hands.shieldBash.speed == 2.5f && impact.hands.shieldBash.hardSpeed == 5.0f &&
	          impact.hands.halfSword.hiltHardSpeed == 5.0f && impact.hands.throwHit.knockSpeed == 5.0f,
	      "Hands.StaggerSpeed, KnockdownSpeed and VanillaKnockdown reach every way of striking; the old per-way keys are gone");
	Check(untouched.hands.halfSword.enabled && untouched.hands.halfSword.fromUnits == 12.0f &&
	          untouched.hands.halfSword.tipInsetUnits == 8.0f && untouched.hands.halfSword.rightHand,
	      "half-swording on by default, the blade from 12 units above the right palm to 8 short of the tip");
	obvr::Config half;
	LoadFrom("ConfigTestHalfSword.ini", "[Hands]\nHalfSwording=0\nHalfSwordFromUnits=15\nHalfSwordTipInsetUnits=5\nHalfSwordRightHand=0\nHalfSwordHardSpeed=9\nKickLookShare=0.5\nKickAimUnits=80\n", half);
	Check(!half.hands.halfSword.enabled && half.hands.halfSword.fromUnits == 15.0f &&
	          half.hands.halfSword.tipInsetUnits == 5.0f && !half.hands.halfSword.rightHand &&
	          half.hands.kick.lookShare == 0.5f && half.hands.kick.aimUnits == 80.0f && untouched.hands.kick.aimUnits == 110.0f,
	      "Hands.HalfSwording, HalfSwordFromUnits, HalfSwordTipInsetUnits, HalfSwordRightHand, KickLookShare and KickAimUnits (110 untouched) are read");
	obvr::Config choke;
	LoadFrom("ConfigTestChokeUp.ini", "[Hands]\nChokeUp=0\n", choke);
	Check(untouched.hands.halfSword.chokeUp && !choke.hands.halfSword.chokeUp, "Hands.ChokeUp on by default, 0 is read");
	obvr::Config animView;
	LoadFrom("ConfigTestAnimView.ini", "[Look]\nFirstPersonAnimations=0\n", animView);
	Check(untouched.look.firstPersonAnimations && !animView.look.firstPersonAnimations,
	      "Look.FirstPersonAnimations on by default, 0 is read");
	obvr::Config animVignette;
	LoadFrom("ConfigTestAnimVignette.ini", "[Look]\nAnimationVignette=0\nAnimationVignetteStrength=0.4\n",
	         animVignette);
	Check(untouched.look.animationVignette && untouched.look.animationVignetteStrength == 0.7f,
	      "Look.AnimationVignette on by default at 0.7");
	Check(!animVignette.look.animationVignette && animVignette.look.animationVignetteStrength == 0.4f,
	      "Look.AnimationVignette=0 and its strength are read");
	Check(untouched.hands.shieldBash.enabled && untouched.hands.shieldBash.speed == 3.0f,
	      "the shield bash on by default, from the one stagger speed");
	obvr::Config bash;
	LoadFrom("ConfigTestBash.ini", "[Hands]\nShieldBash=0\nShieldBashSpeed=2\nShieldBashHardSpeed=4\nShieldBashFatigue=9\n", bash);
	Check(!bash.hands.shieldBash.enabled && bash.hands.shieldBash.fatigue == 9.0f,
	      "Hands.ShieldBash and ShieldBashFatigue are read");
	obvr::Config sprint;
	LoadFrom("ConfigTestSprint.ini", "[Hands]\nSprintFactor=1.2\nSprintFatigue=5\n", sprint);
	Check(sprint.hands.analogWalk.sprintFactor == 1.2f && sprint.hands.analogWalk.sprintFatiguePerSecond == 5.0f,
	      "Hands.SprintFactor and SprintFatigue are read");
	obvr::Config kickHeld;
	LoadFrom("ConfigTestKickHeld.ini", "[Hands]\nKickHoldMaxSeconds=1.5\nMenuHoldSeconds=2\nMenuRingSeconds=0.5\nMenuTapSeconds=0.4\n",
	         kickHeld);
	Check(kickHeld.hands.kick.holdMaxSeconds == 1.5f && kickHeld.hands.menuHoldSeconds == 2.0f &&
	          kickHeld.hands.menuRingSeconds == 0.5f && kickHeld.hands.menuTapSeconds == 0.4f,
	      "Hands.KickHoldMaxSeconds, MenuHoldSeconds and MenuRingSeconds are read");
	obvr::Config sneakOdd;
	LoadFrom("ConfigTestSneakOdd.ini", "[Hands]\nSneakIndicator=crosshair\n", sneakOdd);
	Check(sneakOdd.hands.sneakIndicator == obvr::vr::SneakIndicator::Vignette, "an unknown word keeps the vignette");

	Check(untouched.hands.wristHudWidth == 0.35f && untouched.hands.wristMenuWidth == 0.70f,
	      "the wrist HUD is 35 cm wide and the wrist menu 70 cm by default");
	Check(untouched.hands.menuOnRight, "and the menu hangs on the right wrist");
	Check(untouched.hands.pokeTipForward == 0.08f && untouched.hands.poke.hover == 0.10f &&
	          untouched.hands.poke.press == 0.015f && untouched.hands.poke.release == 0.04f &&
	          untouched.hands.poke.through == 0.06f,
	      "the poke's reach and distances have their defaults");

	obvr::Config wrists;
	LoadFrom("ConfigTestWrists.ini",
	         "[Hands]\nWristHudWidth=0.5\nWristMenuWidth=1.0\nMenuOnRight=0\n"
	         "PokeTipForward=0.1\nPokeHover=0.2\nPokePress=0.02\nPokeRelease=0.05\n"
	         "PokeThrough=0.09\n",
	         wrists);
	Check(wrists.hands.wristHudWidth == 0.5f && wrists.hands.wristMenuWidth == 1.0f,
	      "the wrist widths are read");
	Check(!wrists.hands.menuOnRight, "MenuOnRight=0 is read");
	Check(wrists.hands.pokeTipForward == 0.1f && wrists.hands.poke.hover == 0.2f &&
	          wrists.hands.poke.press == 0.02f && wrists.hands.poke.release == 0.05f &&
	          wrists.hands.poke.through == 0.09f,
	      "and so are the poke's five numbers");

	Check(untouched.hands.hideArms, "the arms are hidden by default");
	Check(untouched.hands.firstPersonAfterLoad, "first person once after a load by default");
	Check(untouched.hands.firstPersonAfterKnockdown, "first person again after a knockdown by default");
	Check(untouched.look.chargenView && untouched.look.chargenDistanceMetres > 0.89f &&
	          untouched.look.chargenDistanceMetres < 0.91f,
	      "the race menu's view before the face by default, 0.9 m off");
	Check(untouched.look.chargenFacesHead, "the race menu's view laid before the head at the opening by default");
	obvr::Config chargen;
	LoadFrom("ConfigTestChargen.ini",
	         "[Look]\nChargenView=0\nChargenDistanceMetres=1.5\nChargenFacesHead=0\n[Hands]\nFirstPersonAfterLoad=0\n"
	         "FirstPersonAfterKnockdown=0\n",
	         chargen);
	Check(!chargen.look.chargenView && chargen.look.chargenDistanceMetres > 1.49f &&
	          chargen.look.chargenDistanceMetres < 1.51f && !chargen.hands.firstPersonAfterLoad &&
	          !chargen.hands.firstPersonAfterKnockdown && !chargen.look.chargenFacesHead,
	      "the race menu's view, its distance, its head switch, the load's and the knockdown's switch are read");
	Check(std::strcmp(untouched.hands.hideNodes, "Arms") == 0,
	      "and the shape to hide is Arms");
	obvr::Config body;
	LoadFrom("ConfigTestBody.ini",
	         "[Hands]\nHideArms=0\nHideFirstPersonNodes=UpperBody, Hand\n",
	         body);
	Check(!body.hands.hideArms, "the switch is read");
	Check(std::strcmp(body.hands.hideNodes, "UpperBody, Hand") == 0,
	      "and the list is read as written");
	obvr::Config cleared;
	LoadFrom("ConfigTestBodyCleared.ini", "[Hands]\nHideFirstPersonNodes=\n", cleared);
	Check(cleared.hands.hideNodes[0] == '\0', "an empty list clears the default");

	Check(untouched.hands.pinHands && std::strcmp(untouched.hands.rightHandBone, "Bip01 R Hand") == 0 &&
	          std::strcmp(untouched.hands.leftHandBone, "Bip01 L Hand") == 0,
	      "the hands are pinned to the Bip01 hand bones by default");
	Check(untouched.hands.rightHandYaw == 90.0f && untouched.hands.leftHandYaw == 90.0f &&
	          untouched.hands.rightHandRoll == 0.0f && untouched.hands.rightHandPitch == 0.0f,
	      "with ninety degrees of yaw and nothing else");
	obvr::Config pinned;
	LoadFrom("ConfigTestPin.ini",
	         "[Hands]\nPinHands=0\nRightHandBone=Bip01 R Finger1\nLeftHandBone=Bip01 L Finger1\n"
	         "RightHandRoll=15\nRightHandPitch=-5\nRightHandYaw=80\nLeftHandRoll=-15\n"
	         "LeftHandPitch=5\nLeftHandYaw=100\n",
	         pinned);
	Check(!pinned.hands.pinHands, "PinHands=0 is read");
	Check(std::strcmp(pinned.hands.rightHandBone, "Bip01 R Finger1") == 0 &&
	          std::strcmp(pinned.hands.leftHandBone, "Bip01 L Finger1") == 0,
	      "and the bone names");
	Check(pinned.hands.rightHandRoll == 15.0f && pinned.hands.rightHandPitch == -5.0f &&
	          pinned.hands.rightHandYaw == 80.0f && pinned.hands.leftHandRoll == -15.0f &&
	          pinned.hands.leftHandPitch == 5.0f && pinned.hands.leftHandYaw == 100.0f,
	      "and the six angles");

	Check(untouched.hands.motionHits && untouched.hands.hitBoundFactor == 0.7f &&
	          untouched.hands.hitPadUnits == 8.0f,
	      "strikes by motion are on by default, within 0.7 of the bound plus eight units");
	obvr::Config strikes;
	LoadFrom("ConfigTestStrikes.ini", "[Hands]\nMotionHits=0\nHitBoundFactor=0.5\nHitPadUnits=12\n",
	         strikes);
	Check(!strikes.hands.motionHits, "MotionHits=0 is read");
	Check(strikes.hands.hitBoundFactor == 0.5f && strikes.hands.hitPadUnits == 12.0f,
	      "and the factor and the pad");

	Check(!untouched.hands.controllerMenus && untouched.hands.laserBeam,
	      "the controllers stay off the menus by default (under construction); the beam is on");
	obvr::Config menus;
	LoadFrom("ConfigTestControllerMenus.ini", "[Hands]\nControllerMenus=1\nLaserBeam=0\n", menus);
	Check(menus.hands.controllerMenus && !menus.hands.laserBeam,
	      "ControllerMenus=1 switches them on and LaserBeam=0 the beam off");
}

void TestNativeMenuLifecycleProbe() {
	std::printf("Native menu lifecycle probe\n");

	obvr::Config untouched;
	Check(!untouched.nativeMenuLifecycleProbe,
	      "the native lifecycle runtime probe defaults off");

	obvr::Config configured;
	LoadFrom("ConfigTestNativeMenuLifecycleProbe.ini",
	         "[Debug]\nNativeMenuLifecycleProbe=1\n", configured);
	Check(configured.nativeMenuLifecycleProbe,
	      "NativeMenuLifecycleProbe=1 enables the controlled diagnostic seam");
}

void TestScriptTimeTrace() {
	std::printf("Script clock\n");

	obvr::Config untouched;
	Check(!untouched.scriptTimeTrace && untouched.scriptTimeTraceMs == 4.0f,
	      "the script clock defaults off, with a 4 ms threshold");

	obvr::Config configured;
	LoadFrom("ConfigTestScriptTimeTrace.ini", "[Debug]\nScriptTimeTrace=1\nScriptTimeTraceMs=2.5\n",
	         configured);
	Check(configured.scriptTimeTrace && configured.scriptTimeTraceMs == 2.5f,
	      "ScriptTimeTrace=1 and ScriptTimeTraceMs=2.5 are read");

	obvr::Config clamped;
	LoadFrom("ConfigTestScriptTimeTraceClamp.ini", "[Debug]\nScriptTimeTraceMs=0\n", clamped);
	Check(clamped.scriptTimeTraceMs == 0.1f, "ScriptTimeTraceMs=0 is held to the 0.1 ms floor");
}

void TestLookRanges() {
	std::printf("The vertical look ranges, old spelling and new\n");

	// VerticalLookRange was one value for both directions before a headset
	// showed that a range sized for looking up stops at the hips going down.
	// An INI written before the split has to keep meaning what it says: the
	// alternative is that it silently reverts to the defaults, and a setting
	// that quietly stops applying is worse than one that fails loudly.
	//
	// Each case needs its own file name. Windows caches the contents of the
	// most recently read INI, so rewriting a path and reading it again can
	// hand back the previous contents.
	obvr::Config legacy;
	LoadFrom("ConfigTestLegacyRange.ini", "[Look]\nVerticalLookRange=25.0\n", legacy);
	CheckNear(legacy.look.verticalLookUpRange, 25.0f, "the old single range still sets up");
	CheckNear(legacy.look.verticalLookDownRange, 25.0f, "and down as well");

	// Both spellings at once: the specific key wins over the general one,
	// which is what lets someone add a down range to an existing file without
	// having to rewrite the line already there.
	obvr::Config mixed;
	LoadFrom("ConfigTestMixedRange.ini",
	         "[Look]\nVerticalLookRange=25.0\nVerticalLookDownRange=99.0\n", mixed);
	CheckNear(mixed.look.verticalLookUpRange, 25.0f, "the old key still supplies the missing half");
	CheckNear(mixed.look.verticalLookDownRange, 99.0f, "and the specific key wins where both apply");

	// The shipped INI has rendering on, because it works. The built-in
	// default is off, and the two are not in conflict: the built-in default
	// is what applies when there is no INI at all, which means a broken
	// installation rather than a configured one. Claiming the VR scene from a
	// broken install is the harder state to diagnose - the headset goes black
	// and the reason is a file that is not there to say so.
	obvr::Config untouched;
	Check(!untouched.tracker.renderToHeadset,
	      "with no INI at all, rendering stays off rather than claiming the scene");
	CheckNear(untouched.tracker.eyeSeparationScale, 1.0f,
	          "and the eye separation scale defaults to the geometric truth of 1");
	Check(!untouched.tracker.monitorVsync,
	      "the monitor window drops the game's vsync while OBVR drives the headset by default");

	// The crosshair defaults, as they have stood since 0.1.1: the depth
	// crosshair on, in third person as well, and in both views only while it
	// is of use. The shipped INI says the same, so a file with the keys
	// deleted behaves like the file as shipped.
	Check(untouched.tracker.crosshair, "the depth crosshair defaults on");
	Check(untouched.tracker.crosshairInThirdPerson, "and is shown in third person by default");
	Check(untouched.tracker.crosshairOnlyWhenNeeded,
	      "first person shows it only while it is of use by default");
	Check(untouched.tracker.crosshairOnlyWhenNeededThirdPerson,
	      "and so does third person");

	obvr::Config split;
	LoadFrom("ConfigTestSplitRange.ini",
	         "[Look]\nVerticalLookUpRange=40.0\nVerticalLookDownRange=140.0\n"
	         "[Head]\nHeadMovementScale=1.5\n"
	         "[Render]\nEnabled=1\nEyeSeparationScale=1.25\nMonitorVsync=1\n",
	         split);
	Check(split.tracker.renderToHeadset, "and can be switched on from the [Render] section");
	Check(split.tracker.monitorVsync, "MonitorVsync=1 keeps the game's vsync, read from [Render]");
	CheckNear(split.tracker.eyeSeparationScale, 1.25f,
	          "EyeSeparationScale is read from the [Render] section");
	CheckNear(split.look.verticalLookUpRange, 40.0f, "the up range is read on its own");
	CheckNear(split.look.verticalLookDownRange, 140.0f, "the down range is read on its own");
	CheckNear(split.tracker.movementScale, 1.5f, "HeadMovementScale is read");
	CheckNear(split.tracker.EffectiveUnitsPerMetre(), 69.99125f * 1.5f,
	          "and multiplies into the effective units per metre");
}

}  // namespace

void TestHeadLook() {
	std::printf("Head look\n");
	obvr::Config on;
	Check(LoadFrom("ConfigTestHeadLookOn.ini", "[Look]\nBlockVerticalLook=0\nAimAtSource=1\n", on),
	      "load head look on");
	Check(on.tracker.headLook, "head look is on by default");
	Check(on.look.blockVerticalLook, "with Aim at source the vertical look stays blocked");
	obvr::Config off;
	Check(LoadFrom("ConfigTestHeadLookOff.ini",
	               "[Head]\nHeadLook=0\n[Look]\nBlockVerticalLook=1\nAimAtSource=1\n", off),
	      "load head look off");
	Check(!off.tracker.headLook, "HeadLook=0 is read");
	Check(!off.look.blockVerticalLook,
	      "with the headset steering nothing the vertical look is unblocked, whatever Aim at source says");

	std::printf("The headset turning the view only\n");
	Check(!on.headTurnsViewOnly, "off by default");
	obvr::Config only;
	LoadFrom("ConfigTestHeadViewOnly.ini",
	         "[Look]\nHeadTurnsViewOnly=1\nAimFollowsGaze=1\nAimAtSource=1\nBlockVerticalLook=1\n", only);
	Check(only.headTurnsViewOnly && !only.aimFollowsGaze && !only.aimAtSource && !only.look.blockVerticalLook,
	      "on, seated: the aim follows no gaze, no source aim, the mouse tilts - whatever the three say");
	obvr::Config fullVr;
	LoadFrom("ConfigTestHeadViewOnlyFullVr.ini",
	         "[Look]\nHeadTurnsViewOnly=1\nAimFollowsGaze=1\nAimAtSource=1\n[Hands]\nEnabled=1\n", fullVr);
	Check(fullVr.aimFollowsGaze && fullVr.aimAtSource, "in Full VR the three are left be");
	obvr::Config cinema;
	LoadFrom("ConfigTestHeadViewOnlyCinema.ini",
	         "[Head]\nHeadLook=0\n[Look]\nHeadTurnsViewOnly=1\nAimFollowsGaze=1\n", cinema);
	Check(cinema.aimFollowsGaze, "with the headset steering nothing it does nothing");
}

// The key table (core/ConfigKeys.h): every row lands in a member of its
// kind inside Config, no key is listed twice but the legacy
// VerticalLookRange pair, and an INI that sets every key away from its
// default moves every member - a row whose member stays is a row that reads
// into the wrong place, unless a step of the read puts it back on purpose
// (those few are named here with their reason).
void TestKeyTable() {
	std::printf("the key table\n");
	UInt32 count = 0;
	const obvr::config::IniKey* rows = obvr::config::ConfigKeys(count);
	Check(count > 600, "more than 600 rows");

	obvr::Config defaults;
	const char* base = reinterpret_cast<const char*>(&defaults);
	UInt32 outside = 0;
	UInt32 wrongSize = 0;
	UInt32 duplicates = 0;
	UInt32 steps = 0;
	for (UInt32 i = 0; i < count; ++i) {
		const obvr::config::IniKey& row = rows[i];
		if (row.kind == obvr::config::IniKind::Step) {
			++steps;
			continue;
		}
		const char* at = static_cast<const char*>(row.at(defaults));
		if (at < base || at + row.size > base + sizeof(obvr::Config)) {
			++outside;
		}
		UInt32 wanted = 0;
		switch (row.kind) {
			case obvr::config::IniKind::Float:
			case obvr::config::IniKind::UInt:
			case obvr::config::IniKind::Int:
			case obvr::config::IniKind::KeyCode:
			case obvr::config::IniKind::HexColor: wanted = 4; break;
			case obvr::config::IniKind::Bool:
			case obvr::config::IniKind::AnchorIsWorld:
			case obvr::config::IniKind::MenusInWorld:
			case obvr::config::IniKind::LegacyWordBool: wanted = 1; break;
			case obvr::config::IniKind::Text: wanted = row.size > 1 ? row.size : 0; break;
			case obvr::config::IniKind::Step: break;
		}
		if (row.size != wanted) {
			++wrongSize;
			std::printf("        %s.%s has size %u, its kind wants %u\n", row.section, row.key, row.size, wanted);
		}
		for (UInt32 j = 0; j < i; ++j) {
			if (rows[j].kind != obvr::config::IniKind::Step && std::strcmp(rows[j].section, row.section) == 0 &&
			    std::strcmp(rows[j].key, row.key) == 0 && std::strcmp(row.key, "VerticalLookRange") != 0) {
				++duplicates;
				std::printf("        %s.%s is listed twice\n", row.section, row.key);
			}
		}
	}
	Check(outside == 0, "every row's member lies inside Config");
	Check(wrongSize == 0, "every row's member has its kind's size");
	Check(duplicates == 0, "no key is listed twice but VerticalLookRange");
	Check(steps >= 20, "the steps are rows too");

	// Every key set away from its default, section by section (Windows reads
	// a section's first block only).
	std::string sections[16];
	std::string bodies[16];
	UInt32 sectionCount = 0;
	for (UInt32 i = 0; i < count; ++i) {
		const obvr::config::IniKey& row = rows[i];
		if (row.kind == obvr::config::IniKind::Step) {
			continue;
		}
		UInt32 s = 0;
		while (s < sectionCount && sections[s] != row.section) {
			++s;
		}
		if (s == sectionCount) {
			sections[sectionCount++] = row.section;
		}
		const void* at = row.at(defaults);
		char line[256];
		switch (row.kind) {
			case obvr::config::IniKind::Float:
				std::snprintf(line, sizeof(line), "%s=%.4f\n", row.key,
				              static_cast<double>(*static_cast<const float*>(at) + 0.25f));
				break;
			case obvr::config::IniKind::Bool:
				std::snprintf(line, sizeof(line), "%s=%d\n", row.key, *static_cast<const bool*>(at) ? 0 : 1);
				break;
			case obvr::config::IniKind::UInt:
				std::snprintf(line, sizeof(line), "%s=%u\n", row.key, *static_cast<const UInt32*>(at) + 1);
				break;
			case obvr::config::IniKind::Int:
				std::snprintf(line, sizeof(line), "%s=%d\n", row.key, *static_cast<const int*>(at) + 1);
				break;
			case obvr::config::IniKind::KeyCode:
				std::snprintf(line, sizeof(line), "%s=0x%02X\n", row.key, (*static_cast<const UInt32*>(at) + 1) & 0xFF);
				break;
			case obvr::config::IniKind::HexColor:
				std::snprintf(line, sizeof(line), "%s=%06X\n", row.key, (*static_cast<const UInt32*>(at) ^ 0x0F0F0F) & 0xFFFFFF);
				break;
			case obvr::config::IniKind::AnchorIsWorld:
				std::snprintf(line, sizeof(line), "%s=%s\n", row.key, *static_cast<const bool*>(at) ? "head" : "world");
				break;
			case obvr::config::IniKind::MenusInWorld:
				std::snprintf(line, sizeof(line), "%s=%s\n", row.key, *static_cast<const bool*>(at) ? "cinema" : "world");
				break;
			case obvr::config::IniKind::LegacyWordBool:
				std::snprintf(line, sizeof(line), "%s=%s\n", row.key,
				              *static_cast<const bool*>(at) ? row.falseWord : row.trueWord);
				break;
			case obvr::config::IniKind::Text:
				std::snprintf(line, sizeof(line), "%s=flipped\n", row.key);
				break;
			case obvr::config::IniKind::Step: break;
		}
		bodies[s] += line;
	}
	std::string contents;
	for (UInt32 s = 0; s < sectionCount; ++s) {
		contents += "[" + sections[s] + "]\n" + bodies[s];
	}
	obvr::Config loaded;
	Check(LoadFrom("config-every-key.ini", contents.c_str(), loaded), "an INI with every key loads");

	// The members a step of the read puts back on purpose, with the reason.
	struct Kept {
		const char* section;
		const char* key;
		const char* why;
	};
	const Kept kept[] = {
		{"Look", "BlockVerticalLook", "SettleVerticalLookBlock: AimAtSource=1 keeps the block on"},
		{"Hands", "LaserBeam", "LaserNeedsOne: with the dot off too the beam comes back"},
		{"Hands", "ReachCloseMetres", "ReachCloseFollowsOpen: held to the open distance"},
		{"Hands", "ReachWalkAwayMetres", "ReachWalkAwayFollowsClose: held to the close distance"},
	};
	UInt32 unmoved = 0;
	for (UInt32 i = 0; i < count; ++i) {
		const obvr::config::IniKey& row = rows[i];
		if (row.kind == obvr::config::IniKind::Step) {
			continue;
		}
		if (std::memcmp(row.at(defaults), row.at(loaded), row.size) != 0) {
			continue;
		}
		bool explained = false;
		for (const Kept& k : kept) {
			explained = explained || (std::strcmp(k.section, row.section) == 0 && std::strcmp(k.key, row.key) == 0);
		}
		if (!explained) {
			++unmoved;
			std::printf("        %s.%s set in the INI left its member unchanged\n", row.section, row.key);
		}
	}
	Check(unmoved == 0, "every key set away from its default moves its member (or a named step puts it back)");
	for (const Kept& k : kept) {
		bool found = false;
		for (UInt32 i = 0; i < count; ++i) {
			found = found || (rows[i].kind != obvr::config::IniKind::Step && std::strcmp(rows[i].section, k.section) == 0 &&
			                  std::strcmp(rows[i].key, k.key) == 0);
		}
		Check(found, k.why);
	}
}

// OBVR_Controls.ini (vr/ButtonBindings.h): read beside OBVR.ini, each key
// that parses taken, a value that does not left at its default, an inline
// comment not part of the value; gone again when the file is.
void TestButtonBindingsFile() {
	std::printf("OBVR_Controls.ini\n");
	char path[512];
	if (!obvr::platform::BuildGamePath("OBVR_Controls.ini", path, sizeof(path))) {
		Check(false, "a path for OBVR_Controls.ini");
		return;
	}
	using obvr::vr::ControlAction;
	using obvr::vr::ControlSource;
	const auto at = [](ControlAction a) { return static_cast<UInt32>(a); };
	obvr::Config none;
	std::remove(path);
	LoadFrom("config-buttons.ini", "[Hands]\nKick=1\n", none);
	bool nothing = true;
	for (UInt32 i = 0; i < obvr::vr::kControlActionCount; ++i) {
		nothing = nothing && !none.hands.buttons.set[i];
	}
	Check(nothing, "no file: nothing bound, the defaults stand");

	std::FILE* file = std::fopen(path, "wb");
	Check(file != nullptr, "OBVR_Controls.ini written beside the test");
	if (file == nullptr) {
		return;
	}
	std::fputs("[Buttons]\nJump=RightB ; the B jumps\nKick = LeftA\nSneak=Bogus\nTogglePov=\n", file);
	std::fclose(file);
	obvr::Config bound;
	LoadFrom("config-buttons.ini", "[Hands]\nKick=1\n", bound);
	const obvr::vr::ControlBindingFile& f = bound.hands.buttons;
	Check(f.set[at(ControlAction::Jump)] && f.sources[at(ControlAction::Jump)][0] == ControlSource::RightB &&
	          f.sources[at(ControlAction::Jump)][1] == ControlSource::None,
	      "Jump=RightB, its comment cut off");
	Check(f.set[at(ControlAction::Kick)] && f.sources[at(ControlAction::Kick)][0] == ControlSource::LeftA,
	      "Kick = LeftA, spaces around the equals sign");
	Check(!f.set[at(ControlAction::Sneak)], "Sneak=Bogus: not taken, the default stays");
	Check(f.set[at(ControlAction::TogglePov)] && f.sources[at(ControlAction::TogglePov)][0] == ControlSource::None,
	      "TogglePov= empty: set, to nothing");
	Check(!f.set[at(ControlAction::Activate)], "a key not written: not set");
	const obvr::vr::ControlBindings resolved = obvr::vr::ResolveControlBindings(f, bound.hands.kickOnB, nullptr);
	Check(obvr::vr::ActionOfSource(resolved, ControlSource::RightB) == ControlAction::Jump &&
	          obvr::vr::ActionOfSource(resolved, ControlSource::LeftA) == ControlAction::Kick,
	      "resolved: the right B jumps, the left A kicks");

	std::remove(path);
	obvr::Config gone;
	LoadFrom("config-buttons.ini", "[Hands]\nKick=1\n", gone);
	Check(!gone.hands.buttons.set[at(ControlAction::Jump)], "the file taken away: nothing bound again");
}

// The shipped OBVR_Controls.ini: every key it shows is an action with a value
// that parses, all of them commented out (a written key would hold even with
// [Hands] Kick=0), and together they are the defaults - the file documents
// the layout as built.
void TestShippedButtonsFile() {
	std::printf("the shipped OBVR_Controls.ini\n");
	std::string path = OBVR_REPO_DIR;
	path += "/OBVR_Controls.ini";
	FILE* file = std::fopen(path.c_str(), "rb");
	Check(file != nullptr, "OBVR_Controls.ini read from the repository");
	if (file == nullptr) {
		return;
	}
	obvr::vr::ControlBindingFile shown;
	bool sectionSeen = false;
	bool active = false;
	bool parses = true;
	bool known = true;
	char line[512];
	while (std::fgets(line, sizeof(line), file) != nullptr) {
		std::string t = line;
		while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ' || t.back() == '\t')) {
			t.pop_back();
		}
		if (t == "[Buttons]") {
			sectionSeen = true;
			continue;
		}
		const bool commented = !t.empty() && t[0] == ';';
		const std::string body = commented ? t.substr(1) : t;
		const std::size_t eq = body.find('=');
		if (t.empty() || eq == std::string::npos || body.find(' ') < eq) {
			continue;  // prose, or a blank line
		}
		const std::string key = body.substr(0, eq);
		UInt32 action = obvr::vr::kControlActionCount;
		for (UInt32 i = 0; i < obvr::vr::kControlActionCount; ++i) {
			if (key == obvr::vr::ControlActionName(static_cast<obvr::vr::ControlAction>(i))) {
				action = i;
			}
		}
		if (action == obvr::vr::kControlActionCount) {
			known = false;
			std::printf("        %s is no action\n", key.c_str());
			continue;
		}
		active = active || !commented;
		shown.set[action] = obvr::vr::ParseControlSources(body.substr(eq + 1).c_str(), shown.sources[action]);
		parses = parses && shown.set[action];
	}
	std::fclose(file);
	bool every = true;
	for (UInt32 i = 0; i < obvr::vr::kControlActionCount; ++i) {
		every = every && shown.set[i];
	}
	Check(sectionSeen, "it has its [Buttons] section");
	Check(known && parses, "every key is an action, every value parses");
	Check(every, "every action is shown");
	Check(!active, "all of them commented out");
	Check(obvr::vr::ResolveControlBindings(shown, true, nullptr) == obvr::vr::DefaultControlBindings(true),
	      "together they are the defaults");
}

// The shipped OBVR.ini against the key table: every key the INI documents is
// one the code reads (a row, a step's own key, or the two read outside the
// table), every key the code reads outside [Debug] is documented, and no key
// is written twice in a section. [Debug] keys are the developer's switches:
// counted, not required.
void TestIniAudit() {
	std::printf("the shipped INI against the key table\n");
	std::string path = OBVR_REPO_DIR;
	path += "/OBVR.ini";
	FILE* file = std::fopen(path.c_str(), "rb");
	Check(file != nullptr, "OBVR.ini read from the repository");
	if (file == nullptr) {
		return;
	}
	// The INI's keys as "Section.Key".
	std::string iniKeys[1024];
	UInt32 iniCount = 0;
	UInt32 iniDuplicates = 0;
	std::string section;
	char line[1024];
	while (std::fgets(line, sizeof(line), file) != nullptr) {
		std::string t = line;
		while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ' || t.back() == '\t')) {
			t.pop_back();
		}
		if (t.empty() || t[0] == ';') {
			continue;
		}
		if (t[0] == '[') {
			const std::size_t close = t.find(']');
			section = close == std::string::npos ? t.substr(1) : t.substr(1, close - 1);
			continue;
		}
		const std::size_t eq = t.find('=');
		if (eq == std::string::npos) {
			continue;
		}
		std::string key = t.substr(0, eq);
		while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
			key.pop_back();
		}
		const std::string full = section + "." + key;
		bool seen = false;
		for (UInt32 i = 0; i < iniCount; ++i) {
			seen = seen || iniKeys[i] == full;
		}
		if (seen) {
			++iniDuplicates;
			std::printf("        %s is written twice in the INI\n", full.c_str());
		} else if (iniCount < 1024) {
			iniKeys[iniCount++] = full;
		}
	}
	std::fclose(file);
	Check(iniCount > 600, "the INI has its keys");
	Check(iniDuplicates == 0, "no key is written twice in a section of the INI");

	// The keys the code reads: the rows, the steps' own keys, and the two
	// read outside the table.
	std::string codeKeys[1024];
	UInt32 codeCount = 0;
	const auto add = [&](const std::string& full) {
		if (codeCount < 1024) {
			codeKeys[codeCount++] = full;
		}
	};
	add("Camera.HookEnabled");  // Config::Load, before the table
	add("Debug.HarnessToken");  // the test overlay's own stamp (core/HarnessToken.h)
	UInt32 count = 0;
	const obvr::config::IniKey* rows = obvr::config::ConfigKeys(count);
	for (UInt32 i = 0; i < count; ++i) {
		const obvr::config::IniKey& row = rows[i];
		if (row.kind != obvr::config::IniKind::Step) {
			add(std::string(row.section) + "." + row.key);
			continue;
		}
		std::string reads = row.reads;
		while (!reads.empty()) {
			const std::size_t space = reads.find(' ');
			const std::string word = reads.substr(0, space);
			reads = space == std::string::npos ? "" : reads.substr(space + 1);
			const std::size_t element = word.find("<Element>");
			if (element == std::string::npos) {
				add(std::string(row.section) + "." + word);
				continue;
			}
			for (UInt32 e = 0; e < obvr::vr::kHudElementCount; ++e) {
				std::string key = word;
				key.replace(element, 9, obvr::vr::kHudElementKeys[e]);
				add(std::string(row.section) + "." + key);
			}
		}
	}
	UInt32 unread = 0;
	for (UInt32 i = 0; i < iniCount; ++i) {
		bool read = false;
		for (UInt32 j = 0; j < codeCount; ++j) {
			read = read || codeKeys[j] == iniKeys[i];
		}
		if (!read) {
			++unread;
			std::printf("        %s is in the INI, and nothing reads it\n", iniKeys[i].c_str());
		}
	}
	Check(unread == 0, "every key the INI documents is read by the code");
	// Read but not documented: the legacy fallback stays out on purpose.
	UInt32 undocumented = 0;
	UInt32 debugOnly = 0;
	for (UInt32 j = 0; j < codeCount; ++j) {
		bool documented = false;
		for (UInt32 i = 0; i < iniCount; ++i) {
			documented = documented || iniKeys[i] == codeKeys[j];
		}
		if (documented || codeKeys[j] == "Look.VerticalLookRange" || codeKeys[j] == "Debug.HarnessToken") {
			continue;
		}
		if (codeKeys[j].compare(0, 6, "Debug.") == 0) {
			++debugOnly;
			continue;
		}
		++undocumented;
		std::printf("        %s is read by the code, and the INI does not document it\n", codeKeys[j].c_str());
	}
	Check(undocumented == 0, "every key the code reads outside [Debug] is documented in the INI");
	std::printf("        %u INI keys, %u keys read, %u [Debug] switches not in the INI\n", iniCount, codeCount, debugOnly);
}

int main() {
	// Unbuffered, so that a crash leaves the lines before it on the console.
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	TestHeadLook();
	TestWaterReflectionMapSizeSetting();
	TestStableLeafSetting();
	TestStableDistantTreesSetting();
	TestDualContentSumsSetting();
	TestLoadingStageSetting();
	TestSinglePassStereoSetting();
	TestWaterReflectionModeSetting();
	std::printf("OBVR config test\n\n");

	TestMissingFileKeepsDefaults();
	std::printf("\n");
	TestKeyCodeNotations();
	std::printf("\n");
	TestKeyCodeRejections();
	std::printf("\n");
	TestSourceParsing();
	std::printf("\n");
	TestAnglesAndFrames();
	std::printf("\n");
	TestLegacyWordBooleans();
	std::printf("\n");
	TestThirdPersonAimVisualPercent();
	std::printf("\n");
	TestLiveMenuBackground();
	std::printf("\n");
	TestPersistentCrosshairCache();
	TestCrosshairCutoutDefault();
	TestCrosshairTooltips();
	TestMirrorMenusToMonitor();
	TestMonitorMenusOnly();
	TestUnpausedMenus();
	TestHandTracking();
	TestNativeMenuLifecycleProbe();
	TestScriptTimeTrace();
	std::printf("\n");
	TestLookRanges();
	TestKeyTable();
	TestIniAudit();
	TestButtonBindingsFile();
	TestShippedButtonsFile();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
