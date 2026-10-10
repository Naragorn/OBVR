// Checks the flat picture following the head (vr/FlatFollow.h).

#include <cstdio>

#include "vr/FlatFollow.h"

using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

// Steps `seconds` at 90 Hz with the head this far from the anchor; true when
// any step took a fresh anchor.
bool Hold(FlatFollowState& s, float degrees, float metres, float seconds, const FlatFollowSettings& settings) {
	bool took = false;
	const float dt = 1.0f / 90.0f;
	for (float t = 0.0f; t < seconds - 1e-4f; t += dt) {
		took = StepFlatFollow(s, degrees, metres, dt, settings) || took;
	}
	return took;
}

void TestFollow() {
	std::printf("The flat picture follows the head\n");
	const FlatFollowSettings settings;
	FlatFollowState s;
	Check(!Hold(s, 10.0f, 0.1f, 5.0f, settings), "looking a little aside for long: stays");
	Check(!Hold(s, 45.0f, 0.0f, 0.5f, settings), "turned away half a second: stays");
	Check(!Hold(s, 5.0f, 0.0f, 0.1f, settings), "and back: the time starts over");
	Check(!Hold(s, 45.0f, 0.0f, 0.9f, settings), "turned away 0.9 s: stays");
	Check(Hold(s, 45.0f, 0.0f, 0.2f, settings), "past a second: a fresh anchor");
	FlatFollowState walked;
	Check(Hold(walked, 0.0f, 0.8f, 1.1f, settings), "walked 80 cm off, facing the same way: follows too");
	FlatFollowSettings off = settings;
	off.followDegrees = 0.0f;
	FlatFollowState kept;
	Check(!Hold(kept, 170.0f, 3.0f, 10.0f, off), "followDegrees 0: never, as before");
	FlatFollowSettings noWalk = settings;
	noWalk.followMetres = 0.0f;
	FlatFollowState turned;
	Check(!Hold(turned, 0.0f, 3.0f, 3.0f, noWalk), "followMetres 0: moving alone does not take it along");
	FlatFollowState once;
	int anchors = 0;
	const float dt = 1.0f / 90.0f;
	for (int i = 0; i < 180; ++i) {
		anchors += StepFlatFollow(once, 45.0f, 0.0f, dt, settings) ? 1 : 0;
	}
	Check(anchors == 2, "two seconds turned away with nothing re-anchored meanwhile: once a second");
	Check(!StepFlatFollow(once, 45.0f, 0.0f, 0.0f, settings), "no time passing: nothing");

	std::printf("A laser on the picture\n");
	FlatFollowState pointed;
	bool took = false;
	for (int i = 0; i < 270; ++i) {
		took = StepFlatFollow(pointed, 60.0f, 1.0f, dt, settings, true) || took;
	}
	Check(!took, "turned far away for three seconds, pointing at it: never taken away from under the hand");
	Check(pointed.awaySeconds == 0.0f, "and the time away stands at nothing");
	Check(!Hold(pointed, 60.0f, 1.0f, 0.9f, settings), "the beam gone, still away: the second starts over");
	Check(Hold(pointed, 60.0f, 1.0f, 0.2f, settings), "and runs out: a fresh anchor");
}

}  // namespace

int main() {
	TestFollow();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
