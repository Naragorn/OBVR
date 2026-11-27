// Checks the left arm's trace through a draw or a sheathe
// (game/ArmDrawTraceLogic.h): one window from the action's start to its end,
// the largest frame's turns and the twist bone's moves against the pinned
// hand, the pins' frames skipped.

#include <cmath>
#include <cstdio>

#include "game/ArmDrawTraceLogic.h"

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

bool Near(float a, float b, float tolerance = 0.05f) { return std::fabs(a - b) <= tolerance; }

NiMatrix33 AboutZ(float degrees) { return AxisAngleRotation(0.0f, 0.0f, 1.0f, degrees * (math::kPi / 180.0f)); }

ArmDrawSample Pin(UInt32 frame, bool inDraw, float handDegrees = 0.0f, float twistY = 0.0f) {
	ArmDrawSample s;
	s.inDraw = inDraw;
	s.pinFrame = frame;
	s.handLocal = AboutZ(handDegrees);
	s.twistValid = true;
	s.twistInHand = NiPoint3{0.0f, twistY, 0.0f};
	return s;
}

void TestIdle() {
	std::printf("Outside a draw: nothing\n");
	ArmDrawTrace t;
	ArmDrawTrace done;
	Check(!StepArmDrawTrace(t, Pin(1, false), done) && !t.running, "no draw: no window, no line");
}

void TestWindow() {
	std::printf("A draw: one window, its largest frame\n");
	ArmDrawTrace t;
	ArmDrawTrace done;
	Check(!StepArmDrawTrace(t, Pin(10, true, 0.0f, 0.0f), done) && t.running, "the draw begins: a window");
	StepArmDrawTrace(t, Pin(11, true, 10.0f, 1.0f), done);
	StepArmDrawTrace(t, Pin(12, true, 40.0f, 5.0f), done);
	StepArmDrawTrace(t, Pin(14, true, 45.0f, 5.5f), done);
	Check(StepArmDrawTrace(t, Pin(15, false), done) && !t.running, "the draw ends: one line, the window closed");
	Check(done.frames == 4, "four pins in it");
	Check(done.skipped == 1, "one frame without a left pin (13)");
	Check(Near(done.mostHand, 30.0f), "the hand's local: 30 degrees in its largest frame");
	Check(Near(done.mostStep, 4.0f) && done.mostStepAt == 3, "the twist: 4 units in the third pin");
	Check(Near(done.mostAway, 5.5f), "5.5 units from where it began");
	Check(!done.sheathe && !done.shield, "a draw, no shield");
}

void TestShieldAndSheathe() {
	std::printf("A sheathe with a shield; a twist not found\n");
	ArmDrawTrace t;
	ArmDrawTrace done;
	ArmDrawSample s = Pin(1, true);
	s.sheathe = true;
	StepArmDrawTrace(t, s, done);
	s = Pin(2, true, 0.0f, 3.0f);
	s.shield = true;
	s.twistValid = false;
	StepArmDrawTrace(t, s, done);
	StepArmDrawTrace(t, Pin(3, false), done);
	Check(done.sheathe && done.shield, "a sheathe, the shield seen in it");
	Check(done.mostStep == 0.0f && done.mostAway == 0.0f, "the twist unread: no move counted");
}

}  // namespace

int main() {
	TestIdle();
	TestWindow();
	TestShieldAndSheathe();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
