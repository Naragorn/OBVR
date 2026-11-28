// Checks the left arm's trace through a draw or a sheathe
// (game/ArmDrawTraceLogic.h): one window from the action's start to its end,
// the largest frame's turns and the twist bone's moves against the pinned
// hand, the pins' frames skipped.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "game/ArmDrawTraceLogic.h"
#include "game/SheathHoldLogic.h"

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
	s.action = inDraw ? kSheathActionEquip : -1;
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
	Check(done.action == kSheathActionEquip && !done.shield, "a draw, no shield");
	Check(!ArmShieldStill(done), "4 units a frame: the shield swung");
}

void TestShieldAndSheathe() {
	std::printf("A sheathe with a shield; a twist not found\n");
	ArmDrawTrace t;
	ArmDrawTrace done;
	ArmDrawSample s = Pin(1, true);
	s.action = kSheathActionUnequip;
	StepArmDrawTrace(t, s, done);
	s = Pin(2, true, 0.0f, 3.0f);
	s.action = kSheathActionUnequip;
	s.shield = true;
	s.twistValid = false;
	StepArmDrawTrace(t, s, done);
	StepArmDrawTrace(t, Pin(3, false), done);
	Check(done.action == kSheathActionUnequip && done.shield, "a sheathe, the shield seen in it");
	Check(done.mostStep == 0.0f && done.mostAway == 0.0f, "the twist unread: no move counted");
}

void TestOtherActions() {
	std::printf("Any action of the arm is a window; a change of action starts the next\n");
	ArmDrawTrace t;
	ArmDrawTrace done;
	ArmDrawSample s = Pin(1, true);
	s.action = 6;  // a block
	StepArmDrawTrace(t, s, done);
	s.pinFrame = 2;
	Check(!StepArmDrawTrace(t, s, done) && t.frames == 2, "the block goes on: one window");
	s.pinFrame = 3;
	s.action = 2;  // an attack right after it
	Check(StepArmDrawTrace(t, s, done) && done.action == 6 && done.frames == 2, "the attack ends the block's window");
	Check(t.running && t.action == 2 && t.frames == 1, "and starts its own");
	Check(std::strcmp(ArmActionName(kSheathActionEquip), "draw") == 0 &&
	          std::strcmp(ArmActionName(kSheathActionUnequip), "sheathe") == 0 &&
	          std::strcmp(ArmActionName(6), "block") == 0 && std::strcmp(ArmActionName(2), "attack") == 0 &&
	          std::strcmp(ArmActionName(8), "stagger") == 0 && std::strcmp(ArmActionName(42), "action") == 0,
	      "the actions named");
}

void TestHeldShield() {
	std::printf("The shield held to the hand: its own turn against the hand, the held pins, the verdict\n");
	ArmDrawTrace t;
	ArmDrawTrace done;
	ArmDrawSample s = Pin(1, true, 0.0f, 3.0f);
	s.shield = true;
	s.held = true;
	StepArmDrawTrace(t, s, done);
	s = Pin(2, true, 70.0f, 3.02f);  // the animation turns the hand's local 70 degrees, the shield stays
	s.shield = true;
	s.held = true;
	StepArmDrawTrace(t, s, done);
	s = Pin(3, true, 75.0f, 3.02f);
	s.shield = true;
	s.twistInHandRot = AboutZ(0.2f);
	StepArmDrawTrace(t, s, done);
	StepArmDrawTrace(t, Pin(4, false), done);
	Check(done.heldPins == 2 && done.frames == 3, "two of three pins held");
	Check(Near(done.mostTurn, 0.2f, 0.01f), "the twist's own turn against the hand: 0.2 degrees");
	Check(Near(done.mostHand, 70.0f), "the animation's turn of the hand still counted");
	Check(ArmShieldStill(done), "0.02 units and 0.2 degrees a frame: held still");
	ArmDrawTrace turned = done;
	turned.mostTurn = 2.0f;
	Check(!ArmShieldStill(turned), "2 degrees a frame: swung");
}

}  // namespace

int main() {
	TestIdle();
	TestWindow();
	TestShieldAndSheathe();
	TestOtherActions();
	TestHeldShield();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
