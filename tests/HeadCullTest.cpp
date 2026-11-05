// Checks the player's hidden head taken out of the world render
// (game/HeadCullLogic.h): when it is culled, that a hidden bit someone else
// set stays theirs, that a second cull in the same render keeps it ours, and
// that the release clears only the bit this code set.

#include <cstdio>

#include "game/HeadCullLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestWanted() {
	std::printf("When the head is culled\n");
	Check(HeadCullWanted(true, true, true), "collapsed, the switch on, the face under the body: culled");
	Check(!HeadCullWanted(false, true, true), "the head not collapsed this frame (HideHead off, no body): drawn");
	Check(!HeadCullWanted(true, false, true), "[Body] CullHiddenHead off: drawn, collapsed as before");
	Check(!HeadCullWanted(true, true, false), "a face node not under the body shown: left alone");
}

void TestCull() {
	std::printf("Laying the cull on\n");
	const HeadCullStep clear = CullHead(0x000E, false);
	Check(clear.flags == 0x000F && clear.ours, "a clear bit: set, and ours");
	const HeadCullStep theirs = CullHead(0x000F, false);
	Check(theirs.flags == 0x000F && !theirs.ours, "a bit already set by someone else: left, not ours");
	const HeadCullStep again = CullHead(0x000F, true);
	Check(again.flags == 0x000F && again.ours, "a second cull in the same render: still ours");
}

void TestRelease() {
	std::printf("Taking it off after the render\n");
	Check(ReleaseHead(0x000F, true) == 0x000E, "ours: cleared, the other flags kept");
	Check(ReleaseHead(0x000F, false) == 0x000F, "not ours: the bit stays");
	Check(ReleaseHead(0x000E, true) == 0x000E, "ours but already clear: unchanged");
	const HeadCullStep round = CullHead(0x0004, false);
	Check(ReleaseHead(round.flags, round.ours) == 0x0004, "cull then release: the flags as they were");
}

}  // namespace

int main() {
	TestWanted();
	TestCull();
	TestRelease();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
