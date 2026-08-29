// Exercises the decision that guards the screen-size copy rewrite. The write
// itself targets an address in the game and stays with the game, like the
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
using obvr::render::UiSizeForFrame;
using obvr::render::UiSizeLockAction;

void TestUiSizeForFrame() {
	std::printf("When the 2D's window into the frame is shaped\n");

	// The cinema case this exists for: 16:9 into a nearly square frame takes
	// the full width and cuts the height.
	const auto cinema = UiSizeForFrame(4028, 3380, 1.7778f);
	Check(cinema.width == 4028, "a 16:9 window keeps the frame's width");
	Check(cinema.height == 2266, "and stands 2266 tall in a 3380 frame");

	// No aspect asked for: the whole frame.
	const auto whole = UiSizeForFrame(4028, 3380, 0.0f);
	Check(whole.width == 4028 && whole.height == 3380, "no aspect means the whole frame");

	// An aspect the frame already has changes nothing.
	const auto same = UiSizeForFrame(2560, 1440, 1.7778f);
	Check(same.width == 2560 && same.height == 1440,
	      "a frame already at the aspect is left whole");

	// An aspect narrower than the frame cuts the width instead - the window
	// is never larger than the frame in either axis.
	const auto narrow = UiSizeForFrame(4028, 3380, 1.0f);
	Check(narrow.width == 3380 && narrow.height == 3380,
	      "a square window into a wide frame keeps the height");

	// Nothing sensible can be shaped from a frame of no size.
	const auto empty = UiSizeForFrame(0, 0, 1.7778f);
	Check(empty.width == 0 && empty.height == 0, "an empty frame stays empty");
}

void TestDecision() {
	std::printf("When the screen-size copy is judged\n");

	// The measured situation this exists for: the game asked for 2560x1440,
	// the frame is 4028x3380, and the copy still reads what the game asked
	// for - which it must, having been copied from the same settings.
	Check(DecideUiSizeLock(true, 2560, 1440, 4028, 3380, 2560, 1440) ==
	          UiSizeLockAction::Lock,
	      "a copy reading the asked-for size is raised to the frame");

	// Switched off: nothing else matters.
	Check(DecideUiSizeLock(false, 2560, 1440, 4028, 3380, 2560, 1440) ==
	          UiSizeLockAction::NotWanted,
	      "the switch off means hands off");

	// No resize happened: asked and created agree, and there is no split.
	Check(DecideUiSizeLock(true, 4028, 3380, 4028, 3380, 4028, 3380) ==
	          UiSizeLockAction::NothingToDo,
	      "an agreeing frame needs no raise");

	// The copy does not read as the asked-for size - it is not the copy this
	// was built against, in either axis.
	Check(DecideUiSizeLock(true, 2560, 1440, 4028, 3380, 1920, 1440) ==
	          UiSizeLockAction::WrongValues,
	      "a width that is not the asked-for one refuses");
	Check(DecideUiSizeLock(true, 2560, 1440, 4028, 3380, 2560, 1080) ==
	          UiSizeLockAction::WrongValues,
	      "and a height that is not the asked-for one refuses too");
}

}  // namespace

int main() {
	std::printf("OBVR ui screen size test\n\n");

	TestUiSizeForFrame();
	std::printf("\n");
	TestDecision();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
