// Checks the decision that stops the engine's head tracking for the player
// while the body is shown in first person (game/PlayerHeadTrackLogic.h), and
// that the byte is given back exactly when OBVR set it.

#include <cstdio>

#include "game/PlayerHeadTrackLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::game::HeadTrackAction;
using obvr::game::HeadTrackState;
using obvr::game::StepPlayerHeadTrack;

void TestFreezeWhileShown() {
	std::printf("Stopped while the body is shown, given back after\n");
	HeadTrackState s;
	Check(StepPlayerHeadTrack(s, false, true, true) == HeadTrackAction::Freeze,
	      "head tracking off, body shown, object there: freeze");
	Check(s.frozen, "noted as OBVR's");
	Check(StepPlayerHeadTrack(s, false, true, true) == HeadTrackAction::Freeze,
	      "kept every frame (the engine may make a new object)");
	Check(StepPlayerHeadTrack(s, false, false, true) == HeadTrackAction::Release,
	      "the body no longer shown (third person, a menu): released");
	Check(!s.frozen, "and forgotten");
	Check(StepPlayerHeadTrack(s, false, false, true) == HeadTrackAction::None, "released once only");
}

void TestWantedOn() {
	std::printf("The INI wants the engine's head tracking\n");
	HeadTrackState s;
	Check(StepPlayerHeadTrack(s, true, true, true) == HeadTrackAction::None, "never touched");
	StepPlayerHeadTrack(s, false, true, true);
	Check(StepPlayerHeadTrack(s, true, true, true) == HeadTrackAction::Release, "switched on while frozen: released");
}

void TestNoObject() {
	std::printf("No head-tracking object\n");
	HeadTrackState s;
	Check(StepPlayerHeadTrack(s, false, true, false) == HeadTrackAction::None, "nothing to write to");
	StepPlayerHeadTrack(s, false, true, true);
	Check(StepPlayerHeadTrack(s, false, true, false) == HeadTrackAction::None,
	      "the object gone (a load): nothing to give back to");
	Check(!s.frozen, "forgotten with it");
	Check(StepPlayerHeadTrack(s, false, false, true) == HeadTrackAction::None,
	      "a new object afterwards is the engine's own: not released by OBVR");
}

}  // namespace

int main() {
	std::printf("OBVR player head tracking test\n\n");
	TestFreezeWhileShown();
	TestWantedOn();
	TestNoObject();
	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}
	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
