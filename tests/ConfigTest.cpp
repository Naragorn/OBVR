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

#include "core/Config.h"
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
	Check(!untouched.handTracking, "the hand-tracked mode is off by default");

	obvr::Config configured;
	LoadFrom("ConfigTestHands.ini", "[Hands]\nEnabled=1\n", configured);
	Check(configured.handTracking, "Hands.Enabled=1 switches it on");

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

	Check(untouched.hands.forceFirstPerson && untouched.hands.hideArms,
	      "first person is forced and the arms hidden by default");
	Check(std::strcmp(untouched.hands.hideNodes, "Arms") == 0,
	      "and the shape to hide is Arms");
	obvr::Config body;
	LoadFrom("ConfigTestBody.ini",
	         "[Hands]\nForceFirstPerson=0\nHideArms=0\nHideFirstPersonNodes=UpperBody, Hand\n",
	         body);
	Check(!body.hands.forceFirstPerson && !body.hands.hideArms, "both switches are read");
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

	obvr::Config split;
	LoadFrom("ConfigTestSplitRange.ini",
	         "[Look]\nVerticalLookUpRange=40.0\nVerticalLookDownRange=140.0\n"
	         "[Head]\nHeadMovementScale=1.5\n"
	         "[Render]\nEnabled=1\nEyeSeparationScale=1.25\n",
	         split);
	Check(split.tracker.renderToHeadset, "and can be switched on from the [Render] section");
	CheckNear(split.tracker.eyeSeparationScale, 1.25f,
	          "EyeSeparationScale is read from the [Render] section");
	CheckNear(split.look.verticalLookUpRange, 40.0f, "the up range is read on its own");
	CheckNear(split.look.verticalLookDownRange, 140.0f, "the down range is read on its own");
	CheckNear(split.tracker.movementScale, 1.5f, "HeadMovementScale is read");
	CheckNear(split.tracker.EffectiveUnitsPerMetre(), 69.99125f * 1.5f,
	          "and multiplies into the effective units per metre");
}

}  // namespace

int main() {
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
	TestUnpausedMenus();
	TestHandTracking();
	std::printf("\n");
	TestLookRanges();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
