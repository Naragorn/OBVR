// Checks what a bracket writes into a profiler capture: its owner's number in
// pass_index, no eye, and a name for every owner the capture can carry.

#include <cstdio>

#include "render/BracketOwner.h"

namespace {

using obvr::render::BracketContext;
using obvr::render::BracketOwner;
using obvr::render::BracketOwnerName;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Same(const char* a, const char* b) {
	while (*a != '\0' && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

void TestContext() {
	std::printf("Context\n");

	const obvr::perf::EventContext unnamed = BracketContext(BracketOwner::Unnamed);
	Check(unnamed.passIndex == 0, "an unnamed bracket writes 0, as every bracket did before");
	Check(unnamed.eye == 2, "a bracket is not an eye");
	Check(unnamed.presentId == 0 && unnamed.sceneId == 0 && unnamed.vrFrameId == 0,
	      "the frame fields are left for the profiler to fill");

	Check(BracketContext(BracketOwner::Poses).passIndex == 1, "poses is 1");
	Check(BracketContext(BracketOwner::GameFrame).passIndex == 2, "the game frame is 2");
	Check(BracketContext(BracketOwner::EyeMirror).passIndex == 3, "the eye mirror is 3");
	Check(BracketContext(BracketOwner::Hud).passIndex == 4, "the HUD is 4");
	Check(BracketContext(BracketOwner::Crosshair).passIndex == 5, "the crosshair is 5");
	Check(BracketContext(BracketOwner::HandHud).passIndex == 6, "the hand HUD is 6");
	Check(BracketContext(BracketOwner::Vignette).passIndex == 7, "the vignette is 7");
	Check(BracketContext(BracketOwner::Canvas).passIndex == 8, "a canvas is 8");
	Check(BracketContext(BracketOwner::SettingsMenu).passIndex == 9, "the settings menu is 9");
}

void TestNames() {
	std::printf("Names\n");

	Check(Same(BracketOwnerName(BracketOwner::Unnamed), "unnamed"), "unnamed");
	Check(Same(BracketOwnerName(BracketOwner::Poses), "poses"), "poses");
	Check(Same(BracketOwnerName(BracketOwner::GameFrame), "game_frame"), "game_frame");
	Check(Same(BracketOwnerName(BracketOwner::EyeMirror), "eye_mirror"), "eye_mirror");
	Check(Same(BracketOwnerName(BracketOwner::Hud), "hud"), "hud");
	Check(Same(BracketOwnerName(BracketOwner::Crosshair), "crosshair"), "crosshair");
	Check(Same(BracketOwnerName(BracketOwner::HandHud), "hand_hud"), "hand_hud");
	Check(Same(BracketOwnerName(BracketOwner::Vignette), "vignette"), "vignette");
	Check(Same(BracketOwnerName(BracketOwner::Canvas), "canvas"), "canvas");
	Check(Same(BracketOwnerName(BracketOwner::SettingsMenu), "settings_menu"), "settings_menu");
	Check(Same(BracketOwnerName(static_cast<BracketOwner>(99)), "?"),
	      "a number no owner has is a question mark, not a crash");
}

}  // namespace

int main() {
	TestContext();
	TestNames();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
