// Checks the hold (vr/GripHold.h): a hand's grip as the fingers closed
// round the controller, read from the grip's pull with a hysteresis - closed
// at GripCloseAt, open again at GripOpenAt - and the click in its place
// where no pull reads.

#include <cmath>
#include <cstdio>

#include "vr/ControllerActions.h"
#include "vr/GripHold.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return std::fabs(a - b) < 0.0001f; }

GripHoldInput Pull(float pull, bool click = false) {
	GripHoldInput in;
	in.valid = true;
	in.pullKnown = true;
	in.pull = pull;
	in.click = click;
	return in;
}

void TestHysteresis() {
	std::printf("The hold by the pull, with its hysteresis\n");
	const GripHoldSettings s;
	Check(s.byPull && Near(s.closeAt, 0.8f) && Near(s.openAt, 0.4f), "the defaults: by the pull, closed at 0.8, open at 0.4 (measured 2026-10-23)");
	GripHoldState st;
	Check(!StepGripHold(st, Pull(0.0f), s), "a relaxed hand (0.0): open");
	Check(!StepGripHold(st, Pull(0.79f), s), "just under the close limit: still open");
	Check(StepGripHold(st, Pull(0.8f), s), "at the close limit: held");
	Check(StepGripHold(st, Pull(1.0f), s), "closed round the grip (1.0): held");
	Check(StepGripHold(st, Pull(0.5f), s), "down to 0.5: still held");
	Check(StepGripHold(st, Pull(0.41f), s), "just above the open limit: still held");
	Check(!StepGripHold(st, Pull(0.4f), s), "at the open limit: open");
	Check(!StepGripHold(st, Pull(0.79f), s), "back up to 0.79: stays open until the close limit");
	Check(StepGripHold(st, Pull(0.95f), s), "and closed again at 0.95");
	Check(!StepGripHold(st, Pull(0.0f), s), "let go: open");
}

void TestClickBesideThePull() {
	std::printf("The click where the pull reads\n");
	const GripHoldSettings s;
	GripHoldState st;
	Check(!StepGripHold(st, Pull(0.6f, true), s), "a click at a pull of 0.6: no hold - the pull decides (the harness's weak grip)");
	Check(StepGripHold(st, Pull(1.0f, false), s), "a full pull without the click: held");
}

void TestFallbacks() {
	std::printf("The click in the pull's place\n");
	const GripHoldSettings s;
	GripHoldState st;
	GripHoldInput legacy;
	legacy.valid = true;
	legacy.pullKnown = false;
	legacy.pull = 1.0f;
	legacy.click = false;
	Check(!StepGripHold(st, legacy, s), "no pull read, no click: open, whatever the pull field says");
	legacy.click = true;
	Check(StepGripHold(st, legacy, s), "no pull read, the click: held");
	legacy.click = false;
	Check(!StepGripHold(st, legacy, s), "the click let go: open at once, no hysteresis on a click");
	GripHoldSettings off;
	off.byPull = false;
	st = GripHoldState{};
	Check(!StepGripHold(st, Pull(1.0f, false), off), "the setting off: a full pull without the click is nothing");
	Check(StepGripHold(st, Pull(0.0f, true), off), "the setting off: the click holds");
	st = GripHoldState{};
	GripHoldInput nan = Pull(std::nanf(""), true);
	Check(StepGripHold(st, nan, s), "a pull that is not a number: the click decides");
	nan.click = false;
	Check(!StepGripHold(st, nan, s), "...both ways");
}

void TestUntracked() {
	std::printf("A hand that is not tracked\n");
	const GripHoldSettings s;
	GripHoldState st;
	StepGripHold(st, Pull(1.0f), s);
	GripHoldInput gone;
	gone.valid = false;
	gone.pullKnown = true;
	gone.pull = 1.0f;
	gone.click = true;
	Check(!StepGripHold(st, gone, s) && !st.held, "untracked: open, and the hold forgotten");
	Check(!StepGripHold(st, Pull(0.6f), s), "tracked again at 0.6: open, not the old hold");
}

void TestSettingsSanity() {
	std::printf("Settings that make no sense\n");
	GripHoldSettings s;
	s.closeAt = std::nanf("");
	Check(Near(GripCloseLimit(s), 0.8f), "a close limit that is not a number: the default 0.8");
	s.closeAt = 0.0f;
	Check(Near(GripCloseLimit(s), 0.8f), "a close limit of 0: the default");
	s.closeAt = 1.5f;
	Check(Near(GripCloseLimit(s), 0.8f), "a close limit above 1: the default");
	s.closeAt = 1.0f;
	Check(Near(GripCloseLimit(s), 1.0f), "a close limit of 1: taken");
	s.closeAt = 0.8f;
	s.openAt = 0.9f;
	Check(Near(GripOpenLimit(s), 0.6f), "an open limit above the close limit: 0.2 below the close limit");
	s.openAt = 0.8f;
	Check(Near(GripOpenLimit(s), 0.6f), "an open limit at the close limit: the same");
	s.openAt = -0.3f;
	Check(Near(GripOpenLimit(s), 0.0f), "a negative open limit: 0");
	s.openAt = std::nanf("");
	Check(Near(GripOpenLimit(s), 0.6f), "an open limit that is not a number: 0.2 below the close limit");
	s.closeAt = 0.1f;
	s.openAt = 0.05f;
	Check(Near(GripOpenLimit(s), 0.05f), "a low pair that keeps its order: taken as given");
	s.openAt = 0.1f;
	Check(Near(GripOpenLimit(s), 0.0f), "an open limit at a low close limit: never under 0");
	// The hold uses the limits as sanitised: close at 0.8 by default when the
	// setting is nonsense, open 0.2 under it.
	GripHoldSettings bad;
	bad.closeAt = 7.0f;
	bad.openAt = 9.0f;
	GripHoldState st;
	Check(!StepGripHold(st, Pull(0.7f), bad) && StepGripHold(st, Pull(0.8f), bad) && StepGripHold(st, Pull(0.61f), bad) &&
	          !StepGripHold(st, Pull(0.6f), bad),
	      "nonsense limits: 0.8 closes, 0.6 opens");
}

void TestPullKnown() {
	std::printf("Whether a hand's pull reads at all\n");
	HandPose hand;
	hand.valid = true;
	hand.actionInput = true;
	hand.actionActiveMask = 1u << input::GripPull;
	Check(GripPullKnown(hand), "the action path with the pull's action active: known");
	hand.actionActiveMask = 0x7F;
	Check(!GripPullKnown(hand), "the pull's action inactive (an old binding, a controller without it): unknown");
	hand.actionActiveMask = 0x1FF;
	hand.actionInput = false;
	Check(!GripPullKnown(hand), "legacy input: unknown");
	hand.actionInput = true;
	hand.valid = false;
	Check(!GripPullKnown(hand), "untracked: unknown");
}

}  // namespace

int main() {
	TestHysteresis();
	TestClickBesideThePull();
	TestFallbacks();
	TestUntracked();
	TestSettingsSanity();
	TestPullKnown();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
