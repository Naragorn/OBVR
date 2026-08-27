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
	TestLookRanges();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
