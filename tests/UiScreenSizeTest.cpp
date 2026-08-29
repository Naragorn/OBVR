// Exercises the decision that guards the renderer screen-size rewrite. The
// write itself needs the game's renderer and stays with the game, like the
// probe's readback; every way the decision can refuse is covered here,
// because refusing correctly is what makes the write safe to attempt.

#include <cstdio>

#include "render/UiScreenSize.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s: %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::render::DecideUiSizeLock;
using obvr::render::UiSizeLockAction;

void TestDecision() {
	std::printf("When the renderer's screen size is judged\n");

	// The measured situation this exists for: the game believes 2560x1440,
	// the frame is 4028x3380, and the renderer's pair reads the frame.
	Check(DecideUiSizeLock(true, 2560, 1440, 4028, 3380, 4028, 3380) ==
	          UiSizeLockAction::Lock,
	      "a pair reading the frame's size is locked to the belief");

	// Switched off: nothing else matters.
	Check(DecideUiSizeLock(false, 2560, 1440, 4028, 3380, 4028, 3380) ==
	          UiSizeLockAction::NotWanted,
	      "the switch off means hands off");

	// No resize happened: belief and frame agree, and there is no split.
	Check(DecideUiSizeLock(true, 4028, 3380, 4028, 3380, 4028, 3380) ==
	          UiSizeLockAction::NothingToDo,
	      "an agreeing belief needs no lock");

	// The pair does not read as the frame - the offset does not mean what it
	// is believed to mean, in either axis.
	Check(DecideUiSizeLock(true, 2560, 1440, 4028, 3380, 2560, 3380) ==
	          UiSizeLockAction::WrongValues,
	      "a width that is not the frame's refuses");
	Check(DecideUiSizeLock(true, 2560, 1440, 4028, 3380, 4028, 1440) ==
	          UiSizeLockAction::WrongValues,
	      "and a height that is not the frame's refuses too");
}

}  // namespace

int main() {
	std::printf("OBVR ui screen size test\n\n");

	TestDecision();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
